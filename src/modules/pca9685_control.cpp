#include "pca9685_control.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm1 = Adafruit_PWMServoDriver(0x40); // Pumps 0-3
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41); // Pumps 4-5

void pca9685_init() {
    Wire.begin();
    pwm1.begin();
    pwm1.setPWMFreq(100); // 100 Hz for peristaltic pumps
    pwm2.begin();
    pwm2.setPWMFreq(100);
}

void set_pump_pwm(int channel, int value) {
    // value: 0-4095, limit to 500 max
    if (value > 500) value = 500;
    if (channel >= 0 && channel <= 3) {
        pwm1.setPWM(channel, 0, value);
    } else if (channel >= 4 && channel <= 5) {
        pwm2.setPWM(channel - 4, 0, value); // channels 0,1 on board 2
    }
}