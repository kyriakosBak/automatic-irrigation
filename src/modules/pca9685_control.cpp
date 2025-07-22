#include "pca9685_control.h"
#include <Wire.h>
#include <Adafruit_MotorShield.h>

// Motor shield instance
Adafruit_MotorShield motor_shield = Adafruit_MotorShield(0x60);

// Motor pointers for up to 4 motors
Adafruit_DCMotor *motors[4] = {nullptr, nullptr, nullptr, nullptr};

void motor_shield_init() {
#if defined(ARDUINO_ARCH_ESP8266)
    Wire.begin(D2, D1); // Wemos D1 mini: SDA=D2, SCL=D1
#else
    Wire.begin();
#endif
    
    if (!motor_shield.begin()) {
        Serial.println("ERROR: Could not find Motor Shield. Check wiring.");
        return;
    }
    
    // Initialize motor pointers (motors 1-4 on shield)
    for (int i = 0; i < 4; i++) {
        motors[i] = motor_shield.getMotor(i + 1);
        if (motors[i]) {
            motors[i]->run(RELEASE); // Ensure motor is stopped initially
        }
    }
    
    Serial.println("Motor shield initialized");
}

void set_motor_speed(int motor_number, int speed) {
    if (motor_number < 1 || motor_number > 4) {
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
    if (motor_number < 1 || motor_number > 4) {
        return; // Invalid motor number
    }
    
    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->run(FORWARD);
    }
}

void run_motor_backward(int motor_number) {
    if (motor_number < 1 || motor_number > 4) {
        return; // Invalid motor number
    }
    
    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->run(BACKWARD);
    }
}

void stop_motor(int motor_number) {
    if (motor_number < 1 || motor_number > 4) {
        return; // Invalid motor number
    }
    
    int motor_index = motor_number - 1;
    if (motors[motor_index]) {
        motors[motor_index]->run(RELEASE);
    }
}

void stop_all_motors() {
    for (int i = 0; i < 4; i++) {
        if (motors[i]) {
            motors[i]->run(RELEASE);
        }
    }
}