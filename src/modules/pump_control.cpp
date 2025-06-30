#include "pump_control.h"
#include "pca9685_control.h"
#include <Arduino.h>
#include "config/config.h"

extern float dosing_ml[NUM_FERTILIZERS];
extern float pump_calibration[NUM_PUMPS];

#define PUMP_ON_PWM 3000
#define PUMP_OFF_PWM 0
#define PUMP_RUN_TIME 5000 // ms

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
    set_pump_pwm(0, PUMP_ON_PWM);
    pump_running[0] = true;
}

void pump_control_run_aux_pump(unsigned long ms) {
    set_pump_pwm(AUX_PUMP_CHANNEL, PUMP_ON_PWM);
    aux_pump_active = true;
    aux_pump_end_time = millis() + ms;
}

void pump_control_stop_aux_pump() {
    set_pump_pwm(AUX_PUMP_CHANNEL, PUMP_OFF_PWM);
    aux_pump_active = false;
}

void pump_control_init() {
    for (int i = 0; i < NUM_PUMPS; i++) {
        set_pump_pwm(i, PUMP_OFF_PWM);
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
            set_pump_pwm(dosing_stage, PUMP_OFF_PWM);
            pump_running[dosing_stage] = false;
            dosing_stage++;
            if (dosing_stage < NUM_FERTILIZERS) {
                set_pump_pwm(dosing_stage, PUMP_ON_PWM);
                pump_running[dosing_stage] = true;
                dosing_end_time = millis() + ml_to_runtime(dosing_stage, dosing_ml[dosing_stage]);
            } else {
                dosing_stage = -1; // Done
            }
        }
        return;
    }
    // Demo: cycle all pumps (if not dosing)
    for (int i = 0; i < NUM_PUMPS; i++) {
        if (!pump_running[i] && millis() % (PUMP_RUN_TIME * (i+1)) < 50) {
            set_pump_pwm(i, PUMP_ON_PWM);
            pump_running[i] = true;
            pump_start_time[i] = millis();
        }
        if (pump_running[i] && millis() - pump_start_time[i] > PUMP_RUN_TIME) {
            set_pump_pwm(i, PUMP_OFF_PWM);
            pump_running[i] = false;
        }
    }
}

bool pump_control_is_dosing() {
    return dosing_stage >= 0;
}