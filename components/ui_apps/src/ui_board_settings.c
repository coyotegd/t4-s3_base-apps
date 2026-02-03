#include "ui_private.h"
#include "esp_log.h"
#include "ui_launcher.h"

static const char *TAG = "ui_board_set";

LV_IMG_DECLARE(swipeR34);

// Forward declare the real function in case we need it (optional)
void __real_ui_home_create(lv_obj_t * parent);

// Helper for neon buttons (copied/adapted from ui_home logic)
static void create_neon_btn(lv_obj_t * parent, const char * icon, const char * text, lv_color_t color, lv_event_cb_t event_cb) {
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_height(btn, 95);
    lv_obj_set_width(btn, LV_PCT(30));
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn, 4, 0);
    lv_obj_set_style_pad_gap(btn, 4, 0);

    // Default Style
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Pressed Style
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

// --- View Switching Logic ---
// We must replicate the safe switching logic because the BSP's internal one is static/private

static lv_timer_t * switch_timer = NULL;
static void (*target_create_func)(lv_obj_t*) = NULL;

static void switch_timer_cb(lv_timer_t * timer) {
    if (target_create_func) {
        ESP_LOGI(TAG, "Switching view...");
        clear_current_view();
        
        lv_obj_t * scr = lv_screen_active();
        if(!scr) {
            scr = lv_obj_create(NULL);
            lv_screen_load(scr);
        }
        
        // Ensure dark background
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x101010), 0);
        
        // Create the new view
        target_create_func(scr);
        
        // Post-creation hooks
        if (target_create_func == ui_media_create) {
            populate_sd_files_list();
        } else if (target_create_func == ui_display_create) {
             if (lv_display_get_default() && lbl_disp_info) {
                int32_t w = lv_display_get_horizontal_resolution(lv_display_get_default());
                int32_t h = lv_display_get_vertical_resolution(lv_display_get_default());
                lv_label_set_text_fmt(lbl_disp_info, "Driver Resolution: 450x600\nActual Pixel Resolution: %" LV_PRId32 "x%" LV_PRId32 "\nDriver: RM690B0\nInterface: QSPI", w, h);
            }
        }
        
        target_create_func = NULL;
    }
    switch_timer = NULL;
}

static void request_switch(void (*func)(lv_obj_t*)) {
    if (switch_timer) lv_timer_del(switch_timer);
    target_create_func = func;
    switch_timer = lv_timer_create(switch_timer_cb, 10, NULL);
    lv_timer_set_repeat_count(switch_timer, 1);
}

// --- Button Event Handlers ---
static void btn_pmic_cb(lv_event_t * e)     { request_switch(ui_pmic_create); }
static void btn_settings_cb(lv_event_t * e) { request_switch(ui_settings_create); } // Note: this is "Set PM" in UI
static void btn_media_cb(lv_event_t * e)    { request_switch(ui_media_create); }
static void btn_display_cb(lv_event_t * e)  { request_switch(ui_display_create); }
static void btn_sysinfo_cb(lv_event_t * e)  { request_switch(ui_sys_info_create); }
static void btn_ota_cb(lv_event_t * e)      { request_switch(ui_network_create); }

#include "ui_launcher.h"
static void evt_swipe_right(lv_event_t * e) {
    if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        ESP_LOGI(TAG, "Swipe Right: Back to Launcher");
        ui_launcher_show();
    }
}

// --- The Wrapped Function ---
// This replaces ui_home_create() from the BSP library to prevent the boot-up flash
void __wrap_ui_home_create(lv_obj_t * parent) {
    ESP_LOGI(TAG, "Blocked original ui_home_create to prevent boot flashing screen");
    // Do nothing - we don't want the original home screen at boot
    // optionally force background to black here if needed
}

// We define our custom create function
static void ui_board_create(lv_obj_t * parent) {
    ESP_LOGI(TAG, "Creating Custom Board Settings View");

    // Initialize global container (required by clear_current_view)
    home_cont = lv_obj_create(parent);
    lv_obj_set_size(home_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(home_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(home_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(home_cont, 0, 0);
    lv_obj_set_style_pad_all(home_cont, 20, 0);
    lv_obj_set_flex_flow(home_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(home_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Add swipe event to the container
    lv_obj_add_event_cb(home_cont, evt_swipe_right, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(home_cont, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Swipe hint icon (Upper Left)
    lv_obj_t * img_swipe = lv_image_create(home_cont);
    lv_image_set_src(img_swipe, &swipeR34);
    lv_obj_add_flag(img_swipe, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(img_swipe, LV_ALIGN_TOP_LEFT, 5, 5);

    // Title: "Board Settings"
    lv_obj_t * lbl_title = lv_label_create(home_cont);
    lv_label_set_text(lbl_title, "Board Settings");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_pad_bottom(lbl_title, 20, 0);

    // 2. Button Container (Row 1)
    lv_obj_t * btn_row1 = lv_obj_create(home_cont);
    lv_obj_set_width(btn_row1, LV_PCT(100));
    lv_obj_set_height(btn_row1, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row1, 0, 0);
    lv_obj_set_flex_flow(btn_row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row1, 8, 0);
    lv_obj_remove_flag(btn_row1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(btn_row1, LV_OBJ_FLAG_SCROLLABLE);

    create_neon_btn(btn_row1, LV_SYMBOL_CHARGE, "PM Status", lv_color_hex(0xFF3300), btn_pmic_cb);
    create_neon_btn(btn_row1, LV_SYMBOL_SETTINGS, "Set PM", lv_color_hex(0x007FFF), btn_settings_cb);
    create_neon_btn(btn_row1, LV_SYMBOL_SD_CARD, "SD Card", lv_color_hex(0x00FFFF), btn_media_cb);

    // 3. Button Container (Row 2)
    lv_obj_t * btn_row2 = lv_obj_create(home_cont);
    lv_obj_set_width(btn_row2, LV_PCT(100));
    lv_obj_set_height(btn_row2, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row2, 0, 0);
    lv_obj_set_flex_flow(btn_row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row2, 8, 0);
    lv_obj_remove_flag(btn_row2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(btn_row2, LV_OBJ_FLAG_SCROLLABLE);

    create_neon_btn(btn_row2, LV_SYMBOL_EYE_OPEN, "Display", lv_color_hex(0x39FF14), btn_display_cb);
    create_neon_btn(btn_row2, LV_SYMBOL_FILE, "System OTA", lv_color_hex(0x9D00FF), btn_sysinfo_cb);
    create_neon_btn(btn_row2, LV_SYMBOL_WIFI, "Wi-Fi", lv_color_hex(0xFF00FF), btn_ota_cb);
}

// This wrapper replaces show_home_view() from the BSP library
// Because show_home_view is called globally (unlike ui_home_create which is internal to ui_home.c)
// we can intercept it here.
void __wrap_show_home_view(lv_event_t * e) {
    (void)e;
    ESP_LOGI(TAG, "Intercepted show_home_view -> switching to Board Settings");
    request_switch(ui_board_create);
}
