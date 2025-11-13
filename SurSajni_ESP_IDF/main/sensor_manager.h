/**
 * @file sensor_manager.h
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "esp_err.h"
#include <stdint.h>

esp_err_t sensor_manager_init(void);
uint8_t sensor_get_tank_level(void);
uint16_t sensor_get_turbidity(void);
float sensor_get_battery_voltage(void);
bool sensor_get_flow_ok(void);

#endif
