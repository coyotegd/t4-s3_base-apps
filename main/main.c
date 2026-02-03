#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "hal_mgr.h"
#include "lvgl_mgr.h"
#include "lvgl.h"
#include "lv_ui.h"
#include "ui_private.h"
#include "ui_launcher.h"

static const char *TAG = "app_launcher";

// HAL callback handlers
static void my_usb_handler(bool plugged, void *user_ctx) {
    ESP_LOGI(TAG, "USB %s", plugged ? "Plugged" : "Unplugged");
}

static void my_charge_handler(bool charging, void *user_ctx) {
    ESP_LOGI(TAG, "Battery %s", charging ? "Charging" : "Not Charging");
}

static void my_battery_handler(bool present, void *user_ctx) {
    ESP_LOGI(TAG, "Battery %s", present ? "Present" : "Removed");
}

static void my_rotation_handler(rm690b0_rotation_t rot, void *user_ctx) {
    ESP_LOGI(TAG, "Display rotation changed to: %d", rot);
}

void app_main(void)
{
    ESP_LOGI(TAG, "My Custom App Starting...");

    // Initialize BSP from the base components
    if (bsp_init() != ESP_OK) {
        ESP_LOGE(TAG, "BSP init failed!");
        return;
    }

    ESP_LOGI(TAG, "BSP initialized - HAL, LVGL ready!");
    
    // Initialize HAL BSP UI system (stats timer, etc.)
    // Note: This will create ui_home, but we've wrapped ui_home_create to be empty overridden by our own UI
    lvgl_mgr_lock();
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0); // Ensure black BG immediately
    lv_ui_init();   // Initialize HAL BSP UI components
    
    // Show our ui_launcher page as default home screen
    ui_launcher_init();
    lvgl_mgr_unlock();

    ESP_LOGI(TAG, "Launcher UI initialized");

    // Register HAL callbacks after UI is set up
    hal_mgr_register_usb_callback(my_usb_handler, NULL);
    hal_mgr_register_charge_callback(my_charge_handler, NULL);
    hal_mgr_register_battery_callback(my_battery_handler, NULL);
    hal_mgr_register_rotation_callback(my_rotation_handler, NULL);

    // Main loop - UI is now interactive via touch
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
