#include "motor_shield_control.h"
#include "logger.h"
#include <Wire.h>
#include <Adafruit_MotorShield.h>

// Motor shield instances
Adafruit_MotorShield motor_shield1 = Adafruit_MotorShield(0x60); // Main shield
Adafruit_MotorShield motor_shield2 = Adafruit_MotorShield(0x61); // Extra shield

// Motor pointers for up to 7 motors (4 on shield1, 3 on shield2)
Adafruit_DCMotor *motors[7] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

void motor_shield_init() {
    Wire.begin();

    bool shield1_ok = motor_shield1.begin();
    bool shield2_ok = motor_shield2.begin();

    if (!shield1_ok) {
        logger_log("ERROR: Motor Shield 1 (0x60) not found - check wiring");
    }
    if (!shield2_ok) {
        logger_log("ERROR: Motor Shield 2 (0x61) not found - check wiring");
    }
    if (!shield1_ok && !shield2_ok) {
        logger_log("FATAL: No motor shields found - system cannot operate");
        return;
    }

    // Initialize motor pointers (motors 1-4 on shield1)
    for (int i = 0; i < 4; i++) {
        motors[i] = shield1_ok ? motor_shield1.getMotor(i + 1) : nullptr;
        if (motors[i]) {
            motors[i]->run(RELEASE);
        }
    }
    // Initialize motor pointers (motors 1-3 on shield2)
    for (int i = 0; i < 3; i++) {
        motors[4 + i] = shield2_ok ? motor_shield2.getMotor(i + 1) : nullptr;
        if (motors[4 + i]) {
            motors[4 + i]->run(RELEASE);
        }
    }

    logger_log("Motor shields initialized successfully");
}

void set_motor_speed(int motor_number, int speed) {
    if (motor_number < 1 || motor_number > 7) {
        return; // Invalid motor number
    }

    if (speed < 0) speed = 0;
    if (speed > 255) speed = 255;

    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->setSpeed(speed);
        // Add delay to ensure I2C command is processed
        delay(50);
        
        String log_msg = "Motor " + String(motor_number) + " speed set to " + String(speed);
        logger_log(log_msg.c_str());
    }
}

void run_motor_forward(int motor_number) {
    if (motor_number < 1 || motor_number > 7) {
        return; // Invalid motor number
    }

    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->run(FORWARD);
        // Add delay to ensure I2C command is processed
        delay(50);
        
        String log_msg = "Motor " + String(motor_number) + " started";
        logger_log(log_msg.c_str());
    }
}

void stop_motor(int motor_number) {
    if (motor_number < 1 || motor_number > 7) {
        return; // Invalid motor number
    }

    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->run(RELEASE);
        // Add delay to ensure I2C command is processed
        delay(50);
        
        String log_msg = "Motor " + String(motor_number) + " stopped";
        logger_log(log_msg.c_str());
    }
}

void stop_all_motors() {
    logger_log("Stopping all motors");
    for (int i = 0; i < 7; i++) {
        if (motors[i]) {
            motors[i]->run(RELEASE);
        }
    }
}
