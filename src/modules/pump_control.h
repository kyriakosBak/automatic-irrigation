#pragma once
void pump_control_init();
void pump_control_run();
void trigger_dosing();
unsigned long ml_to_runtime(int pump, float ml);
void pump_control_run_aux_pump(unsigned long ms);
void pump_control_stop_aux_pump();
bool pump_control_is_dosing();
int get_fertilizer_motor_speed();
int get_current_day_of_week();
float get_current_dosing_ml(int fertilizer_index); 