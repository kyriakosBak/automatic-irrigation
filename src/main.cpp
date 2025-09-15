#include "modules/motor_shield_control.h"
#include "modules/pump_control.h"
#include "modules/valve_control.h"
#include "modules/scheduler.h"
#include "modules/sensors.h"
#include "modules/logger.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

#include <LittleFS.h>
fs::FS &filesystem = LittleFS;
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config/config.h"
#include <time.h>

Preferences preferences;

// Function declarations
void setup_routes();
void start_watering_sequence();
void save_settings();

// Dosing settings (ml per fertilizer) - now per day of week
float weekly_dosing_ml[7][NUM_FERTILIZERS]; // [day_of_week][fertilizer_index]
bool weekly_watering_enabled[7]; // [day_of_week] - true if watering is enabled for that day
// Schedule: hour and minute for daily run
int schedule_hour = 8;
int schedule_minute = 0;

float pump_calibration[NUM_FERTILIZERS] = {1, 1, 1, 1, 1}; // ml/sec for fertilizer pumps only
int fertilizer_motor_speed = 200; // Default motor speed for fertilizer pumps
unsigned long watering_duration_ms = MAX_WATERING_TIME_MS; // Configurable watering duration

AsyncWebServer server(80);
String wifi_ssid = "";
String wifi_password = "";

static unsigned long fill_start_time = 0;

void init_weekly_dosing() {
    // Initialize with default values (10ml for each fertilizer, every day enabled)
    for (int day = 0; day < 7; day++) {
        weekly_watering_enabled[day] = true; // Enable watering for all days by default
        for (int fert = 0; fert < NUM_FERTILIZERS; fert++) {
            weekly_dosing_ml[day][fert] = 1.0;
        }
    }
}

void load_settings() {
    preferences.begin("irrigation", false); // Open in read-write mode
    
    // Initialize with defaults first
    init_weekly_dosing();
    
    // Load weekly dosing schedule
    for (int day = 0; day < 7; day++) {
        for (int fert = 0; fert < NUM_FERTILIZERS; fert++) {
            String key = "dose_" + String(day) + "_" + String(fert);
            weekly_dosing_ml[day][fert] = preferences.getFloat(key.c_str(), 1.0);
        }
        String enabled_key = "water_" + String(day);
        weekly_watering_enabled[day] = preferences.getBool(enabled_key.c_str(), true);
    }
    
    // Load other settings
    schedule_hour = preferences.getInt("sched_hour", 8);
    schedule_minute = preferences.getInt("sched_min", 0);
    
    for (int i = 0; i < NUM_FERTILIZERS; i++) {
        String cal_key = "cal_" + String(i);
        pump_calibration[i] = preferences.getFloat(cal_key.c_str(), 1.0);
    }
    
    fertilizer_motor_speed = preferences.getInt("fert_speed", 200);
    watering_duration_ms = preferences.getULong("water_dur", MAX_WATERING_TIME_MS);
    
    preferences.end();
    logger_log("Settings loaded from NVS");
}

void save_settings() {
    preferences.begin("irrigation", false); // Open in read-write mode
    
    // Save weekly dosing schedule
    for (int day = 0; day < 7; day++) {
        for (int fert = 0; fert < NUM_FERTILIZERS; fert++) {
            String key = "dose_" + String(day) + "_" + String(fert);
            preferences.putFloat(key.c_str(), weekly_dosing_ml[day][fert]);
        }
        String enabled_key = "water_" + String(day);
        preferences.putBool(enabled_key.c_str(), weekly_watering_enabled[day]);
    }
    
    // Save other settings
    preferences.putInt("sched_hour", schedule_hour);
    preferences.putInt("sched_min", schedule_minute);
    
    for (int i = 0; i < NUM_FERTILIZERS; i++) {
        String cal_key = "cal_" + String(i);
        preferences.putFloat(cal_key.c_str(), pump_calibration[i]);
    }
    
    preferences.putInt("fert_speed", fertilizer_motor_speed);
    preferences.putULong("water_dur", watering_duration_ms);
    
    preferences.end();
    logger_log("Settings saved to NVS");
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

// This is the function that scheduler should call
void trigger_dosing() {
    start_watering_sequence();
}

void start_ap_mode() {
    WiFi.softAP("IrrigationSetup");
    IPAddress IP = WiFi.softAPIP();
    String log_msg = "AP mode started - IP: " + IP.toString();
    logger_log(log_msg.c_str());
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
extern bool humidifier_pump_active;
extern bool watering_pump_active;
static bool ntp_synced = false;

// Watering sequence state machine
enum WateringState {
    IDLE,
    DOSING,
    FILLING,
    FILLED,
    WATERING
};
static WateringState watering_state = IDLE;

void start_watering_sequence() {
    if (watering_state == IDLE) {
        start_fertilizer_dosing();
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
    
    // REST API: Run humidifier pump
    server.on("/api/run_humidifier_pump", HTTP_POST, [](AsyncWebServerRequest *request){
        unsigned long ms = 5000;
        if (request->hasParam("ms", true)) ms = request->getParam("ms", true)->value().toInt();
        pump_control_run_humidifier_pump(ms);
        request->send(200, "text/plain", "Humidifier pump running");
    });
    
    // REST API: Stop humidifier pump
    server.on("/api/stop_humidifier_pump", HTTP_POST, [](AsyncWebServerRequest *request){
        pump_control_stop_humidifier_pump();
        request->send(200, "text/plain", "Humidifier pump stopped");
    });
    
    // REST API: Run watering pump
    server.on("/api/run_watering_pump", HTTP_POST, [](AsyncWebServerRequest *request){
        unsigned long ms = watering_duration_ms;
        if (request->hasParam("ms", true)) ms = request->getParam("ms", true)->value().toInt();
        pump_control_run_watering_pump(ms);
        request->send(200, "text/plain", "Watering pump running");
    });
    
    // REST API: Stop watering pump
    server.on("/api/stop_watering_pump", HTTP_POST, [](AsyncWebServerRequest *request){
        pump_control_stop_watering_pump();
        request->send(200, "text/plain", "Watering pump stopped");
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
    
    // REST API: Get watering duration
    server.on("/api/watering_duration", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"watering_duration_ms\":" + String(watering_duration_ms) + "}";
        request->send(200, "application/json", json);
    });
    
    // REST API: Set watering duration
    server.on("/api/watering_duration", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("watering_duration_ms", true)) {
            watering_duration_ms = request->getParam("watering_duration_ms", true)->value().toInt();
            // Clamp the value to a reasonable range (1 second to 30 minutes)
            if (watering_duration_ms < 1000) watering_duration_ms = 1000;
            if (watering_duration_ms > 1800000) watering_duration_ms = 1800000;
        }
        save_settings();
        request->send(200, "text/plain", "Watering duration saved");
    });
    
    // REST API: Debug pump control
    server.on("/api/debug_pump", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!request->hasParam("pump", true) || !request->hasParam("action", true)) {
            request->send(400, "text/plain", "Missing pump or action parameter");
            return;
        }
        
        int pump = request->getParam("pump", true)->value().toInt();
        String action = request->getParam("action", true)->value();
        
        if (action == "on") {
            int speed = 200; // Default speed
            if (request->hasParam("speed", true)) {
                speed = request->getParam("speed", true)->value().toInt();
            }
            
            if (pump >= 0 && pump <= 4) {
                // Fertilizer pumps (0-4 map to motors 1-5)
                int motor_num = pump + 1;
                set_motor_speed(motor_num, speed);
                run_motor_forward(motor_num);
                request->send(200, "text/plain", "Fertilizer pump " + String(pump) + " turned on");
            } else if (pump == 5) {
                // Watering pump
                pump_control_run_watering_pump(60000);
                filling = true;
                request->send(200, "text/plain", "Watering pump turned on");
            } else if (pump == 6) {
                // Humidifier pump
                pump_control_run_humidifier_pump(60000);
                request->send(200, "text/plain", "Humidifier pump turned on");
            } else {
                request->send(400, "text/plain", "Invalid pump number");
            }
        } else if (action == "off") {
            if (pump >= 0 && pump <= 4) {
                // Fertilizer pumps (0-4 map to motors 1-5)
                int motor_num = pump + 1;
                stop_motor(motor_num);
                request->send(200, "text/plain", "Fertilizer pump " + String(pump) + " turned off");
            } else if (pump == 5) {
                // Watering pump
                pump_control_stop_watering_pump();
                filling = false;
                request->send(200, "text/plain", "Watering pump turned off");
            } else if (pump == 6) {
                // Humidifier pump
                pump_control_stop_humidifier_pump();
                request->send(200, "text/plain", "Humidifier pump turned off");
            } else {
                request->send(400, "text/plain", "Invalid pump number");
            }
        } else {
            request->send(400, "text/plain", "Invalid action. Use 'on' or 'off'");
        }
    });
    
    // REST API: Stop all pumps
    server.on("/api/stop_all_pumps", HTTP_POST, [](AsyncWebServerRequest *request){
        // Stop all fertilizer pumps (motors 1-5)
        for (int i = 1; i <= 5; i++) {
            stop_motor(i);
        }
        // Stop main tank
        valve_control_stop_main_tank();
        filling = false;
        // Stop humidifier pump
        pump_control_stop_humidifier_pump();
        // Stop watering pump
        pump_control_stop_watering_pump();
        
        request->send(200, "text/plain", "All pumps stopped");
    });
    
    // REST API: Get status
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<256> doc;
        doc["tank_full"] = sensors_get_liquid_level();
        doc["filling"] = filling;
        doc["humidifier_pump"] = humidifier_pump_active;
        doc["watering_pump"] = watering_pump_active;
        doc["watering_duration_ms"] = watering_duration_ms;
        doc["ota_ready"] = true;
        
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
    
    // REST API: Get OTA info
    server.on("/api/ota_info", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<256> doc;
        doc["hostname"] = "irrigation-system";
        doc["ip"] = WiFi.localIP().toString();
        doc["ota_port"] = 3232; // Default Arduino OTA port
        doc["password_protected"] = true;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Logger API: Get logs
    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        String logs = logger_get_logs();
        StaticJsonDocument<4096> doc;
        doc["logs"] = logs;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Logger API: Clear logs
    server.on("/api/logs", HTTP_DELETE, [](AsyncWebServerRequest *request){
        logger_clear();
        request->send(200, "text/plain", "Logs cleared");
    });
    
    // Logger API: Download logs
    server.on("/api/logs/download", HTTP_GET, [](AsyncWebServerRequest *request){
        String logs = logger_get_logs();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", logs);
        response->addHeader("Content-Disposition", "attachment; filename=irrigation_logs.txt");
        request->send(response);
    });
    
    // Logger API: Get log info
    server.on("/api/logs/info", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<128> doc;
        doc["current_file_size"] = logger_get_file_size();
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
    logger_log("Waiting for NTP sync...");
    time_t now = 0;
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 30) {
        delay(500);
        now = time(nullptr);
        retries++;
    }
    if (now < 8 * 3600 * 2) {
        logger_log("NTP sync failed");
        ntp_synced = false;
    } else {
        logger_log("NTP sync successful");
        ntp_synced = true;
    }
}

void setup_ota() {
    // Set hostname for OTA
    ArduinoOTA.setHostname("irrigation");
    
    // Optionally set a password for OTA updates
    ArduinoOTA.setPassword("irrigation2024");
    
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_LittleFS
            type = "filesystem";
        }
        logger_log(("OTA Start: " + type).c_str());
        
        // Stop all pumps and valves during OTA
        for (int i = 1; i <= 5; i++) {
            stop_motor(i);
        }
        valve_control_stop_main_tank();
        pump_control_stop_humidifier_pump();
        pump_control_stop_watering_pump();
    });
    
    ArduinoOTA.onEnd([]() {
        logger_log("OTA End");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int last_percent = 0;
        unsigned int percent = (progress / (total / 100));
        if (percent != last_percent && percent % 10 == 0) {
            String log_msg = "OTA Progress: " + String(percent) + "%";
            logger_log(log_msg.c_str());
            last_percent = percent;
        }
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        String error_msg = "OTA Error: ";
        if (error == OTA_AUTH_ERROR) {
            error_msg += "Auth Failed";
        } else if (error == OTA_BEGIN_ERROR) {
            error_msg += "Begin Failed";
        } else if (error == OTA_CONNECT_ERROR) {
            error_msg += "Connect Failed";
        } else if (error == OTA_RECEIVE_ERROR) {
            error_msg += "Receive Failed";
        } else if (error == OTA_END_ERROR) {
            error_msg += "End Failed";
        }
        logger_log(error_msg.c_str());
    });
    
    ArduinoOTA.begin();
    logger_log("OTA Ready");
}

void setup() {
    Serial.begin(115200);
    motor_shield_init();
    pump_control_init();
    valve_control_init();
    scheduler_init();
    sensors_init();
    logger_init();

    if (!LittleFS.begin()) {
        logger_log("LittleFS Mount Failed");
    }
    init_weekly_dosing(); // Initialize with defaults first
    load_settings();       // Then load from file if available

    bool wifi_ok = false;
    if (load_wifi_credentials()) {
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(500);
        }
        if (WiFi.status() == WL_CONNECTED) {
            String log_msg = "WiFi connected - IP: " + WiFi.localIP().toString();
            logger_log(log_msg.c_str());
            wifi_ok = true;
        } else {
            logger_log("WiFi connect failed");
        }
    } else {
        logger_log("No WiFi credentials found in LittleFS, starting AP mode");
    }
    if (!wifi_ok) {
        start_ap_mode();
        return;
    }

    sync_ntp();

    // Set timezone for Amsterdam (CET/CEST)
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    setup_ota();

    setup_routes();
    server.begin();
}

void loop() {
    ArduinoOTA.handle();
    
    scheduler_run();
    pump_control_run();
    sensors_read();

    switch (watering_state) {
        case IDLE:
            break;
        case DOSING:
            if (!pump_control_is_dosing()) {
                // Dosing is complete, move to filling
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
                    watering_state = FILLED;
                } else if (millis() - fill_start_time > MAIN_TANK_FILL_TIMEOUT_MS) {
                    valve_control_stop_main_tank();
                    filling = false;
                    watering_state = FILLED;
                    logger_log("[SAFETY] Main tank fill timeout reached, valve closed");
                }
            } else {
                watering_state = FILLED;
            }
            break;
        case FILLED: {
            // Start watering pump for configured time after tank is filled
            pump_control_run_watering_pump(watering_duration_ms);
            watering_state = WATERING;
            String log_msg = "[DEBUG] Tank filled - starting watering pump for " + String(watering_duration_ms) + "ms";
            logger_log(log_msg.c_str());
            break;
        }
        case WATERING:
            // Wait for watering to complete (pump will stop automatically)
            if (!watering_pump_active) {
                watering_state = IDLE;
                logger_log("[DEBUG] Watering complete - sequence finished");
            }
            break;
    }

    if (filling && sensors_get_liquid_level()) {
        valve_control_stop_main_tank();
        filling = false;
    }
    delay(100);
}