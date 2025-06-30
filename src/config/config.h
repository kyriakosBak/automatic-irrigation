#pragma once
// Configuration constants
#define NUM_PUMPS 6
#define NUM_FERTILIZERS 4
#define MAIN_TANK_CHANNEL 0
#define AUX_TANK_CHANNEL 1
#define VALVE_PIN 25 // GPIO pin for solenoid valve
#define AUX_PUMP_CHANNEL 5 // Peristaltic pump for auxiliary tank
#define LIQUID_SENSOR_PIN 32 // Capacitive liquid sensor pin
#define MAIN_TANK_FILL_TIMEOUT_MS 66000 // Default: 1 minute, can be changed
// Add more as needed 