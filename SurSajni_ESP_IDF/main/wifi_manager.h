/**
 * @file wifi_manager.h
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

esp_err_t wifi_manager_init(void);
bool wifi_is_connected(void);

#endif
