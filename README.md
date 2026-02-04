# t4-s3_base-apps

ESP32-S3 applications using the t4-s3_hal_bsp-lvgl base components.

## 🚀 Quick Start for Beginners

### Prerequisites

1. **Install VS Code** - [Download here](https://code.visualstudio.com/)
2. **Install ESP-IDF Extension** in VS Code:
   - Open VS Code
   - Press `Ctrl+Shift+X` (Extensions)
   - Search for "ESP-IDF"
   - Install "Espressif IDF" extension
   - Follow the extension's setup wizard to install ESP-IDF v5.5

### Clone & Setup

**⚠️ CRITICAL: Use `--recursive` when cloning!**

This project uses **Git Submodules** - it depends on the [t4-s3_hal_bsp-lvgl](https://github.com/coyotegd/t4-s3_hal_bsp-lvgl) library which is automatically downloaded with the `--recursive` flag.

```bash
git clone --recursive https://github.com/coyotegd/t4-s3_base-apps.git
cd t4-s3_base-apps
```

**What does `--recursive` do?**
- Downloads this main project
- Automatically downloads the `t4-s3_hal_bsp-lvgl` submodule into `external/hal_bsp/`
- Without it, `external/hal_bsp/` will be empty and the build will fail

**Already cloned without `--recursive`? No problem:**

```bash
cd t4-s3_base-apps
git submodule update --init --recursive
```

### Build & Flash

1. **Open in VS Code:**
   ```bash
   code t4-s3_base-apps
   ```

2. **Reload Window** (to activate ESP-IDF extension):
   - Press `Ctrl+Shift+P`
   - Type "Developer: Reload Window"
   - Press Enter

3. **Build the Project:**
   - Click the **Build** button in the status bar (bottom of VS Code)
   - Or press `F1` → "ESP-IDF: Build your project"
   - Or use terminal: `idf.py build`

4. **Connect your ESP32-S3 board** via USB

5. **Flash to Device:**
   - Click the **Flash** button in status bar
   - Select your serial port when prompted (e.g., `/dev/ttyUSB0` or `COM3`)
   - Or use terminal: `idf.py -p /dev/ttyUSB0 flash monitor`

6. **Monitor Output:**
   - Click the **Monitor** button in status bar
   - Or the flash command above includes monitoring

## �️ Development Tools

### ESP-IDF Helper Script (`idfsh`)

This project includes an interactive helper for common ESP-IDF tasks. The script is located in the **hal_bsp submodule** (`external/hal_bsp/tools/idfsh.sh`) and is available via symlink at `tools/idfsh.sh`.

**Usage:**
```bash
# From project root
source tools/idfsh.sh
idfsh
```

**Features:**
- Interactive menu for build/flash/monitor
- Aggressive full clean (removes `build/` and `managed_components/`)
- Ad-hoc port/baud selection
- Enhanced size report with color-coded status
- CMake reconfigure
- Chip erase with confirmation

See `tools/README.md` for full documentation.

**Note:** After updating the hal_bsp submodule, the tools will be sourced directly from `external/hal_bsp/tools/`. Currently, a local copy is provided for convenience.

## �🔧 Workspace Configuration

This project uses **fully portable settings** - no hardcoded paths!

**What's included in `.vscode/settings.json`:**
- Workspace-relative paths only (`${workspaceFolder}`)
- Project-specific settings (target: ESP32-S3)
- IntelliSense configuration

**What's auto-configured:**
- ESP-IDF installation path
- Python environment
- Toolchain paths
- Serial port (you select when flashing)

After cloning, just open in VS Code and everything works! No manual path configuration needed.

## 📁 Project Structure

```
t4-s3_base-apps/
├── external/
│   └── hal_bsp/          ← t4-s3_hal_bsp-lvgl submodule (HAL, BSP, LVGL)
├── components/
│   └── ui_apps/          ← Custom UI applications (maze game, launcher, etc.)
├── main/
│   ├── main.c            ← Application entry point
│   └── ui_board_settings.c ← Custom home screen (wraps HAL BSP home)
├── .vscode/              ← VS Code settings (portable)
├── CMakeLists.txt        ← Project build configuration
├── partitions.csv        ← Flash partition table
├── sdkconfig.defaults    ← Default ESP-IDF configuration
└── README.md             ← This file
```

## 🎮 What's Included

- **3D Maze Game** - Canvas-based 3D maze with LVGL 9
- **Application Launcher** - Main menu for apps
- **Board Settings** - Custom home screen (replaces HAL BSP home)
- **HAL BSP Integration** - Full hardware abstraction (display, touch, power)
- **LVGL 9.2** - Modern UI framework with canvas rendering

## Development Notes

### Customizing HAL BSP UI (Linker Wrap Trick)

This project uses a GCC linker feature called "wrapping" to replace the original library's Home screen with our own custom "Board Settings" page, without modifying the underlying library source code.

**The Concept: An Independent Hub**

We have created an **independent hub** (`main/ui_board_settings.c`) that serves as the new center of the application's settings.

*   **Patterned after the Original:** It duplicates the style and button logic of the original library home page but removes unwanted elements (like date, time, and images).

*   **Fully Independent:** The original `ui_home.c` file is completely ignored for drawing the screen.

    1. **It is defined in `hal_bsp`**
    In generic `t4-s3_hal_bsp-lvgl` code (specifically in `ui_home.c`), `show_home_view` is just a helper function that handles the event to switch navigation.

    2. **It is overridden in your Project**
    Crucially, your project **intercepts** this function. In `main/CMakeLists.txt`, there is a linker flag: `"-Wl,--wrap=show_home_view"`. This tells the compiler: "Whenever anyone calls the function `show_home_view`, call `__wrap_show_home_view` instead." See, components/ui_apps/CmakeLists.txt:

target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=show_home_view" "-Wl,--wrap=ui_home_create")

*   **Connected Ecosystem:** Even though the hub is new, its buttons still link to the complex, existing pages of the HAL BSP library (PMIC, Display, System Info), so we don't have to rewrite that functionality.

**How it works:**

1.  In `main/CMakeLists.txt`, we pass a flag to the linker: `-Wl,--wrap=show_home_view`.
2.  This behaves like a "switcheroo". It tells the build system: "Whenever any part of the app tries to call
    `show_home_view` (to go home), call `__wrap_show_home_view` instead."
3.  We implement `__wrap_show_home_view` in `main/ui_board_settings.c`, which launches our custom independent hub
    instead of the original home page ui_home (hub) in t4-s3_hal_bsp-lvgl.

**Key Files:**

*   `main/ui_board_settings.c`: The new independent hub implementation.
*   `main/CMakeLists.txt`: Contains the `target_link_libraries` configuration with the `--wrap` option.

**Why do this?**

*   It allows us to keep the `external/hal_bsp` submodule clean and unmodified.
*   We can update the submodule in the future without losing our UI customizations.
*   Global calls (like "Back" gestures from other screens) automatically route to our custom screen.

---

# UI Apps - Canvas-Based Rendering with LVGL 9

## Overview

The UI applications in this project use **canvas-based rendering** with LVGL 9's layer API. The maze game (`ui_maze.c`) was converted from widget-based rendering to canvas drawing for better performance and memory efficiency.

## Critical LVGL 9 Requirements

### 1. Canvas Layer API is Mandatory

**LVGL 9 has NO direct canvas drawing functions.** Functions like `lv_canvas_draw_line()` and `lv_canvas_draw_rect()` **do not exist**.

You MUST use the layer API:

```c
lv_layer_t layer;

// Initialize layer before any drawing
lv_canvas_init_layer(canvas, &layer);

// Draw using lv_draw_* functions with &layer
lv_draw_line(&layer, &line_dsc);
lv_draw_rect(&layer, &rect_dsc, &area);

// Finish layer to commit drawing to canvas
lv_canvas_finish_layer(canvas, &layer);
```

### 2. Widget Size Must Be Set Explicitly

After calling `lv_canvas_set_buffer()`, you **MUST** call `lv_obj_set_size()`:

```c
lv_canvas_set_buffer(canvas, buffer, width, height, LV_COLOR_FORMAT_RGB565);
lv_obj_set_size(canvas, width, height);  // CRITICAL - without this, clip_area is (0,0) to (0,0)
```

**Without this**, the layer's `clip_area` remains (0,0) to (0,0), and nothing will render.

## Memory Configuration

### PSRAM Allocation

The canvas buffer (600×376×2 bytes = 451,200 bytes) is allocated from PSRAM:

```c
canvas_buffer = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
```

### LVGL Memory Configuration

**Required in `sdkconfig`:**

```
CONFIG_LV_USE_CLIB_MALLOC=y
```

This routes LVGL's internal allocations through ESP-IDF's heap allocator, which has PSRAM access. The default `CONFIG_LV_USE_BUILTIN_MALLOC` uses a 64KB internal pool that cannot access PSRAM.

## Coordinate Scaling

The original maze game was designed for **320×170** resolution. The T4-S3 display is **600×446**, with 70 pixels reserved for buttons, giving a **600×376** canvas.

### Scaling Macros

```c
#define CANVAS_WIDTH 600
#define CANVAS_HEIGHT 376  // LV_VER_RES (446) - 70 for buttons

#define SCALE_X(x) (((x) * CANVAS_WIDTH) / 320)
#define SCALE_Y(y) (((y) * CANVAS_HEIGHT) / 170)
```

All drawing coordinates from the original 320×170 game are scaled using these macros.

## Drawing Pattern

### 3D View Rendering

```c
static void draw_3d_view(void) {
    // 1. Clear canvas
    lv_canvas_fill_bg(render_container, lv_color_black(), LV_OPA_COVER);
    
    // 2. Initialize layer
    lv_canvas_init_layer(render_container, &layer);
    
    // 3. Draw perspective lines (creates tunnel effect)
    draw_canvas_line(0, 0, 130, 60, LINE_COLOR, 2);
    draw_canvas_line(320, 0, 190, 60, LINE_COLOR, 2);
    draw_canvas_line(0, 170, 130, 110, LINE_COLOR, 2);
    draw_canvas_line(320, 170, 190, 110, LINE_COLOR, 2);
    
    // 4. Draw walls based on maze state
    if (wall_ahead) {
        draw_canvas_rect(20, 9, 280, 152, lv_color_hex(0x808080));
    }
    
    // 5. Finish layer (commits drawing)
    lv_canvas_finish_layer(render_container, &layer);
}
```

### Helper Functions

```c
static void draw_canvas_line(int x1, int y1, int x2, int y2, lv_color_t color, int width) {
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = width;
    line_dsc.p1.x = SCALE_X(x1);
    line_dsc.p1.y = SCALE_Y(y1);
    line_dsc.p2.x = SCALE_X(x2);
    line_dsc.p2.y = SCALE_Y(y2);
    lv_draw_line(&layer, &line_dsc);  // Note: &layer parameter
}
```

## Common Pitfalls

### ❌ Problem: Blank Screen Despite Layer API Calls

**Cause:** Widget size not set after `lv_canvas_set_buffer()`

**Solution:** Always call `lv_obj_set_size()` immediately after `lv_canvas_set_buffer()`

### ❌ Problem: Memory Allocation Failures

**Cause:** LVGL using `CONFIG_LV_USE_BUILTIN_MALLOC` with 64KB internal pool

**Solution:** Change to `CONFIG_LV_USE_CLIB_MALLOC` in `sdkconfig`

### ❌ Problem: Incorrect Coordinate Dimensions

**Symptoms:** 
- Canvas height hardcoded to 450 when actual is 376
- Scaling dividing by 380 instead of 376

**Solution:** Use consistent dimensions everywhere:
```c
const int canvas_height = LV_VER_RES - 70;  // 446 - 70 = 376
#define CANVAS_HEIGHT 376
#define SCALE_Y(y) (((y) * 376) / 170)  // Not 380, not 450
```

### ❌ Problem: Trying to Use Non-Existent Functions

**Don't use (these don't exist in LVGL 9):**
- `lv_canvas_draw_line()`
- `lv_canvas_draw_rect()`
- `lv_canvas_draw_arc()`

**Use instead:**
```c
lv_canvas_init_layer(canvas, &layer);
lv_draw_line(&layer, &line_dsc);      // Note &layer
lv_draw_rect(&layer, &rect_dsc, &area);
lv_canvas_finish_layer(canvas, &layer);
```

## Maze Game Features

### Controls
- **Touch left side:** Turn left
- **Touch right side:** Turn right
- **Touch center:** Move forward
- **Map button:** Toggle between 3D view and top-down map
- **Back button:** Return to launcher (or 3D view from map)

### Visual Elements
- **Cyan perspective lines:** Create 3D tunnel depth effect
- **Gray rectangles:** Walls directly ahead
- **Trapezoid outlines:** Side walls (left/right)
- **Smaller gray rect:** Distant wall (2 spaces ahead)
- **Red triangle (map view):** Player position and facing direction
- **Cyan cells (map view):** Maze walls

## Performance Notes

- Canvas rendering uses ~450KB PSRAM for pixel buffer
- All drawing operations go through LVGL's layer system
- No widget tree overhead - single canvas widget
- Efficient for complex graphics with many primitives
- PSRAM access is slower than internal RAM but necessary for large buffers

## UI Application Files

- `components/ui_apps/src/ui_maze.c` - 3D maze game with canvas rendering
- `components/ui_apps/include/ui_maze.h` - Public API
- `components/ui_apps/src/ui_launcher.c` - Main launcher screen
- `components/ui_apps/src/ui_sports.c` - Sports app (placeholder)
- `components/ui_apps/src/ui_board_settings.c` - System settings

## Build Requirements

- ESP-IDF v5.5+
- LVGL 9.2.2+
- ESP32-S3 with PSRAM
- Display: 600×446 resolution

## References

- [LVGL Canvas Documentation](https://docs.lvgl.io/master/widgets/canvas.html)
- [LVGL Layer API](https://docs.lvgl.io/master/overview/layer.html)
- ESP-IDF heap_caps documentation for PSRAM allocation

