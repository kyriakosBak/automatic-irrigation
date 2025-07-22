#include "motor_shield_control.h"
#include <Wire.h>
#include <Adafruit_MotorShield.h>

// Motor shield instances
Adafruit_MotorShield motor_shield1 = Adafruit_MotorShield(0x60); // Main shield
Adafruit_MotorShield motor_shield2 = Adafruit_MotorShield(0x70); // Extra shield

// Motor pointers for up to 6 motors (4 on shield1, 2 on shield2)
Adafruit_DCMotor *motors[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

void motor_shield_init() {
    Wire.begin();

    bool shield1_ok = motor_shield1.begin();
    bool shield2_ok = motor_shield2.begin();

    if (!shield1_ok) {
        Serial.println("ERROR: Could not find Motor Shield 1 (0x60). Check wiring.");
    }
    if (!shield2_ok) {
        Serial.println("ERROR: Could not find Motor Shield 2 (0x70). Check wiring.");
    }
    if (!shield1_ok && !shield2_ok) {
        return;
    }

    // Initialize motor pointers (motors 1-4 on shield1)
    for (int i = 0; i < 4; i++) {
        motors[i] = shield1_ok ? motor_shield1.getMotor(i + 1) : nullptr;
        if (motors[i]) {
            motors[i]->run(RELEASE);
        }
    }
    // Initialize motor pointers (motors 1-2 on shield2)
    for (int i = 0; i < 2; i++) {
        motors[4 + i] = shield2_ok ? motor_shield2.getMotor(i + 1) : nullptr;
        if (motors[4 + i]) {
            motors[4 + i]->run(RELEASE);
        }
    }

    Serial.println("Motor shields initialized");
}

void set_motor_speed(int motor_number, int speed) {
    if (motor_number < 1 || motor_number > 6) {
        return; // Invalid motor number
    }

    if (speed < 0) speed = 0;
    if (speed > 255) speed = 255;

    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->setSpeed(speed);
    }
}

void run_motor_forward(int motor_number) {
    if (motor_number < 1 || motor_number > 6) {
        return; // Invalid motor number
    }

    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->run(FORWARD);
    }
}

void run_motor_backward(int motor_number) {
    if (motor_number < 1 || motor_number > 6) {
        return; // Invalid motor number
    }

    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->run(BACKWARD);
    }
}

void stop_motor(int motor_number) {
    if (motor_number < 1 || motor_number > 6) {
        return; // Invalid motor number
    }

    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->run(RELEASE);
    }
}

void stop_all_motors() {
    for (int i = 0; i < 6; i++) {
        if (motors[i]) {
            motors[i]->run(RELEASE);
        }
    }
}
