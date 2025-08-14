#include "pump_control.h"
#include "motor_shield_control.h"
#include <Arduino.h>
#include "config/config.h"

extern float dosing_ml[NUM_FERTILIZERS];
extern float pump_calibration[NUM_PUMPS];
extern int fertilizer_motor_speed;

#define MAX_MOTOR_SPEED 255  // Full speed for motors

unsigned long pump_start_time[NUM_PUMPS] = {0};
bool pump_running[NUM_PUMPS] = {false};

// Dosing state
static int dosing_stage = -1;
static unsigned long dosing_end_time = 0;

// Auxiliary pump state
bool aux_pump_active = false;
static unsigned long aux_pump_end_time = 0;

unsigned long ml_to_runtime(int pump, float ml) {
    float cal = (pump >= 0 && pump < NUM_PUMPS && pump_calibration[pump] > 0) ? pump_calibration[pump] : 1.0;
    return (unsigned long)(ml * 1000 / cal);
}

void trigger_dosing() {
    dosing_stage = 0;
    dosing_end_time = millis() + ml_to_runtime(0, dosing_ml[0]);
    set_motor_speed(1, fertilizer_motor_speed);  // Motor 1 for pump 0
    run_motor_forward(1);
    pump_running[0] = true;
    Serial.print("[DEBUG] Pump 0: OPEN (dosing started, ");
    Serial.print(dosing_ml[0]);
    Serial.println(" ml)");
}

void pump_control_run_aux_pump(unsigned long ms) {
    int aux_motor = AUX_PUMP_CHANNEL + 1;  // Convert channel to motor number (1-4)
    set_motor_speed(aux_motor, MAX_MOTOR_SPEED);
    run_motor_forward(aux_motor);
    aux_pump_active = true;
    aux_pump_end_time = millis() + ms;
    Serial.print("[DEBUG] Aux pump: OPEN (run for ");
    Serial.print(ms);
    Serial.println(" ms)");
}

void pump_control_stop_aux_pump() {
    int aux_motor = AUX_PUMP_CHANNEL + 1;  // Convert channel to motor number (1-4)
    stop_motor(aux_motor);
    aux_pump_active = false;
    Serial.println("[DEBUG] Aux pump: CLOSED");
}

void pump_control_init() {
    // Stop all motors initially
    stop_all_motors();
    for (int i = 0; i < NUM_PUMPS; i++) {
        pump_running[i] = false;
        pump_start_time[i] = 0;
    }
    dosing_stage = -1;
    aux_pump_active = false;
}

void pump_control_run() {
    // Auxiliary pump logic
    if (aux_pump_active && millis() > aux_pump_end_time) {
        pump_control_stop_aux_pump();
    }
    // Dosing sequence
    if (dosing_stage >= 0 && dosing_stage < NUM_FERTILIZERS) {
        if (millis() > dosing_end_time) {
            int motor_num = dosing_stage + 1;  // Convert pump index to motor number (1-4)
            stop_motor(motor_num);
            Serial.print("[DEBUG] Pump ");
            Serial.print(dosing_stage);
            Serial.println(": CLOSED (dosing complete)");
            pump_running[dosing_stage] = false;
            dosing_stage++;
            if (dosing_stage < NUM_FERTILIZERS) {
                motor_num = dosing_stage + 1;  // Convert pump index to motor number (1-4)
                set_motor_speed(motor_num, fertilizer_motor_speed);
                run_motor_forward(motor_num);
                pump_running[dosing_stage] = true;
                Serial.print("[DEBUG] Pump ");
                Serial.print(dosing_stage);
                Serial.print(": OPEN (dosing started, ");
                Serial.print(dosing_ml[dosing_stage]);
                Serial.println(" ml)");
                dosing_end_time = millis() + ml_to_runtime(dosing_stage, dosing_ml[dosing_stage]);
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