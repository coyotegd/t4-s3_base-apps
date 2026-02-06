#include "ui_launcher.h"
#include "ui_maze.h"
#include "ui_sports.h"
#include "ui_weather.h"
#include "ui_private.h"
#include "esp_log.h"

#include "wifi_mgr.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "ui_launcher";

static lv_obj_t *launcher_screen = NULL;
static lv_obj_t * lbl_header_time = NULL;
static lv_obj_t * lbl_header_wifi = NULL;
static lv_timer_t * status_timer = NULL;

static void status_bar_timer_cb(lv_timer_t * t) {
    if (!lbl_header_time || !lbl_header_wifi) return;
    
    // Update Time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Check WiFi connection first, then time sync status
    if (!wifi_mgr_is_connected()) {
        lv_label_set_text(lbl_header_time, "waiting for Wi-Fi connection . . .");
    } else if (timeinfo.tm_year > (2020 - 1900)) {
        lv_label_set_text_fmt(lbl_header_time, "%02d/%02d/%04d %02d:%02d", 
            timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year + 1900,
            timeinfo.tm_hour, timeinfo.tm_min);
    } else {
        lv_label_set_text(lbl_header_time, "http d/t syncing . . .");
    }
    
    // Update WiFi Icon
    if (wifi_mgr_is_connected()) {
        lv_label_set_text(lbl_header_wifi, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(lbl_header_wifi, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text(lbl_header_wifi, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(lbl_header_wifi, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

static void launcher_cleanup_cb(lv_event_t * e) {
    if (status_timer) {
        lv_timer_del(status_timer);
        status_timer = NULL;
    }
    lbl_header_time = NULL;
    lbl_header_wifi = NULL;
    ESP_LOGI(TAG, "Launcher cleanup complete");
}

// Helper function to create neon-style buttons matching HAL BSP theme
static void create_neon_btn(lv_obj_t * parent, const char * icon, const char * text, lv_color_t color, lv_event_cb_t event_cb) {
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_height(btn, 95);
    lv_obj_set_flex_grow(btn, 1); // Allow button to grow in row layout
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 4, 0);
    lv_obj_set_style_pad_gap(btn, 4, 0);

    // Default Style - transparent background with colored border (neon effect)
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Pressed Style - filled with glow effect
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 30, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(btn, color, LV_PART_MAIN | LV_STATE_PRESSED);
    
    // Icon
    lv_obj_t * lbl_icon = lv_label_create(btn);
    lv_label_set_text(lbl_icon, icon);
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_white(), 0);

    // Label
    lv_obj_t * lbl_text = lv_label_create(btn);
    lv_label_set_text(lbl_text, text);
    lv_obj_set_style_text_font(lbl_text, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_text, lv_color_white(), 0);
}

// Button event handlers
static void btn_maze_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Maze button clicked");
        ui_maze_show();
    }
}

static void btn_sports_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Sports button clicked");
        ui_sports_show();
    }
}

static void btn_weather_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Weather button clicked");
        ui_weather_show();
    }
}

static void btn_settings_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Settings button clicked");
        
        // Create a new screen for the HAL BSP home view
        // This prevents destroyng the active screen before the new one is ready
        // and provides a valid screen for the view switcher to use
        lv_obj_t * next_screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(next_screen, lv_color_hex(0x000000), 0); // Default black
        lv_screen_load(next_screen);

        // Call the HAL BSP's home view (hardware settings)
        // This sets up the timer to build the UI on lv_screen_active() (which is now next_screen)
        show_home_view(e);
        
        // Properly destroy launcher with delay to avoid deleting the object 
        // currently processing this event
        if (launcher_screen) {
            ESP_LOGI(TAG, "Scheduling launcher screen destruction");
            lv_obj_delete_delayed(launcher_screen, 100);
            launcher_screen = NULL;
        }
    }
}

void ui_launcher_destroy(void) {
    if (launcher_screen) {
        ESP_LOGI(TAG, "Destroying launcher screen");
        lv_obj_del(launcher_screen);
        launcher_screen = NULL;
    }
}

void ui_launcher_init(void) {
    ESP_LOGI(TAG, "Initializing launcher screen");
    
    // Create the container screen
    launcher_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(launcher_screen, LV_OBJ_FLAG_SCROLLABLE);       // Disable scrolling
    lv_obj_set_style_bg_color(launcher_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_flex_flow(launcher_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(launcher_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(launcher_screen, launcher_cleanup_cb, LV_EVENT_DELETE, NULL);

    // Header Row
    lv_obj_t * header_row = lv_obj_create(launcher_screen);
    lv_obj_remove_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);            // Disable scrolling
    lv_obj_set_size(header_row, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(header_row, 10, 0);
    lv_obj_set_style_pad_right(header_row, 10, 0);
    
    // Time Label
    lbl_header_time = lv_label_create(header_row);
    lv_label_set_text(lbl_header_time, "waiting for Wi-Fi connection . . .");
    lv_obj_set_style_text_font(lbl_header_time, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl_header_time, lv_color_white(), 0);
    
    // WiFi Label
    lbl_header_wifi = lv_label_create(header_row);
    lv_label_set_text(lbl_header_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_header_wifi, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_header_wifi, lv_palette_main(LV_PALETTE_RED), 0);
    
    // Start Timer
    if (status_timer) lv_timer_del(status_timer);
    status_timer = lv_timer_create(status_bar_timer_cb, 1000, NULL);
    status_bar_timer_cb(NULL);
    
    // Create Main Content Container (Dark Grey Background) - vertically centered
    lv_obj_t * main_cont = lv_obj_create(launcher_screen);
    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);              // Disable scrolling
    lv_obj_set_width(main_cont, LV_PCT(100));
    lv_obj_set_flex_grow(main_cont, 1);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);               // Column layout for Title + Buttons
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x101010), 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 10, 0);

    // Title: "LilyGo T4-S3 & Me" (Gold Color) - Centered between status bar and buttons
    lv_obj_t * lbl_title = lv_label_create(main_cont);
    lv_label_set_text(lbl_title, "LilyGo T4-S3 & Me");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFD700), 0); // Gold

    // Button Row Container - Vertically centered
    lv_obj_t * btn_row = lv_obj_create(main_cont);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, LV_PCT(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);                 // Transparent
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);                    // Horizontal Row for buttons
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row, 10, 0);
    
    // Maze Button
    create_neon_btn(btn_row, LV_SYMBOL_SHUFFLE, "Maze", lv_palette_main(LV_PALETTE_BLUE), btn_maze_event_cb);
    
    // Sports Button
    create_neon_btn(btn_row, LV_SYMBOL_GPS, "Sports", lv_palette_main(LV_PALETTE_GREEN), btn_sports_event_cb);
    
    // Weather Button
    create_neon_btn(btn_row, LV_SYMBOL_TINT, "Weather", lv_palette_main(LV_PALETTE_CYAN), btn_weather_event_cb);
    
    // Settings Button
    create_neon_btn(btn_row, LV_SYMBOL_SETTINGS, "Settings", lv_color_hex(0xFF3300), btn_settings_event_cb);
    
    // Load the screen
    lv_screen_load(launcher_screen);
    
    ESP_LOGI(TAG, "Launcher screen initialized");
}

void ui_launcher_show(void) {
    ESP_LOGI(TAG, "Showing launcher screen");
    
    // Clean up HAL BSP views if they exist
    clear_current_view();
    
    // Always recreate launcher for clean state
    ui_launcher_destroy();
    ui_launcher_init();
}
