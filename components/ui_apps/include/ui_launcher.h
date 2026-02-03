#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the launcher screen (app homepage)
 * Shows buttons for: Maze, Sports, Settings
 */
void ui_launcher_init(void);

/**
 * @brief Destroy the launcher screen and free all resources
 */
void ui_launcher_destroy(void);

/**
 * @brief Show the launcher screen
 * Used to return from apps to the home screen
 */
void ui_launcher_show(void);

#ifdef __cplusplus
}
#endif
