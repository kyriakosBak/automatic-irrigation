#include "modules/pca9685_control.h"
#include "modules/pump_control.h"
#include "modules/valve_control.h"
#include "modules/scheduler.h"
#include "modules/sensors.h"
#if defined(ARDUINO_ARCH_ESP8266)
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#define SPIFFS LittleFS
#include <LittleFS.h>
FS& filesystem = LittleFS;
#else
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
fs::FS &filesystem = SPIFFS;
#endif
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config/config.h"
#include <time.h>

// Function declarations
void setup_routes();

// Dosing settings (ml per fertilizer)
float dosing_ml[NUM_FERTILIZERS] = {10, 10, 10, 10};
// Schedule: hour and minute for daily run
int schedule_hour = 8;
int schedule_minute = 0;

float pump_calibration[NUM_PUMPS] = {1, 1, 1, 1, 1, 1}; // ml/sec for each pump

AsyncWebServer server(80);
String wifi_ssid = "";
String wifi_password = "";

static unsigned long fill_start_time = 0;

void load_settings() {
    File f = filesystem.open("/settings.json", "r");
    if (!f) {
        Serial.println("No settings file, using defaults");
        return;
    }
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.println("Failed to parse settings.json, using defaults");
        return;
    }
    for (int i = 0; i < NUM_FERTILIZERS; i++) {
        if (doc["dosing"][i].is<float>())
            dosing_ml[i] = doc["dosing"][i].as<float>();
    }
    if (doc["schedule"]["hour"].is<int>())
        schedule_hour = doc["schedule"]["hour"].as<int>();
    if (doc["schedule"]["minute"].is<int>())
        schedule_minute = doc["schedule"]["minute"].as<int>();
    for (int i = 0; i < NUM_PUMPS; i++) pump_calibration[i] = doc["calibration"][i].as<float>();
    Serial.println("Settings loaded from SPIFFS");
}

void save_settings() {
    StaticJsonDocument<512> doc;
    for (int i = 0; i < NUM_FERTILIZERS; i++)
        doc["dosing"][i] = dosing_ml[i];
    doc["schedule"]["hour"] = schedule_hour;
    doc["schedule"]["minute"] = schedule_minute;
    doc["calibration"] = JsonArray();
    for (int i = 0; i < NUM_PUMPS; i++) doc["calibration"].add(pump_calibration[i]);
    File f = filesystem.open("/settings.json", "w");
    if (!f) {
        Serial.println("Failed to open settings.json for writing");
        return;
    }
    serializeJson(doc, f);
    f.close();
    Serial.println("Settings saved to SPIFFS");
}

bool load_wifi_credentials() {
    File f = filesystem.open("/wifi.json", "r");
    if (!f) return false;
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;
    wifi_ssid = doc["ssid"].as<String>();
    wifi_password = doc["password"].as<String>();
    return wifi_ssid.length() > 0;
}

// Forward declaration for scheduler
void trigger_dosing();

void start_ap_mode() {
    WiFi.softAP("IrrigationSetup");
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<form method='POST'><label>SSID: <input name='ssid'></label><br><label>Password: <input name='password' type='password'></label><br><button type='submit'>Save</button></form>";
        request->send(200, "text/html", html);
    });
    server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String pass = request->getParam("password", true)->value();
            StaticJsonDocument<128> doc;
            doc["ssid"] = ssid;
            doc["password"] = pass;
            File f = SPIFFS.open("/wifi.json", "w");
            serializeJson(doc, f);
            f.close();
            request->send(200, "text/html", "Saved. Rebooting...");
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "text/plain", "Missing SSID or password");
        }
    });
    server.begin();
}

// Status variables
static bool filling = false;
extern bool aux_pump_active;
static bool ntp_synced = false;

// Watering sequence state machine
enum WateringState {
    IDLE,
    DOSING,
    WAIT_FOR_DOSING,
    FILLING,
    WAIT_FOR_FILL
};
static WateringState watering_state = IDLE;

void start_watering_sequence() {
    if (watering_state == IDLE) {
        trigger_dosing();
        watering_state = DOSING;
    }
}

void setup_routes() {
    // REST API: Trigger watering sequence
    server.on("/api/start_watering", HTTP_POST, [](AsyncWebServerRequest *request){
        if (watering_state != IDLE) {
            request->send(409, "text/plain", "Sequence already running");
            return;
        }
        start_watering_sequence();
        request->send(200, "text/plain", "Watering sequence started");
    });

    // REST API: Get dosing
    server.on("/api/dosing", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "[";
        for (int i = 0; i < NUM_FERTILIZERS; i++) {
            json += String(dosing_ml[i]);
            if (i < NUM_FERTILIZERS-1) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set dosing
    server.on("/api/dosing", HTTP_POST, [](AsyncWebServerRequest *request){
        for (int i = 0; i < NUM_FERTILIZERS; i++) {
            if (request->hasParam(String("ml")+i, true)) {
                dosing_ml[i] = request->getParam(String("ml")+i, true)->value().toFloat();
            }
        }
        save_settings();
        request->send(200, "text/plain", "OK");
    });
    
    // REST API: Get schedule
    server.on("/api/schedule", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{";
        json += "\"hour\":" + String(schedule_hour) + ",";
        json += "\"minute\":" + String(schedule_minute) + "}";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set schedule
    server.on("/api/schedule", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("hour", true)) schedule_hour = request->getParam("hour", true)->value().toInt();
        if (request->hasParam("minute", true)) schedule_minute = request->getParam("minute", true)->value().toInt();
        save_settings();
        request->send(200, "text/plain", "OK");
    });
    
    // REST API: Fill main tank
    server.on("/api/fill_main_tank", HTTP_POST, [](AsyncWebServerRequest *request){
        valve_control_fill_main_tank();
        filling = true;
        request->send(200, "text/plain", "Filling main tank");
    });
    
    // REST API: Stop main tank
    server.on("/api/stop_main_tank", HTTP_POST, [](AsyncWebServerRequest *request){
        valve_control_stop_main_tank();
        filling = false;
        request->send(200, "text/plain", "Stopped main tank");
    });
    
    // REST API: Run auxiliary pump
    server.on("/api/run_aux_pump", HTTP_POST, [](AsyncWebServerRequest *request){
        unsigned long ms = 5000;
        if (request->hasParam("ms", true)) ms = request->getParam("ms", true)->value().toInt();
        pump_control_run_aux_pump(ms);
        request->send(200, "text/plain", "Aux pump running");
    });
    
    // REST API: Stop auxiliary pump
    server.on("/api/stop_aux_pump", HTTP_POST, [](AsyncWebServerRequest *request){
        pump_control_stop_aux_pump();
        request->send(200, "text/plain", "Aux pump stopped");
    });
    
    // REST API: Get calibration
    server.on("/api/calibration", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "[";
        for (int i = 0; i < NUM_PUMPS; i++) {
            json += String(pump_calibration[i]);
            if (i < NUM_PUMPS-1) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set calibration
    server.on("/api/calibration", HTTP_POST, [](AsyncWebServerRequest *request){
        for (int i = 0; i < NUM_PUMPS; i++) {
            if (request->hasParam(String("cal")+i, true)) {
                pump_calibration[i] = request->getParam(String("cal")+i, true)->value().toFloat();
            }
        }
        save_settings();
        request->send(200, "text/plain", "Calibration saved");
    });
    
    // REST API: Get status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<256> doc;
        doc["tank_full"] = sensors_get_liquid_level();
        doc["filling"] = filling;
        doc["aux_pump"] = aux_pump_active;
        time_t now = time(nullptr);
        struct tm timeinfo;
        if (localtime_r(&now, &timeinfo)) {
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
            doc["time"] = buf;
        } else {
            doc["time"] = "N/A";
        }
        doc["ntp_synced"] = ntp_synced;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Serve web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
}

void sync_ntp() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Waiting for NTP sync...");
    time_t now = 0;
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 30) {
        delay(500);
        now = time(nullptr);
        retries++;
    }
    if (now < 8 * 3600 * 2) {
        Serial.println("NTP sync failed");
        ntp_synced = false;
    } else {
        Serial.println("NTP sync successful");
        ntp_synced = true;
    }
}

void setup() {
    Serial.begin(115200);
    motor_shield_init();
    pump_control_init();
    valve_control_init();
    scheduler_init();
    sensors_init();

#if defined(ARDUINO_ARCH_ESP8266)
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
    }
#else
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
    }
#endif
    load_settings();

    bool wifi_ok = false;
    if (load_wifi_credentials()) {
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(500);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected");
            Serial.println(WiFi.localIP());
            wifi_ok = true;
        } else {
            Serial.println("\nWiFi connect failed");
        }
    } else {
        Serial.println("No WiFi credentials found in SPIFFS");
    }
    if (!wifi_ok) {
        start_ap_mode();
        return;
    }

    sync_ntp();

    // Set timezone for Amsterdam (CET/CEST)
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    setup_routes();
    server.begin();
}

void loop() {
    scheduler_run();
    pump_control_run();
    valve_control_run();
    sensors_read();

    switch (watering_state) {
        case IDLE:
            break;
        case DOSING:
            if (pump_control_is_dosing()) {
                watering_state = WAIT_FOR_DOSING;
            } else {
                watering_state = FILLING;
            }
            break;
        case WAIT_FOR_DOSING:
            if (!pump_control_is_dosing()) {
                valve_control_fill_main_tank();
                filling = true;
                fill_start_time = millis();
                watering_state = FILLING;
            }
            break;
        case FILLING:
            if (filling) {
                if (sensors_get_liquid_level()) {
                    valve_control_stop_main_tank();
                    filling = false;
                    watering_state = WAIT_FOR_FILL;
                } else if (millis() - fill_start_time > MAIN_TANK_FILL_TIMEOUT_MS) {
                    valve_control_stop_main_tank();
                    filling = false;
                    watering_state = WAIT_FOR_FILL;
                    Serial.println("[SAFETY] Main tank fill timeout reached, valve closed.");
                }
            } else {
                watering_state = WAIT_FOR_FILL;
            }
            break;
        case WAIT_FOR_FILL:
            watering_state = IDLE;
            break;
    }

    if (filling && sensors_get_liquid_level()) {
        valve_control_stop_main_tank();
        filling = false;
    }
    delay(100);
}