#include "pump_control.h"
#include "motor_shield_control.h"
#include <Arduino.h>
#include "config/config.h"
#include <time.h>

extern float weekly_dosing_ml[7][NUM_FERTILIZERS];
extern bool weekly_watering_enabled[7];
extern float pump_calibration[NUM_FERTILIZERS];
extern int fertilizer_motor_speed;

#define MAX_MOTOR_SPEED 255  // Full speed for motors

unsigned long pump_start_time[NUM_PUMPS] = {0};
bool pump_running[NUM_PUMPS] = {false};

// Dosing state
static int dosing_stage = -1;
static unsigned long dosing_end_time = 0;

// Humidifier pump state
bool humidifier_pump_active = false;
static unsigned long humidifier_pump_end_time = 0;

unsigned long ml_to_runtime(int pump, float ml) {
    float cal = (pump >= 0 && pump < NUM_FERTILIZERS && pump_calibration[pump] > 0) ? pump_calibration[pump] : 1.0;
    return (unsigned long)(ml * 1000 / cal);
}

int get_current_day_of_week() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo)) {
        return timeinfo.tm_wday; // 0=Sunday, 1=Monday, ..., 6=Saturday
    }
    return 0; // Default to Sunday if time is not available
}

float get_current_dosing_ml(int fertilizer_index) {
    int day = get_current_day_of_week();
    if (fertilizer_index >= 0 && fertilizer_index < NUM_FERTILIZERS) {
        return weekly_dosing_ml[day][fertilizer_index];
    }
    return 10.0; // Default value
}

bool is_watering_enabled_today() {
    int day = get_current_day_of_week();
    return weekly_watering_enabled[day];
}

void trigger_dosing() {
    // Check if watering is enabled for today
    if (!is_watering_enabled_today()) {
        Serial.println("[DEBUG] Watering is disabled for today - skipping dosing");
        return;
    }
    
    dosing_stage = 0;
    float current_ml = get_current_dosing_ml(0);
    dosing_end_time = millis() + ml_to_runtime(0, current_ml);
    set_motor_speed(1, fertilizer_motor_speed);  // Motor 1 for pump 0
    run_motor_forward(1);
    pump_running[0] = true;
    Serial.print("[DEBUG] Pump 0: OPEN (dosing started, ");
    Serial.print(current_ml);
    Serial.println(" ml)");
}

void pump_control_run_humidifier_pump(unsigned long ms) {
    int humidifier_motor = HUMIDIFIER_PUMP_CHANNEL;  // Motor 7 for humidifier pump
    set_motor_speed(humidifier_motor, MAX_MOTOR_SPEED);
    run_motor_forward(humidifier_motor);
    humidifier_pump_active = true;
    humidifier_pump_end_time = millis() + ms;
    Serial.print("[DEBUG] Humidifier pump: OPEN (run for ");
    Serial.print(ms);
    Serial.println(" ms)");
}

void pump_control_stop_humidifier_pump() {
    int humidifier_motor = HUMIDIFIER_PUMP_CHANNEL;  // Motor 7 for humidifier pump
    stop_motor(humidifier_motor);
    humidifier_pump_active = false;
    Serial.println("[DEBUG] Humidifier pump: CLOSED");
}

void pump_control_init() {
    // Stop all motors initially
    stop_all_motors();
    for (int i = 0; i < NUM_PUMPS; i++) {
        pump_running[i] = false;
        pump_start_time[i] = 0;
    }
    dosing_stage = -1;
    humidifier_pump_active = false;
}

void pump_control_run() {
    // Humidifier pump logic
    if (humidifier_pump_active && millis() > humidifier_pump_end_time) {
        pump_control_stop_humidifier_pump();
    }
    // Dosing sequence
    if (dosing_stage >= 0 && dosing_stage < NUM_FERTILIZERS) {
        if (millis() > dosing_end_time) {
            int motor_num = dosing_stage + 1;  // Convert pump index to motor number (1-5)
            stop_motor(motor_num);
            Serial.print("[DEBUG] Pump ");
            Serial.print(dosing_stage);
            Serial.println(": CLOSED (dosing complete)");
            pump_running[dosing_stage] = false;
            dosing_stage++;
            if (dosing_stage < NUM_FERTILIZERS) {
                motor_num = dosing_stage + 1;  // Convert pump index to motor number (1-5)
                set_motor_speed(motor_num, fertilizer_motor_speed);
                run_motor_forward(motor_num);
                pump_running[dosing_stage] = true;
                Serial.print("[DEBUG] Pump ");
                Serial.print(dosing_stage);
                Serial.print(": OPEN (dosing started, ");
                float current_ml = get_current_dosing_ml(dosing_stage);
                Serial.print(current_ml);
                Serial.println(" ml)");
                dosing_end_time = millis() + ml_to_runtime(dosing_stage, current_ml);
            } else {
                dosing_stage = -1; // Done
            }
        }
        return;
    }
}

bool pump_control_is_dosing() {
    return dosing_stage >= 0;
}

int get_fertilizer_motor_speed() {
    return fertilizer_motor_speed;
}