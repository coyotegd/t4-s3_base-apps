#include "ui_sports.h"
#include "ui_launcher.h"
#include "esp_log.h"

static const char *TAG = "ui_sports";
static lv_obj_t *sports_screen = NULL;

static void btn_back_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Back button clicked");
        
        // Properly destroy sports screen before returning
        if (sports_screen) {
            lv_obj_del(sports_screen);
            sports_screen = NULL;
        }
        
        ui_launcher_show();
    }
}

void ui_sports_show(void) {
    ESP_LOGI(TAG, "Showing Sports app");
    
    // Clean up previous instance if exists
    if (sports_screen) {
        lv_obj_del(sports_screen);
        sports_screen = NULL;
    }
    
    // Create main container
    sports_screen = lv_obj_create(NULL);
    lv_obj_t *screen = sports_screen;
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a3300), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_screen_load(sports_screen);
    
    // Title
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, LV_SYMBOL_IMAGE " Sports App");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // Content area
    lv_obj_t *content = lv_label_create(screen);
    lv_label_set_text(content, "Sports app coming soon...\n\n"
                              "This will be a sports app.");
    lv_obj_set_style_text_font(content, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(content, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_align(content, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, -20);
    
    // Back Button
    lv_obj_t *btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, 200, 60);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(btn_back, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_back);
}
