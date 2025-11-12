#!/usr/bin/env python3
"""
Arduino to ESP-IDF Converter for Sursajni Controller
This script converts Arduino-style code to ESP-IDF format
"""

import re
import sys

def convert_arduino_to_esp_idf(input_file, output_file):
    """Convert Arduino .ino file to ESP-IDF .cpp file"""
    
    with open(input_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Header conversions
    header_replacements = {
        '#include <SPI.h>': '#include "driver/spi_master.h"',
        '#include <Wire.h>': '#include "driver/i2c.h"',
        '#include <EEPROM.h>': '#include "nvs_flash.h"\n#include "nvs.h"',
        '#include <WiFi.h>': '#include "esp_wifi.h"\n#include "esp_event.h"',
        '#include <WebServer.h>': '#include "esp_http_server.h"',
        '#include <ArduinoJson.h>': '// ArduinoJson can still be used with ESP-IDF\n#include <ArduinoJson.h>',
        '#include <ESPmDNS.h>': '#include "mdns.h"',
        '#include <ArduinoOTA.h>': '#include "esp_ota_ops.h"\n#include "esp_https_ota.h"',
        '#include <WiFiUdp.h>': '#include "lwip/sockets.h"',
        '#include <NTPClient.h>': '#include "esp_sntp.h"',
        '#include <esp_task_wdt.h>': '#include "esp_task_wdt.h"',
        '#include <driver/ledc.h>': '#include "driver/ledc.h"',
    }
    
    for arduino_include, espidf_include in header_replacements.items():
        content = content.replace(arduino_include, espidf_include)
    
    # Add ESP-IDF specific headers at the beginning
    esp_idf_headers = '''/* ESP-IDF Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
'''
    
    # Function call conversions
    function_replacements = {
        r'Serial\.begin\(': 'esp_log_level_set("*", ESP_LOG_INFO); // Serial.begin(',
        r'Serial\.print\(': 'ESP_LOGI(TAG, "%s", String(',
        r'Serial\.println\(': 'ESP_LOGI(TAG, "%s\\n", String(',
        r'Serial\.printf\(': 'ESP_LOGI(TAG, ',
        r'delay\(': 'vTaskDelay(pdMS_TO_TICKS(',
        r'\bLOW\b': '0',
        r'\bHIGH\b': '1',
        r'\bINPUT\b': 'GPIO_MODE_INPUT',
        r'\bINPUT_PULLUP\b': 'GPIO_MODE_INPUT', # Will need pull-up config separately
        r'\bOUTPUT\b': 'GPIO_MODE_OUTPUT',
        r'digitalWrite\(': 'gpio_set_level(',
        r'digitalRead\(': 'gpio_get_level(',
        r'pinMode\(': 'gpio_set_direction(',
        r'analogRead\(': 'adc1_get_raw(',
        r'millis\(\)': '(uint32_t)(esp_timer_get_time() / 1000)',
    }
    
    for pattern, replacement in function_replacements.items():
        content = re.sub(pattern, replacement, content)
    
    # Add setup() to app_main() conversion marker
    content = re.sub(r'void setup\(\) \{', 
                     'extern "C" void app_main(void) {\n    // Initialize NVS\n    esp_err_t ret = nvs_flash_init();\n    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {\n        ESP_ERROR_CHECK(nvs_flash_erase());\n        ret = nvs_flash_init();\n    }\n    ESP_ERROR_CHECK(ret);\n\n    ESP_ERROR_CHECK(esp_netif_init());\n    ESP_ERROR_CHECK(esp_event_loop_create_default());\n\n    // Original setup() code:\n    {', 
                     content)
    
    # Convert loop() to task
    content = re.sub(r'void loop\(\) \{',
                     '}\n\nvoid loop_task(void *pvParameters) {\n    while(1) {',
                     content, count=1)
    
    # Add task creation at end of app_main
    # This is a simplified approach - in reality you'd need to properly close the setup function
    
    # Add LOG tag
    content = 'static const char *TAG = "SURSAJNI";\n\n' + content
    
    # Write converted content
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"Conversion complete: {input_file} -> {output_file}")
    print("Note: This is a basic conversion. Manual review and adjustments are required.")
    print("Key areas to check:")
    print("1. EEPROM → NVS conversions")
    print("2. WiFi initialization")
    print("3. Task creation and management")
    print("4. GPIO initialization")
    print("5. Library-specific code (LoRa, U8g2, RTClib)")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 convert.py <input.ino> <output.cpp>")
        sys.exit(1)
    
    convert_arduino_to_esp_idf(sys.argv[1], sys.argv[2])
