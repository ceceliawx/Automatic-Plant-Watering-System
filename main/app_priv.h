/*
Plant Watering System
*/

// app_priv.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

void app_driver_init(void);
// void app_moisture_sensor_init(void);
int app_moisture_sensor_read(void);
void app_pump_set_state(bool state);



