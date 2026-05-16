/**
 * @file fonts.h
 * @brief Font definitions used by the ST7789 text drawing functions.
 *
 * This file declares the font metadata structure and the available font
 * instances used by the display driver.
 */

#ifndef FONTS_H_
#define FONTS_H_

#include <stdint.h>

/**
 * @brief Storage format used by a font table.
 */
typedef enum
{
    FONT_STORAGE_16 = 0, /**< Font rows stored as 16-bit values. */
    FONT_STORAGE_32      /**< Font rows stored as 32-bit values. */
} FontStorage_t;

/**
 * @brief Font metadata used by the drawing functions.
 */
typedef struct
{
    uint8_t width;           /**< Character width in pixels. */
    uint8_t height;          /**< Character height in pixels. */
    FontStorage_t storage;   /**< Font row storage format. */
    const void *data;        /**< Pointer to the font bitmap table. */
} FontDef_t;

/** @brief 8x12 pixel font. */
extern const FontDef_t Font_8x12;

/** @brief 12x20 pixel font. */
extern const FontDef_t Font_12x20;

/** @brief 16x26 pixel font. */
extern const FontDef_t Font_16x26;

/** @brief 20x32 pixel font. */
extern const FontDef_t Font_20x32;

#endif /* FONTS_H_ */
