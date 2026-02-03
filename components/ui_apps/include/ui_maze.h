#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maze constants
#define MAZE_SIZE 32
#define LEVEL_COUNT 3

/**
 * @brief Show the Maze app screen
 */
void ui_maze_show(void);

/**
 * @brief Clean up maze resources
 */
void ui_maze_cleanup(void);

#ifdef __cplusplus
}
#endif
