/***************************************************
  3D Maze Game - Ported from Arduino to ESP-IDF LVGL
  Original by Joel Krueger

  Find your way out of the maze using touch screen.
  Get to the opening on the edge of the maze.

  Touch controls:
      center of touch screen -- move forward
   left side of touch screen -- turn left
  right side of touch screen -- turn right
****************************************************/

#include "ui_maze.h"
#include "ui_launcher.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "ui_maze";

// Maze game state
static lv_obj_t *maze_screen = NULL;
static lv_obj_t *render_container = NULL;
static lv_obj_t *top_bar = NULL;      // Dynamic top controls container
static lv_obj_t *content_panel = NULL; // Fills remaining space for canvas/map
static lv_obj_t *map_panel = NULL;  // Scrollable panel for map view
static lv_obj_t *map_canvas = NULL;  // Canvas inside map panel
static lv_obj_t *player_marker = NULL;  // Player position indicator on map
static lv_obj_t *stats_label = NULL;
static lv_obj_t *btn_map = NULL;
static lv_obj_t *btn_back = NULL;
static lv_obj_t *btn_mode = NULL;
static lv_obj_t *lbl_mode = NULL;
static bool showing_map = false;

// Canvas rendering with layer API (required in LVGL 9)
static void *canvas_buffer = NULL;
static void *player_marker_buffer = NULL;
static lv_layer_t layer;

static int level = 0;
static const int level_total = 3;
static const int maze_tall = 32;
static int maze_row = 8;
static uint32_t maze_col = 7;
static int facing = 0;  // 0=north 1=east 2=south 3=west
static bool suppress_throat_horiz = false;  // Hide inner top/bottom lines after stepping forward
static bool strict_occupancy_mode = true;   // Draw only occupancy-driven walls (no corner connectors)

// Top controls height (buttons + small margin)
#define TOP_CONTROLS_H 60
// Display dimensions - scaled from 320x170 to full width and remaining height
// Dynamic canvas size will be driven by content_panel size
static int canvas_w = 0;
static int canvas_h = 0;
#define CANVAS_WIDTH LV_HOR_RES
#define CANVAS_HEIGHT (LV_VER_RES - TOP_CONTROLS_H)

// Scaling macros for coordinates (320->600, 170->CANVAS_HEIGHT)
static inline int SCALE_X(int x) { return (canvas_w ? (x * canvas_w) / 320 : ((x * CANVAS_WIDTH) / 320)); }
static inline int SCALE_Y(int y) { return (canvas_h ? (y * canvas_h) / 170 : ((y * CANVAS_HEIGHT) / 170)); }

// Early forward declarations to satisfy references in size-changed handler
static void draw_3d_view(void);
static void touch_event_handler(lv_event_t *e);

// Content size change handler: reallocate canvas buffer to fill content_panel
static void content_size_changed_cb(lv_event_t *e) {
    lv_obj_t *panel = lv_event_get_target(e);
    int w = lv_obj_get_width(panel);
    int h = lv_obj_get_height(panel);
    if (w <= 0 || h <= 0) return;
    
    // Update globals used by scaling
    canvas_w = w;
    canvas_h = h;
    
    // Allocate/resize canvas buffer
    size_t buf_size = (size_t)w * (size_t)h * sizeof(lv_color_t);
    if (canvas_buffer) {
        heap_caps_free(canvas_buffer);
        canvas_buffer = NULL;
    }
    canvas_buffer = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!canvas_buffer) {
        ESP_LOGE(TAG, "Failed to allocate canvas buffer for %dx%d", w, h);
        return;
    }
    memset(canvas_buffer, 0, buf_size);
    
    if (!render_container) {
        render_container = lv_canvas_create(panel);
        lv_obj_add_flag(render_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(render_container, LV_OBJ_FLAG_SCROLLABLE);
        // Remove borders/outlines on 3D canvas
        lv_obj_set_style_border_width(render_container, 0, 0);
        lv_obj_set_style_border_opa(render_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_outline_width(render_container, 0, 0);
        lv_obj_set_style_outline_opa(render_container, LV_OPA_TRANSP, 0);
        // Handle only CLICKED to avoid duplicate actions from PRESSED+CLICKED
        lv_obj_add_event_cb(render_container, touch_event_handler, LV_EVENT_CLICKED, NULL);
    }
    lv_canvas_set_buffer(render_container, canvas_buffer, w, h, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(render_container, w, h);
    lv_obj_align(render_container, LV_ALIGN_CENTER, 0, 0);
    
    if (!showing_map) {
        draw_3d_view();
    }
}

// Colors
#define LINE_COLOR lv_color_hex(0x00FFFF)  // Cyan
#define BG_COLOR lv_color_hex(0x003030)    // Dark cyan
#define MAP_COLOR lv_color_hex(0x000070)   // Dark blue (near navy) for map
#define PERSPECTIVE_SHORTEN 10             // Shorten connectors by 10px at vanishing point

// Maze data - 32x32 bit arrays (each row is a 32-bit word, 1=wall, 0=empty)
static uint32_t maze[3][32] = {
    { // Level 1
        0b11111111111111111111111111111111,
        0b10001000000000000000000000000001,
        0b10101010101111111111111111111111,
        0b10101000000000000000000000000001,
        0b10101010101111111101111111111101,
        0b10101000000000000000100001000101,
        0b10101010101111111110101011010101,
        0b10101010100000000000101001010101,
        0b10100000001111111110101101010101,
        0b10111111111000000000101001010001,
        0b10000000000011111111101011011111,
        0b11111110111110000000101000010001,
        0b10000000000010111111101111110101,
        0b10111110111010100000001000000101,
        0b10100010100010101111111011111101,
        0b10101010101110100000000010000001,
        0b10101010101000111111111110111111,
        0b10101010101010000000000000100001,
        0b10101010101010111111111111101101,
        0b10101010101010000000000000001001,
        0b10101010101011111111111111111011,
        0b10001000101000000000001000001001,
        0b11111111101011111111101010101101,
        0b10000000001010001000101010100101,
        0b10111111111010101010101010110001,
        0b10000100011010101010101010011111,
        0b10110001010000101010100001000000,
        0b10011111010111101010111111011111,
        0b10100010010100001010000001000001,
        0b10101011110101111011111101111101,
        0b10001000000100000010000000000001,
        0b11111111111111111111111111111111
    },
    { // Level 2
        0b11111111111111111111111111111111,
        0b10010000000000001000000000000001,
        0b10111101111110111111101111111011,
        0b10100000000010000001000000010001,
        0b10111111011111010101010101010101,
        0b10000000000000010000000100000101,
        0b10111111111111111111111111111001,
        0b10001000100000000000100010001011,
        0b10101010101111111110101010101011,
        0b10100010001000000010001000100001,
        0b10111111111011111011111111111101,
        0b10000000100010001000000000000001,
        0b10111110101010101111111111111111,
        0b10000010101010100000000100010001,
        0b11111010101010111111110101010101,
        0b10000010101010100000000001000101,
        0b10111110101000101111111111111101,
        0b10000010101111111000100010000101,
        0b11111010001000000010001000100001,
        0b10000011111111111111111111111101,
        0b10111110000100000000100010000001,
        0b10000000110101111110101010111111,
        0b10111111100101000000101010000001,
        0b10000010000101011111101011111101,
        0b11111010111101000000001000000001,
        0b10001010100011111111101111111111,
        0b10101010101000000000101000010000,
        0b10101010101111111111101011010101,
        0b10100010001000000000001001000101,
        0b10111110101011111111111111111101,
        0b10000000100000000000000000000001,
        0b11111111111111111111111111111111
    },
    { // Level 3
        0b11111111111111111111111111111111,
        0b10010000000000001000000000000001,
        0b10111101111110111111101111111011,
        0b10100000000010000001000000010001,
        0b10111111011111010101010101011101,
        0b10000000000000010000000100000101,
        0b10111111111111111111111111111001,
        0b10001000100000000000100010001011,
        0b10101010101111111110101010101011,
        0b10100010001000000010001000100001,
        0b10111111111011111011111111111101,
        0b10000000100010001000000000000001,
        0b10111110101010101111111111111111,
        0b10000010101010100000000100010001,
        0b11111010101010111111110101010101,
        0b10000010101010100000000001000101,
        0b10111110101000101111111111111101,
        0b10000010101111111000100010000101,
        0b11111010001000000010001000100001,
        0b10000011111111111111111111111101,
        0b10111110000100000000100010000001,
        0b10000000110101111110101010111111,
        0b10111111100101000000101010000001,
        0b10000010000101011111101011111101,
        0b11111010111101000000001000000001,
        0b10001010100011111111101111111111,
        0b10101010101000000000101000010001,
        0b10101010101111111111101011010101,
        0b10100010101000000000001001000101,
        0b10111110101011111111111111111100,
        0b10000000100000000000000000000001,
        0b11111111111111111111111111111111
    }
};

// Forward declarations
static void draw_3d_view(void);
static void draw_map_view(void);
static void touch_event_handler(lv_event_t *e);
static void btn_map_event_cb(lv_event_t *e);
static void btn_back_event_cb(lv_event_t *e);
static void btn_mode_event_cb(lv_event_t *e);
static bool check_wall_at(int row, uint32_t col);
static void move_forward(void);
static void turn_left(void);
static void turn_right(void);
static void check_level_complete(void);
static void next_level(void);

// Helper function to check if there's a wall at a position
static bool check_wall_at(int row, uint32_t col) {
    if (row < 0 || row >= maze_tall || col >= 32) {
        return true;  // Out of bounds = wall
    }
    return (maze[level][row] & (1UL << (31 - col))) != 0;
}

// Check wall using relative offsets from player: forward (>0 toward facing), right (>0 to the right)
static inline bool check_wall_rel(int off_forward, int off_right) {
    int r = maze_row;
    int c = (int)maze_col;

    switch (facing) {
        case 0: // North
            r -= off_forward;
            c += off_right;
            break;
        case 1: // East
            r += off_right;
            c += off_forward;
            break;
        case 2: // South
            r += off_forward;
            c -= off_right;
            break;
        case 3: // West
            r -= off_right;
            c -= off_forward;
            break;
    }
    return check_wall_at(r, (uint32_t)c);
}

// Update stats label with direction and position
static void update_stats_label(void) {
    if (!stats_label) return;
    
    static char stats_buf[64];
    const char *dir_str = "?";
    
    switch (facing) {
        case 0: dir_str = "North"; break;
        case 1: dir_str = "East"; break;
        case 2: dir_str = "South"; break;
        case 3: dir_str = "West"; break;
    }
    
    snprintf(stats_buf, sizeof(stats_buf), "%s   Row: %d  Col: %lu", dir_str, maze_row + 1, (unsigned long)(maze_col + 1));
    lv_label_set_text(stats_label, stats_buf);
}

// Draw a line using layer API
static void draw_canvas_line(int x1, int y1, int x2, int y2, lv_color_t color, int width) {
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = width;
    line_dsc.p1.x = SCALE_X(x1);
    line_dsc.p1.y = SCALE_Y(y1);
    line_dsc.p2.x = SCALE_X(x2);
    line_dsc.p2.y = SCALE_Y(y2);
    lv_draw_line(&layer, &line_dsc);
}

// Draw a line shortened by `shorten_px` at the endpoint (x2,y2), pulling back toward (x1,y1)
static void draw_canvas_line_shortened_to(int x1, int y1, int x2, int y2, int shorten_px, lv_color_t color, int width) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int len = (int)sqrtf((float)(dx * dx + dy * dy));
    if (len <= 0) {
        draw_canvas_line(x1, y1, x2, y2, color, width);
        return;
    }
    int end_x = x2 - (dx * shorten_px) / len;
    int end_y = y2 - (dy * shorten_px) / len;
    draw_canvas_line(x1, y1, end_x, end_y, color, width);
}


// Draw the 3D perspective view
static void draw_3d_view(void) {
    if (!render_container) return;

    ESP_LOGI(TAG, "draw_3d_view called - pos: (%d,%lu) facing: %d", maze_row, (unsigned long)maze_col, facing);
    
    // Clear canvas
    lv_canvas_fill_bg(render_container, lv_color_black(), LV_OPA_COVER);
    
    // Initialize layer for drawing
    lv_canvas_init_layer(render_container, &layer);
    
    // Occupancy around the player based on facing direction
    bool wall_ahead      = check_wall_rel(1,  0);
    bool wall_left_near  = check_wall_rel(0, -1);
    bool wall_right_near = check_wall_rel(0,  1);
    
    // Replace perspective lines with two rectangle walls toward center (four vertical lines)
    // Opening geometry: centered around screen mid (160,85)
    const int inner_top_y = 30;     // closer to top for larger near opening
    const int inner_bottom_y = 140; // closer to bottom for larger near opening
    const int opening_h = (inner_bottom_y - inner_top_y); // visible vertical span
    const int opening_w = opening_h + 30;                 // widen by ~15px each side (adds +10 more from previous)
    const int opening_center_x = 160;                     // screen center
    const int inner_left_x = opening_center_x - (opening_w / 2);
    const int inner_right_x = opening_center_x + (opening_w / 2);

    ESP_LOGI(TAG, "Drawing wireframe based on 2D occupancy");
    ESP_LOGI(TAG, "Neighbors: ahead=%d left=%d right=%d fwd_left=%d fwd_right=%d",
             wall_ahead, wall_left_near, wall_right_near,
             check_wall_rel(1,-1), check_wall_rel(1,1));
    // Remove far side small rectangles; use classic diagonals to corners

    // Invariant: R9C8 facing North (row=8,col=7,facing=0) with open corridors left/right/ahead
    // must render inner verticals, and top/bottom horizontals extended to display edges,
    // with no horizontal segment between inner verticals. Do not change this geometry.
    bool is_r9c8n = (maze_row == 8 && maze_col == 7 && facing == 0);
    if (is_r9c8n && !wall_ahead && !wall_left_near && !wall_right_near) {
        ESP_LOGI(TAG, "R9C8N invariant view active");
    }
    // Draw corridor frame unconditionally when the path ahead is open
    if (!wall_ahead) {
        // Inner verticals (throat)
        draw_canvas_line(inner_left_x, inner_top_y, inner_left_x, inner_bottom_y, LINE_COLOR, 2);
        draw_canvas_line(inner_right_x, inner_top_y, inner_right_x, inner_bottom_y, LINE_COLOR, 2);

        // Optionally add inner top/bottom to form full throat rectangle (R9C8 view)
        if (!suppress_throat_horiz) {
            // Extend horizontals only to display edges (no segment between inner verticals)
            draw_canvas_line(0, inner_top_y, inner_left_x, inner_top_y, LINE_COLOR, 2);                 // top left extension
            draw_canvas_line(inner_right_x, inner_top_y, 320, inner_top_y, LINE_COLOR, 2);              // top right extension
            draw_canvas_line(0, inner_bottom_y, inner_left_x, inner_bottom_y, LINE_COLOR, 2);           // bottom left extension
            draw_canvas_line(inner_right_x, inner_bottom_y, 320, inner_bottom_y, LINE_COLOR, 2);        // bottom right extension
        }

        // Perspective connectors: join inner corners to screen center (vanishing point) when in Connect mode
        if (!strict_occupancy_mode) {
            const int vanish_x = 160;
            const int vanish_y = 85;
            draw_canvas_line_shortened_to(inner_left_x,  inner_top_y,    vanish_x, vanish_y, PERSPECTIVE_SHORTEN, LINE_COLOR, 2);
            draw_canvas_line_shortened_to(inner_right_x, inner_top_y,    vanish_x, vanish_y, PERSPECTIVE_SHORTEN, LINE_COLOR, 2);
            draw_canvas_line_shortened_to(inner_left_x,  inner_bottom_y, vanish_x, vanish_y, PERSPECTIVE_SHORTEN, LINE_COLOR, 2);
            draw_canvas_line_shortened_to(inner_right_x, inner_bottom_y, vanish_x, vanish_y, PERSPECTIVE_SHORTEN, LINE_COLOR, 2);
        }

        // Far-end vertical connectors at 3 and 5 steps ahead when both side walls exist
        const int vanish_x = 160;
        const int vanish_y = 85;
        // Depth factors (0 near throat, 1 at vanishing point)
        const double t3 = 0.50;  // approx 3 steps
        const double t5 = 0.75;  // approx 5 steps
        // At 3 steps ahead
        if (check_wall_rel(3, -1) && check_wall_rel(3, 1)) {
            int lx3 = (int)(inner_left_x  + t3 * (vanish_x - inner_left_x));
            int rx3 = (int)(inner_right_x + t3 * (vanish_x - inner_right_x));
            int y3_top = (int)(inner_top_y     + t3 * (vanish_y - inner_top_y));
            int y3_bot = (int)(inner_bottom_y  + t3 * (vanish_y - inner_bottom_y));
            draw_canvas_line(lx3, y3_top, lx3, y3_bot, LINE_COLOR, 2);
            draw_canvas_line(rx3, y3_top, rx3, y3_bot, LINE_COLOR, 2);
        }
        // At 5 steps ahead
        if (check_wall_rel(5, -1) && check_wall_rel(5, 1)) {
            int lx5 = (int)(inner_left_x  + t5 * (vanish_x - inner_left_x));
            int rx5 = (int)(inner_right_x + t5 * (vanish_x - inner_right_x));
            int y5_top = (int)(inner_top_y     + t5 * (vanish_y - inner_top_y));
            int y5_bot = (int)(inner_bottom_y  + t5 * (vanish_y - inner_bottom_y));
            draw_canvas_line(lx5, y5_top, lx5, y5_bot, LINE_COLOR, 2);
            draw_canvas_line(rx5, y5_top, rx5, y5_bot, LINE_COLOR, 2);
        }
    }
    
    // Near side walls: draw outer verticals only when wall is present
    if (wall_left_near) {
        draw_canvas_line(0, 0, 0, 170, LINE_COLOR, 2);
        // If corridor is open, connect edges to inner vertical to emphasize side wall
        if (!wall_ahead) {
            if (strict_occupancy_mode) {
                // Legacy connectors and horizontals for strict mode
                draw_canvas_line(0, 0, inner_left_x, inner_top_y, LINE_COLOR, 2);
                draw_canvas_line(0, 170, inner_left_x, inner_bottom_y, LINE_COLOR, 2);
                draw_canvas_line(0, inner_top_y, inner_left_x, inner_top_y, LINE_COLOR, 2);
                draw_canvas_line(0, inner_bottom_y, inner_left_x, inner_bottom_y, LINE_COLOR, 2);
            }
            // In Connect mode, trapezoid is formed by outer vertical, inner vertical, and diagonals to vanishing point
            // (diagonals are already drawn from inner corners to center above)
        }
    }
    if (wall_right_near) {
        draw_canvas_line(320, 0, 320, 170, LINE_COLOR, 2);
        if (!wall_ahead) {
            if (strict_occupancy_mode) {
                // Legacy connectors and horizontals for strict mode
                draw_canvas_line(320, 0, inner_right_x, inner_top_y, LINE_COLOR, 2);
                draw_canvas_line(320, 170, inner_right_x, inner_bottom_y, LINE_COLOR, 2);
                draw_canvas_line(inner_right_x, inner_top_y, 320, inner_top_y, LINE_COLOR, 2);
                draw_canvas_line(inner_right_x, inner_bottom_y, 320, inner_bottom_y, LINE_COLOR, 2);
            }
            // In Connect mode, trapezoid is formed by outer vertical, inner vertical, and diagonals to vanishing point
            // (diagonals are already drawn from inner corners to center above)
        }
    }

    // Turn-and-step cues removed; handled by conditional diagonals above

    // Removed small diagonal frames; extend view via corner diagonals

    // Throat continuity lines are handled above when forward is open
    
    // Far-end square removed to avoid confusion; render only actual wall faces

    // Removed conditional far-3 wall; use far-end square above for open corridor depth
    
    // Wall ahead as a large wireframe rectangle
    if (wall_ahead) {
        ESP_LOGI(TAG, "Drawing wall ahead (scaled 90%% with perspective connectors)");
        // Use explicit screen extents to avoid any mismatch
        const int screen_w = 320;
        const int screen_h = 170;
        int wall_w = (screen_w * 90) / 100;
        int wall_h = (screen_h * 90) / 100;
        int wall_x = (screen_w - wall_w) / 2;
        int wall_y = (screen_h - wall_h) / 2;
        // Wall rectangle (scaled to 90% of canvas and centered)
        draw_canvas_line(wall_x,        wall_y,        wall_x+wall_w, wall_y,        LINE_COLOR, 2); // top
        draw_canvas_line(wall_x,        wall_y+wall_h, wall_x+wall_w, wall_y+wall_h, LINE_COLOR, 2); // bottom
        draw_canvas_line(wall_x,        wall_y,        wall_x,        wall_y+wall_h, LINE_COLOR, 2); // left
        draw_canvas_line(wall_x+wall_w, wall_y,        wall_x+wall_w, wall_y+wall_h, LINE_COLOR, 2); // right
        // In Connect mode, draw perspective connectors from wall corners to vanishing point
        if (!strict_occupancy_mode) {
            const int vanish_x = 160;
            const int vanish_y = 85;
            draw_canvas_line_shortened_to(wall_x,           wall_y,           vanish_x, vanish_y, PERSPECTIVE_SHORTEN, LINE_COLOR, 2); // top-left corner
            draw_canvas_line_shortened_to(wall_x+wall_w,    wall_y,           vanish_x, vanish_y, PERSPECTIVE_SHORTEN, LINE_COLOR, 2); // top-right corner
            draw_canvas_line_shortened_to(wall_x,           wall_y+wall_h,    vanish_x, vanish_y, PERSPECTIVE_SHORTEN, LINE_COLOR, 2); // bottom-left corner
            draw_canvas_line_shortened_to(wall_x+wall_w,    wall_y+wall_h,    vanish_x, vanish_y, PERSPECTIVE_SHORTEN, LINE_COLOR, 2); // bottom-right corner
        }
    }
    
    // Side wall overlays can be added here if needed based on wall_left/wall_right
    
    // Finish layer and render to canvas
    lv_canvas_finish_layer(render_container, &layer);
    
    // Update stats display
    update_stats_label();
    
    ESP_LOGI(TAG, "draw_3d_view complete");
}

// Update player marker position on map
static void update_player_marker(void) {
    if (player_marker && showing_map) {
        int cell_size = 18;
        int player_x = maze_col * cell_size;
        int player_y = maze_row * cell_size;
        lv_obj_set_pos(player_marker, player_x, player_y);
        
        // Redraw triangle with current facing direction
        lv_canvas_fill_bg(player_marker, lv_color_hex(0x000000), LV_OPA_TRANSP);
        lv_layer_t marker_layer;
        lv_canvas_init_layer(player_marker, &marker_layer);
        
        lv_draw_triangle_dsc_t tri_dsc;
        lv_draw_triangle_dsc_init(&tri_dsc);
        tri_dsc.color = lv_color_hex(0xFF0000);
        tri_dsc.opa = LV_OPA_COVER;

        switch (facing) {
            case 0:  // North - point up
                tri_dsc.p[0].x = 9;  tri_dsc.p[0].y = 2;   // Top
                tri_dsc.p[1].x = 2;  tri_dsc.p[1].y = 16;  // Bottom-left
                tri_dsc.p[2].x = 16; tri_dsc.p[2].y = 16;  // Bottom-right
                break;
            case 1:  // East - point right
                tri_dsc.p[0].x = 16; tri_dsc.p[0].y = 9;   // Right
                tri_dsc.p[1].x = 2;  tri_dsc.p[1].y = 2;   // Top-left
                tri_dsc.p[2].x = 2;  tri_dsc.p[2].y = 16;  // Bottom-left
                break;
            case 2:  // South - point down
                tri_dsc.p[0].x = 9;  tri_dsc.p[0].y = 16;  // Bottom
                tri_dsc.p[1].x = 2;  tri_dsc.p[1].y = 2;   // Top-left
                tri_dsc.p[2].x = 16; tri_dsc.p[2].y = 2;   // Top-right
                break;
            case 3:  // West - point left
                tri_dsc.p[0].x = 2;  tri_dsc.p[0].y = 9;   // Left
                tri_dsc.p[1].x = 16; tri_dsc.p[1].y = 2;   // Top-right
                tri_dsc.p[2].x = 16; tri_dsc.p[2].y = 16;  // Bottom-right
                break;
        }

        lv_draw_triangle(&marker_layer, &tri_dsc);
        lv_canvas_finish_layer(player_marker, &marker_layer);
    }
}

// Draw the 2D map view - full 32x32 maze on scrollable panel
static void draw_map_view(void) {

    // Hide 3D canvas and show map panel
    lv_obj_add_flag(render_container, LV_OBJ_FLAG_HIDDEN);
    
    if (!map_panel) {
        // Create scrollable panel for full map under content_panel
        const int panel_h = lv_obj_get_height(content_panel);
        const int panel_w = lv_obj_get_width(content_panel);
        map_panel = lv_obj_create(content_panel);
        lv_obj_set_size(map_panel, panel_w, panel_h);
        lv_obj_align(map_panel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(map_panel, lv_color_black(), 0);
        lv_obj_set_scrollbar_mode(map_panel, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_scroll_dir(map_panel, LV_DIR_VER);
        // Remove borders/outlines on map panel
        lv_obj_set_style_border_width(map_panel, 0, 0);
        lv_obj_set_style_border_opa(map_panel, LV_OPA_TRANSP, 0);
        lv_obj_set_style_outline_width(map_panel, 0, 0);
        lv_obj_set_style_outline_opa(map_panel, LV_OPA_TRANSP, 0);
        
        // Create full-size canvas for entire 32x32 maze
        int cell_size = 18;  // Pixels per cell
        int full_map_size = 32 * cell_size;  // 576 pixels
        size_t map_buf_size = full_map_size * full_map_size * sizeof(lv_color_t);
        
        void *map_buffer = heap_caps_malloc(map_buf_size, MALLOC_CAP_SPIRAM);
        if (!map_buffer) {
            ESP_LOGE(TAG, "Failed to allocate map buffer");
            return;
        }
        memset(map_buffer, 0, map_buf_size);
        
        map_canvas = lv_canvas_create(map_panel);
        lv_canvas_set_buffer(map_canvas, map_buffer, full_map_size, full_map_size, LV_COLOR_FORMAT_RGB565);
        lv_obj_set_size(map_canvas, full_map_size, full_map_size);
        lv_obj_align(map_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_clear_flag(map_canvas, LV_OBJ_FLAG_SCROLLABLE);
        // Remove borders/outlines on map canvas
        lv_obj_set_style_border_width(map_canvas, 0, 0);
        lv_obj_set_style_border_opa(map_canvas, LV_OPA_TRANSP, 0);
        lv_obj_set_style_outline_width(map_canvas, 0, 0);
        lv_obj_set_style_outline_opa(map_canvas, LV_OPA_TRANSP, 0);
        
        // Draw entire 32x32 maze
        if (render_container) lv_obj_add_flag(render_container, LV_OBJ_FLAG_HIDDEN);
        lv_layer_t map_layer;
        lv_canvas_init_layer(map_canvas, &map_layer);

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = MAP_COLOR;
        rect_dsc.bg_opa = LV_OPA_COVER;
        rect_dsc.border_opa = LV_OPA_TRANSP;

        for (int row = 0; row < 32; row++) {
            for (uint32_t col = 0; col < 32; col++) {
                if (check_wall_at(row, col)) {
                    lv_area_t area;
                    area.x1 = (int)(col * cell_size);
                    area.y1 = (int)(row * cell_size);
                    area.x2 = area.x1 + cell_size - 2;
                    area.y2 = area.y1 + cell_size - 2;
                    lv_draw_rect(&map_layer, &rect_dsc, &area);
                }
            }
        }

        lv_canvas_finish_layer(map_canvas, &map_layer);
        
        // Create player marker as canvas with directional triangle
        int marker_size = 18;  // Match cell size
        size_t marker_buf_size = marker_size * marker_size * sizeof(lv_color_t);
        player_marker_buffer = heap_caps_malloc(marker_buf_size, MALLOC_CAP_SPIRAM);
        if (!player_marker_buffer) {
            ESP_LOGE(TAG, "Failed to allocate player marker buffer");
            return;
        }
        memset(player_marker_buffer, 0, marker_buf_size);
        
        player_marker = lv_canvas_create(map_panel);
        lv_canvas_set_buffer(player_marker, player_marker_buffer, marker_size, marker_size, LV_COLOR_FORMAT_RGB565);
        lv_obj_set_size(player_marker, marker_size, marker_size);
        lv_canvas_fill_bg(player_marker, lv_color_hex(0x000000), LV_OPA_TRANSP);  // Transparent background
        lv_obj_clear_flag(player_marker, LV_OBJ_FLAG_SCROLLABLE);
    }
    
    // Update player marker position and direction
    int cell_size = 18;
    int player_x = maze_col * cell_size;  // Align to cell
    int player_y = maze_row * cell_size;
    lv_obj_set_pos(player_marker, player_x, player_y);
    
    // Redraw triangle pointing in facing direction
    lv_canvas_fill_bg(player_marker, lv_color_hex(0x000000), LV_OPA_TRANSP);
    lv_layer_t marker_layer;
    lv_canvas_init_layer(player_marker, &marker_layer);
    
    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.color = lv_color_hex(0xFF0000);
    tri_dsc.opa = LV_OPA_COVER;

    // Triangle points based on facing direction (base width = 14, corridor width)
    switch (facing) {
        case 0:  // North - point up
            tri_dsc.p[0].x = 9;  tri_dsc.p[0].y = 2;   // Top
            tri_dsc.p[1].x = 2;  tri_dsc.p[1].y = 16;  // Bottom-left
            tri_dsc.p[2].x = 16; tri_dsc.p[2].y = 16;  // Bottom-right
            break;
        case 1:  // East - point right
            tri_dsc.p[0].x = 16; tri_dsc.p[0].y = 9;   // Right
            tri_dsc.p[1].x = 2;  tri_dsc.p[1].y = 2;   // Top-left
            tri_dsc.p[2].x = 2;  tri_dsc.p[2].y = 16;  // Bottom-left
            break;
        case 2:  // South - point down
            tri_dsc.p[0].x = 9;  tri_dsc.p[0].y = 16;  // Bottom
            tri_dsc.p[1].x = 2;  tri_dsc.p[1].y = 2;   // Top-left
            tri_dsc.p[2].x = 16; tri_dsc.p[2].y = 2;   // Top-right
            break;
        case 3:  // West - point left
            tri_dsc.p[0].x = 2;  tri_dsc.p[0].y = 9;   // Left
            tri_dsc.p[1].x = 16; tri_dsc.p[1].y = 2;   // Top-right
            tri_dsc.p[2].x = 16; tri_dsc.p[2].y = 16;  // Bottom-right
            break;
    }

    lv_draw_triangle(&marker_layer, &tri_dsc);
    lv_canvas_finish_layer(player_marker, &marker_layer);
    
    // Show map panel
    lv_obj_clear_flag(map_panel, LV_OBJ_FLAG_HIDDEN);
    
    // Scroll to player position
    int player_y_scroll = maze_row * cell_size - (CANVAS_HEIGHT / 2);
    int player_x_scroll = maze_col * cell_size - (CANVAS_WIDTH / 2);
    lv_obj_scroll_to(map_panel, player_x_scroll, player_y_scroll, LV_ANIM_ON);
}

// Movement functions
static void move_forward(void) {
    bool can_move = false;
    
    switch (facing) {
        case 0:  // north
            if (!check_wall_at(maze_row - 1, maze_col)) {
                maze_row--;
                can_move = true;
            }
            break;
        case 1:  // east
            if (!check_wall_at(maze_row, maze_col + 1)) {
                maze_col++;
                can_move = true;
            }
            break;
        case 2:  // south
            if (!check_wall_at(maze_row + 1, maze_col)) {
                maze_row++;
                can_move = true;
            }
            break;
        case 3:  // west
            if (!check_wall_at(maze_row, maze_col - 1)) {
                maze_col--;
                can_move = true;
            }
            break;
    }
    
    if (can_move) {
        // After stepping forward, suppress inner horizontals to show R8C8-style view
        suppress_throat_horiz = true;
        check_level_complete();
        if (showing_map) {
            update_player_marker();
        } else {
            draw_3d_view();
        }
    }
}

static void turn_left(void) {
    facing--;
    if (facing < 0) {
        facing = 3;
    }
    // Restore inner horizontals on turn (R9C8-style view)
    suppress_throat_horiz = false;
    if (showing_map) {
        update_player_marker();
    } else {
        draw_3d_view();
    }
}

static void turn_right(void) {
    facing++;
    if (facing > 3) {
        facing = 0;
    }
    // Restore inner horizontals on turn (R9C8-style view)
    suppress_throat_horiz = false;
    if (showing_map) {
        update_player_marker();
    } else {
        draw_3d_view();
    }
}

// Timer callback for level completion
static void level_complete_timer_cb(lv_timer_t *timer) {
    lv_obj_t *label = (lv_obj_t *)lv_timer_get_user_data(timer);
    if (label) {
        lv_obj_del(label);
    }
    next_level();
}

static void check_level_complete(void) {
    // Check if player reached the edge
    if (maze_col == 0 || maze_col == 31 || maze_row == 0 || maze_row == maze_tall - 1) {
        ESP_LOGI(TAG, "Level %d complete!", level + 1);
        
        // Flash the screen
        lv_obj_t *congrats = lv_label_create(maze_screen);
        lv_label_set_text_fmt(congrats, "LEVEL %d\nCOMPLETE!", level + 1);
        lv_obj_set_style_text_font(congrats, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(congrats, lv_color_hex(0xFFFF00), 0);  // Yellow
        lv_obj_set_style_text_align(congrats, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(congrats, LV_ALIGN_CENTER, 0, 0);
        
        // Move to next level after a delay
        lv_timer_t *timer = lv_timer_create(level_complete_timer_cb, 2000, congrats);
        lv_timer_set_repeat_count(timer, 1);
    }
}

static void next_level(void) {
    level++;
    if (level >= level_total) {
        level = 0;  // Loop back to first level
    }
    
    // Reset position
    maze_row = 8;
    maze_col = 7;
    facing = 0;
    suppress_throat_horiz = false;
    
    draw_3d_view();
}

// Touch event handler
static void touch_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    // Process only CLICKED to prevent double move/turn
    if (code != LV_EVENT_CLICKED) return;
    
    // Ignore touches when showing map view - only Back button should work
    if (showing_map) {
        return;
    }
    
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    
    ESP_LOGI(TAG, "Touch at X:%d Y:%d", point.x, point.y);
    
    // Touch screen is divided into 3 vertical sections
    // left side (x < 200) = turn left
    // right side (x > 400) = turn right  
    // center (200 <= x <= 400) = move forward
    
    if (point.x < 200) {
        turn_left();   // Left side
    } else if (point.x > 400) {
        turn_right();  // Right side
    } else {
        move_forward();  // Center
    }
}

// Map button event handler
static void btn_map_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Map button: enter map view only; hide Map button
        if (!showing_map) {
            showing_map = true;
            draw_map_view();
            if (btn_map) lv_obj_add_flag(btn_map, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Switched to map view (Map button hidden)");
        }
    }
}

// Back button event handler
static void btn_back_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (showing_map) {
            // In map view: go back to 3D perspective
            showing_map = false;
            if (map_panel) {
                lv_obj_add_flag(map_panel, LV_OBJ_FLAG_HIDDEN);
            }
            lv_obj_clear_flag(render_container, LV_OBJ_FLAG_HIDDEN);
            draw_3d_view();
            if (btn_map) lv_obj_clear_flag(btn_map, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Switched back to 3D view");
        } else {
            // In 3D view: go back to UI Launcher
            ESP_LOGI(TAG, "Exiting to launcher");
            ui_maze_cleanup();
            ui_launcher_show();
        }
    }
}

// Occupancy mode toggle button handler
static void btn_mode_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        strict_occupancy_mode = !strict_occupancy_mode;
        if (lbl_mode) {
            lv_label_set_text(lbl_mode, strict_occupancy_mode ? "Strict" : "Connect");
        }
        if (!showing_map) {
            draw_3d_view();
        }
        ESP_LOGI(TAG, "Strict occupancy mode: %s", strict_occupancy_mode ? "ON" : "OFF");
    }
}

// Cleanup function
void ui_maze_cleanup(void) {
    // Free canvas buffers
    if (canvas_buffer) {
        heap_caps_free(canvas_buffer);
        canvas_buffer = NULL;
    }
    if (player_marker_buffer) {
        heap_caps_free(player_marker_buffer);
        player_marker_buffer = NULL;
    }
    
    if (maze_screen) {
        lv_obj_del(maze_screen);
        maze_screen = NULL;
    }
    
    render_container = NULL;
    top_bar = NULL;
    content_panel = NULL;
    map_panel = NULL;
    map_canvas = NULL;
    player_marker = NULL;
    stats_label = NULL;
    btn_map = NULL;
    btn_back = NULL;
    btn_mode = NULL;
    lbl_mode = NULL;
    showing_map = false;
}

// Main show function
void ui_maze_show(void) {
    ESP_LOGI(TAG, "Showing 3D Maze game");
    
    // Clean up previous instance if exists
    ui_maze_cleanup();
    
    // Reset game state
    level = 0;
    maze_row = 8;
    maze_col = 7;
    facing = 0;
    showing_map = false;
    suppress_throat_horiz = false;
    
    // Create main container
    maze_screen = lv_obj_create(NULL);
    lv_obj_set_size(maze_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(maze_screen, lv_color_black(), 0);
    lv_obj_clear_flag(maze_screen, LV_OBJ_FLAG_SCROLLABLE);
    // Remove borders/outlines on root container
    lv_obj_set_style_border_width(maze_screen, 0, 0);
    lv_obj_set_style_border_opa(maze_screen, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_width(maze_screen, 0, 0);
    lv_obj_set_style_outline_opa(maze_screen, LV_OPA_TRANSP, 0);
    
    // Configure root as flex column to allow dynamic fill
    lv_obj_set_flex_flow(maze_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(maze_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Top bar for dynamic alignment (buttons + stats)
    top_bar = lv_obj_create(maze_screen);
    lv_obj_set_width(top_bar, lv_pct(100));
    lv_obj_set_height(top_bar, TOP_CONTROLS_H);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_left(top_bar, 5, 0);
    lv_obj_set_style_pad_right(top_bar, 5, 0);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    // Remove borders/outlines on top bar
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_border_opa(top_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_width(top_bar, 0, 0);
    lv_obj_set_style_outline_opa(top_bar, LV_OPA_TRANSP, 0);

    // Content panel fills remaining space below top bar
    content_panel = lv_obj_create(maze_screen);
    lv_obj_set_width(content_panel, lv_pct(100));
    lv_obj_set_flex_grow(content_panel, 1);
    lv_obj_set_style_bg_opa(content_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content_panel, 0, 0);
    lv_obj_clear_flag(content_panel, LV_OBJ_FLAG_SCROLLABLE);
    // Remove borders/outlines on content panel
    lv_obj_set_style_border_width(content_panel, 0, 0);
    lv_obj_set_style_border_opa(content_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_width(content_panel, 0, 0);
    lv_obj_set_style_outline_opa(content_panel, LV_OPA_TRANSP, 0);
    
    // Listen for size changes to (re)allocate canvas buffer dynamically
    lv_obj_add_event_cb(content_panel, content_size_changed_cb, LV_EVENT_SIZE_CHANGED, NULL);
    
    // Initial allocation will be handled by size-changed event after screen load
    
    // Stats label - direction and position (center of top bar)
    stats_label = lv_label_create(top_bar);
    lv_obj_set_style_text_color(stats_label, lv_color_hex(0x00FFFF), 0);  // Cyan
    lv_obj_set_style_text_font(stats_label, &lv_font_montserrat_16, 0);
    update_stats_label();
    
    // Map Button - Neon style
    btn_map = lv_btn_create(top_bar);
    lv_obj_set_size(btn_map, 100, 50);
    
    // Neon transparent style
    lv_obj_set_style_bg_opa(btn_map, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_map, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_map, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_map, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_map, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Pressed style
    lv_obj_set_style_bg_opa(btn_map, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn_map, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn_map, 20, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(btn_map, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN | LV_STATE_PRESSED);
    
    lv_obj_add_event_cb(btn_map, btn_map_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl_map = lv_label_create(btn_map);
    lv_label_set_text(lbl_map, "Map");
    lv_obj_set_style_text_color(lbl_map, lv_color_white(), 0);
    lv_obj_center(lbl_map);

    // Mode Toggle Button - Neon style
    btn_mode = lv_btn_create(top_bar);
    lv_obj_set_size(btn_mode, 120, 50);
    lv_obj_set_style_bg_opa(btn_mode, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_mode, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_mode, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_mode, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_mode, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // Pressed style
    lv_obj_set_style_bg_opa(btn_mode, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn_mode, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn_mode, 20, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(btn_mode, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_mode, btn_mode_event_cb, LV_EVENT_CLICKED, NULL);

    lbl_mode = lv_label_create(btn_mode);
    lv_label_set_text(lbl_mode, strict_occupancy_mode ? "Strict" : "Connect");
    lv_obj_set_style_text_color(lbl_mode, lv_color_white(), 0);
    lv_obj_center(lbl_mode);
    
    // Back Button - Neon style
    btn_back = lv_btn_create(top_bar);
    lv_obj_set_size(btn_back, 100, 50);
    
    // Neon transparent style
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_back, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_back, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_back, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_back, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Pressed style
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn_back, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn_back, 20, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(btn_back, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN | LV_STATE_PRESSED);
    
    lv_obj_add_event_cb(btn_back, btn_back_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_set_style_text_color(lbl_back, lv_color_white(), 0);
    lv_obj_center(lbl_back);
    
    // Load screen; size event will allocate and draw 3D view
    lv_screen_load(maze_screen);
}
