#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Show the Weather app screen
 */
void ui_weather_show(void);

/**
 * @brief Clean up weather resources
 */
void ui_weather_cleanup(void);

#ifdef __cplusplus
}
#endif
