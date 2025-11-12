/**
 * @file pump_control.h
 * @brief Pump control logic
 */

#ifndef PUMP_CONTROL_H
#define PUMP_CONTROL_H

#include "esp_err.h"
#include "common.h"

esp_err_t pump_control_init(void);
esp_err_t pump_start(void);
esp_err_t pump_stop(void);
bool pump_is_running(void);

#endif // PUMP_CONTROL_H
