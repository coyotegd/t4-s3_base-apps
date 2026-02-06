#include "ui_weather.h"
#include "ui_launcher.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ui_weather";

static lv_obj_t *weather_screen = NULL;

static void btn_back_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Back button clicked");
        ui_launcher_show();
    }
}

void ui_weather_cleanup(void) {
    if (weather_screen) {
        ESP_LOGI(TAG, "Cleaning up weather screen");
        lv_obj_del(weather_screen);
        weather_screen = NULL;
    }
}

void ui_weather_show(void) {
    ESP_LOGI(TAG, "Showing weather screen");
    
    // Clean up if already exists
    ui_weather_cleanup();
    
    // Create main screen
    weather_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(weather_screen, lv_color_hex(0x001020), 0); // Dark blue background
    lv_obj_set_flex_flow(weather_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(weather_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Top bar with back button
    lv_obj_t *top_bar = lv_obj_create(weather_screen);
    lv_obj_set_size(top_bar, LV_PCT(100), 60);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(top_bar, 10, 0);
    
    // Back button
    lv_obj_t *btn_back = lv_button_create(top_bar);
    lv_obj_set_size(btn_back, 80, 45);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Back");
    lv_obj_center(lbl_back);
    
    // Content area
    lv_obj_t *content = lv_obj_create(weather_screen);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_style_bg_color(content, lv_color_hex(0x001020), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_gap(content, 20, 0);
    
    // Weather icon
    lv_obj_t *icon = lv_label_create(content);
    lv_label_set_text(icon, LV_SYMBOL_TINT);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(icon, lv_palette_main(LV_PALETTE_CYAN), 0); // Cyan water drop
    
    // Title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "Weather");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_CYAN), 0);
    
    // Placeholder text
    lv_obj_t *placeholder = lv_label_create(content);
    lv_label_set_text(placeholder, "Weather data coming soon...\n\nThis screen will display:\n"
                                   "• Current temperature\n"
                                   "• Weather conditions\n"
                                   "• Forecast\n"
                                   "• Location info");
    lv_label_set_long_mode(placeholder, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(placeholder, LV_PCT(90));
    lv_obj_set_style_text_font(placeholder, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(placeholder, lv_color_white(), 0);
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
    
    // Load screen
    lv_screen_load(weather_screen);
    
    ESP_LOGI(TAG, "Weather screen initialized");
}
