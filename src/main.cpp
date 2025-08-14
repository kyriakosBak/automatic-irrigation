#include "modules/motor_shield_control.h"
#include "modules/pump_control.h"
#include "modules/valve_control.h"
#include "modules/scheduler.h"
#include "modules/sensors.h"
#include <WiFi.h>
#include <ESPmDNS.h>


#include <LittleFS.h>
fs::FS &filesystem = LittleFS;
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config/config.h"
#include <time.h>

// Function declarations
void setup_routes();

// Dosing settings (ml per fertilizer) - now per day of week
float weekly_dosing_ml[7][NUM_FERTILIZERS]; // [day_of_week][fertilizer_index]
bool weekly_watering_enabled[7]; // [day_of_week] - true if watering is enabled for that day
// Schedule: hour and minute for daily run
int schedule_hour = 8;
int schedule_minute = 0;

float pump_calibration[NUM_FERTILIZERS] = {1, 1, 1, 1, 1}; // ml/sec for fertilizer pumps only
int fertilizer_motor_speed = 50; // Default motor speed for fertilizer pumps

AsyncWebServer server(80);
String wifi_ssid = "";
String wifi_password = "";

static unsigned long fill_start_time = 0;

void init_weekly_dosing() {
    // Initialize with default values (10ml for each fertilizer, every day enabled)
    for (int day = 0; day < 7; day++) {
        weekly_watering_enabled[day] = true; // Enable watering for all days by default
        for (int fert = 0; fert < NUM_FERTILIZERS; fert++) {
            weekly_dosing_ml[day][fert] = 10.0;
        }
    }
}

void load_settings() {
    File f = filesystem.open("/settings.json", "r");
    if (!f) {
        Serial.println("No settings file, using defaults");
        init_weekly_dosing();
        return;
    }
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.println("Failed to parse settings.json, using defaults");
        init_weekly_dosing();
        return;
    }
    
    // Load weekly dosing schedule
    if (doc["weekly_dosing"].is<JsonArray>()) {
        JsonArray weekly = doc["weekly_dosing"];
        for (int day = 0; day < 7 && day < weekly.size(); day++) {
            if (weekly[day].is<JsonArray>()) {
                JsonArray day_dosing = weekly[day];
                for (int fert = 0; fert < NUM_FERTILIZERS && fert < day_dosing.size(); fert++) {
                    if (day_dosing[fert].is<float>()) {
                        weekly_dosing_ml[day][fert] = day_dosing[fert].as<float>();
                    }
                }
            }
        }
    } else {
        // Fallback: try to load old single dosing format
        init_weekly_dosing();
        for (int i = 0; i < NUM_FERTILIZERS; i++) {
            if (doc["dosing"][i].is<float>()) {
                float old_value = doc["dosing"][i].as<float>();
                // Apply the old value to all days
                for (int day = 0; day < 7; day++) {
                    weekly_dosing_ml[day][i] = old_value;
                }
            }
        }
    }
    
    // Load weekly watering enabled settings
    if (doc["weekly_watering_enabled"].is<JsonArray>()) {
        JsonArray weekly_enabled = doc["weekly_watering_enabled"];
        for (int day = 0; day < 7 && day < weekly_enabled.size(); day++) {
            if (weekly_enabled[day].is<bool>()) {
                weekly_watering_enabled[day] = weekly_enabled[day].as<bool>();
            }
        }
    }
    
    if (doc["schedule"]["hour"].is<int>())
        schedule_hour = doc["schedule"]["hour"].as<int>();
    if (doc["schedule"]["minute"].is<int>())
        schedule_minute = doc["schedule"]["minute"].as<int>();
    for (int i = 0; i < NUM_FERTILIZERS; i++) pump_calibration[i] = doc["calibration"][i].as<float>();
    if (doc["fertilizer_motor_speed"].is<int>())
        fertilizer_motor_speed = doc["fertilizer_motor_speed"].as<int>();
    Serial.println("Settings loaded from LittleFS");
}

void save_settings() {
    StaticJsonDocument<1024> doc;
    
    // Save weekly dosing schedule
    JsonArray weekly = doc.createNestedArray("weekly_dosing");
    for (int day = 0; day < 7; day++) {
        JsonArray day_dosing = weekly.createNestedArray();
        for (int fert = 0; fert < NUM_FERTILIZERS; fert++) {
            day_dosing.add(weekly_dosing_ml[day][fert]);
        }
    }
    
    // Save weekly watering enabled settings
    JsonArray weekly_enabled = doc.createNestedArray("weekly_watering_enabled");
    for (int day = 0; day < 7; day++) {
        weekly_enabled.add(weekly_watering_enabled[day]);
    }
    
    doc["schedule"]["hour"] = schedule_hour;
    doc["schedule"]["minute"] = schedule_minute;
    doc["calibration"] = JsonArray();
    for (int i = 0; i < NUM_FERTILIZERS; i++) doc["calibration"].add(pump_calibration[i]);
    doc["fertilizer_motor_speed"] = fertilizer_motor_speed;
    File f = filesystem.open("/settings.json", "w");
    if (!f) {
        Serial.println("Failed to open settings.json for writing");
        return;
    }
    serializeJson(doc, f);
    f.close();
    Serial.println("Settings saved to LittleFS");
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
            File f = filesystem.open("/wifi.json", "w");
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

    // REST API: Get weekly dosing
    server.on("/api/weekly_dosing", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "[";
        for (int day = 0; day < 7; day++) {
            json += "[";
            for (int fert = 0; fert < NUM_FERTILIZERS; fert++) {
                json += String(weekly_dosing_ml[day][fert]);
                if (fert < NUM_FERTILIZERS-1) json += ",";
            }
            json += "]";
            if (day < 6) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set weekly dosing
    server.on("/api/weekly_dosing", HTTP_POST, [](AsyncWebServerRequest *request){
        for (int day = 0; day < 7; day++) {
            for (int fert = 0; fert < NUM_FERTILIZERS; fert++) {
                String param_name = "day" + String(day) + "_fert" + String(fert);
                if (request->hasParam(param_name, true)) {
                    weekly_dosing_ml[day][fert] = request->getParam(param_name, true)->value().toFloat();
                }
            }
        }
        save_settings();
        request->send(200, "text/plain", "Weekly dosing saved");
    });
    
    // REST API: Get weekly watering enabled
    server.on("/api/weekly_watering_enabled", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "[";
        for (int day = 0; day < 7; day++) {
            json += weekly_watering_enabled[day] ? "true" : "false";
            if (day < 6) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set weekly watering enabled
    server.on("/api/weekly_watering_enabled", HTTP_POST, [](AsyncWebServerRequest *request){
        for (int day = 0; day < 7; day++) {
            String param_name = "day" + String(day) + "_enabled";
            if (request->hasParam(param_name, true)) {
                String value = request->getParam(param_name, true)->value();
                weekly_watering_enabled[day] = (value == "true" || value == "1");
            }
        }
        save_settings();
        request->send(200, "text/plain", "Weekly watering schedule saved");
    });
    
    // REST API: Get dosing (legacy - returns current day)
    server.on("/api/dosing", HTTP_GET, [](AsyncWebServerRequest *request){
        time_t now = time(nullptr);
        struct tm timeinfo;
        int day = 0;
        if (localtime_r(&now, &timeinfo)) {
            day = timeinfo.tm_wday;
        }
        String json = "[";
        for (int i = 0; i < NUM_FERTILIZERS; i++) {
            json += String(weekly_dosing_ml[day][i]);
            if (i < NUM_FERTILIZERS-1) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set dosing (legacy - sets for all days)
    server.on("/api/dosing", HTTP_POST, [](AsyncWebServerRequest *request){
        for (int i = 0; i < NUM_FERTILIZERS; i++) {
            if (request->hasParam(String("ml")+i, true)) {
                float value = request->getParam(String("ml")+i, true)->value().toFloat();
                // Set for all days of the week
                for (int day = 0; day < 7; day++) {
                    weekly_dosing_ml[day][i] = value;
                }
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
    
    // REST API: Get calibration (fertilizer pumps only)
    server.on("/api/calibration", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "[";
        for (int i = 0; i < NUM_FERTILIZERS; i++) {
            json += String(pump_calibration[i]);
            if (i < NUM_FERTILIZERS-1) json += ",";
        }
        json += "]";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set calibration (fertilizer pumps only)
    server.on("/api/calibration", HTTP_POST, [](AsyncWebServerRequest *request){
        for (int i = 0; i < NUM_FERTILIZERS; i++) {
            if (request->hasParam(String("cal")+i, true)) {
                pump_calibration[i] = request->getParam(String("cal")+i, true)->value().toFloat();
            }
        }
        save_settings();
        request->send(200, "text/plain", "Calibration saved");
    });
    
    // REST API: Get fertilizer motor speed
    server.on("/api/fertilizer_motor_speed", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"fertilizer_motor_speed\":" + String(fertilizer_motor_speed) + "}";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set fertilizer motor speed
    server.on("/api/fertilizer_motor_speed", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("fertilizer_motor_speed", true)) {
            fertilizer_motor_speed = request->getParam("fertilizer_motor_speed", true)->value().toInt();
            // Clamp the value to a reasonable range
            if (fertilizer_motor_speed < 1) fertilizer_motor_speed = 1;
            if (fertilizer_motor_speed > 255) fertilizer_motor_speed = 255;
        }
        save_settings();
        request->send(200, "text/plain", "Fertilizer motor speed saved");
    });
    
    // REST API: Get status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<256> doc;
        doc["tank_full"] = sensors_get_liquid_level();
        doc["filling"] = filling;
        doc["aux_pump"] = aux_pump_active;
        
        // Add watering enabled status for today
        time_t now = time(nullptr);
        struct tm timeinfo;
        int current_day = 0;
        if (localtime_r(&now, &timeinfo)) {
            current_day = timeinfo.tm_wday;
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
            doc["time"] = buf;
        } else {
            doc["time"] = "N/A";
        }
        doc["watering_today"] = weekly_watering_enabled[current_day];
        doc["ntp_synced"] = ntp_synced;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Serve web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(filesystem, "/index.html", "text/html");
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

    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
    }
    init_weekly_dosing(); // Initialize with defaults first
    load_settings();       // Then load from file if available

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
        Serial.println("No WiFi credentials found in LittleFS, starting AP mode");
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