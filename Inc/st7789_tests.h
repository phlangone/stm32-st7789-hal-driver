/**
 * @file st7789_tests.h
 * @brief Optional display validation helpers for the ST7789 driver.
 *
 * Include this header only when ST7789_ENABLE_TESTS is enabled in
 * st7789_conf.h. These functions are intended for display bring-up,
 * wiring validation and quick visual checks.
 */

#ifndef ST7789_TESTS_H_
#define ST7789_TESTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "st7789.h"

#if (ST7789_ENABLE_TESTS != 0)

/**
 * @brief Draws vertical RGB565 color bars across the display.
 * @return HAL_OK on success, otherwise the first drawing error.
 */
HAL_StatusTypeDef ST7789_Test_ColorBars(void);

/**
 * @brief Draws rectangles, circles and triangles to validate primitives.
 * @return HAL_OK on success, otherwise the first drawing error.
 */
HAL_StatusTypeDef ST7789_Test_Shapes(void);

/**
 * @brief Cycles through the four supported rotations and draws a border mark.
 * @return HAL_OK on success, otherwise the first drawing error.
 */
HAL_StatusTypeDef ST7789_Test_Rotation(void);

/**
 * @brief Draws a centered text message using the selected font.
 * @param font Pointer to a font definition.
 * @return HAL_OK on success, otherwise the first drawing error.
 */
HAL_StatusTypeDef ST7789_Test_Text(const FontDef_t *font);

/**
 * @brief Runs color, shape, rotation and optional text tests.
 * @param font Pointer to a font definition. Pass NULL to skip the text test.
 * @return HAL_OK on success, otherwise the first drawing error.
 */
HAL_StatusTypeDef ST7789_Test_Full(const FontDef_t *font);

#endif /* ST7789_ENABLE_TESTS */

#ifdef __cplusplus
}
#endif

#endif /* ST7789_TESTS_H_ */
