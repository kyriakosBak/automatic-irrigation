#pragma once

// Motor control functions using Adafruit Motor Shield V2
void motor_shield_init();
void set_motor_speed(int motor_number, int speed);
void run_motor_forward(int motor_number);
void run_motor_backward(int motor_number);
void stop_motor(int motor_number);
void stop_all_motors();