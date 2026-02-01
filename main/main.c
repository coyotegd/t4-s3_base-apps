#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "hal_mgr.h"
#include "lvgl_mgr.h"
#include "lvgl.h"

static const char *TAG = "my_app";

void app_main(void)
{
    ESP_LOGI(TAG, "My Custom App Starting...");

    // Initialize BSP from the base components
    if (bsp_init() != ESP_OK) {
        ESP_LOGE(TAG, "BSP init failed!");
        return;
    }

    ESP_LOGI(TAG, "BSP initialized - HAL, LVGL ready!");

    // Your custom app code here
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "App running...");
    }
}
