/**
 * @file st7789.h
 * @brief Public API for the ST7789 TFT display driver.
 *
 * This file exposes the public interface of a handle-free ST7789 driver for
 * STM32 projects. The driver supports SPI or 8-bit 8080 parallel interfaces,
 * display initialization, raw command/data transfers, drawing primitives, text
 * rendering, RGB565 and monochrome bitmaps, optional DMA helpers and optional
 * test routines.
 *
 * Hardware options, pins, dimensions, offsets and optional features are defined
 * in @ref st7789_conf.h.
 *
 * @note The application is expected to provide the HAL configuration generated
 *       by STM32CubeMX, including the selected SPI peripheral when SPI mode is
 *       used.
 */

#ifndef ST7789_H_
#define ST7789_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "fonts.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup ST7789 ST7789 TFT Display Driver
 * @brief Public API, types and constants for the ST7789 display driver.
 * @{
 */

/** @name Controller command definitions
 *  @{
 */

/** @defgroup ST7789_Commands ST7789 controller commands
 *  @brief Command codes sent to the display controller.
 *  @{
 */
#define ST7789_CMD_NOP        0x00U
#define ST7789_CMD_SWRESET    0x01U
#define ST7789_CMD_SLPIN      0x10U
#define ST7789_CMD_SLPOUT     0x11U
#define ST7789_CMD_NORON      0x13U
#define ST7789_CMD_INVOFF     0x20U
#define ST7789_CMD_INVON      0x21U
#define ST7789_CMD_DISPOFF    0x28U
#define ST7789_CMD_DISPON     0x29U
#define ST7789_CMD_CASET      0x2AU
#define ST7789_CMD_RASET      0x2BU
#define ST7789_CMD_RAMWR      0x2CU
#define ST7789_CMD_MADCTL     0x36U
#define ST7789_CMD_COLMOD     0x3AU
/** @}
 */
/** @}
 */

/** @name MADCTL register definitions
 *  @{
 */

/** @defgroup ST7789_MADCTL MADCTL register bits
 *  @brief Masks used for rotation, mirroring and color order.
 *  @{
 */
#define ST7789_MADCTL_MY      0x80U
#define ST7789_MADCTL_MX      0x40U
#define ST7789_MADCTL_MV      0x20U
#define ST7789_MADCTL_ML      0x10U
#define ST7789_MADCTL_RGB     0x00U
#define ST7789_MADCTL_BGR     0x08U
#define ST7789_MADCTL_MH      0x04U
/**
 * @brief MADCTL value for rotation 0 degrees.
 *
 * Adjust these macros if your ST7789 module appears mirrored or rotated.
 * The color order is added at runtime from st7789.madctl_color_order.
 */
#define ST7789_ROTATION_0_MADCTL      (ST7789_MADCTL_MY)
#define ST7789_ROTATION_90_MADCTL     (ST7789_MADCTL_MV | ST7789_MADCTL_MX)
#define ST7789_ROTATION_180_MADCTL    (ST7789_MADCTL_MX | ST7789_MADCTL_MY)
#define ST7789_ROTATION_270_MADCTL    (ST7789_MADCTL_MV | ST7789_MADCTL_MY)
/** @}
 */
/** @}
 */

/** @name Color mode definitions
 *  @{
 */

/** @defgroup ST7789_ColorModes Color modes
 *  @brief Values accepted by the COLMOD register.
 *  @{
 */
#define ST7789_COLOR_MODE_12BIT   0x03U
#define ST7789_COLOR_MODE_16BIT   0x55U
#define ST7789_COLOR_MODE_18BIT   0x66U
/** @}
 */
/** @}
 */

/** @name Color helpers
 *  @{
 */

/**
 * @brief Converts 8-bit RGB components to RGB565 format.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 */
#define ST7789_COLOR565(r, g, b) \
    (uint16_t)((((uint16_t)(r) & 0xF8U) << 8) | \
               (((uint16_t)(g) & 0xFCU) << 3) | \
               (((uint16_t)(b) & 0xF8U) >> 3))
/** @}
 */

/** @name Public types
 *  @{
 */

/**
 * @brief Supported display orientations.
 */
typedef enum
{
    ST7789_ROTATION_0 = 0,    /**< Default 0-degree rotation. */
    ST7789_ROTATION_90,       /**< 90-degree rotation. */
    ST7789_ROTATION_180,      /**< 180-degree rotation. */
    ST7789_ROTATION_270       /**< 270-degree rotation. */
} ST7789_Rotation_t;
/** @}
 */

/** @name Configuration include and compile-time validation
 *  @{
 */

#include "st7789_conf.h"

#ifndef ST7789_INIT_GPIO
#define ST7789_INIT_GPIO 0
#endif

#ifndef ST7789_USE_DMA
#define ST7789_USE_DMA 0
#endif

#if !defined(HAL_SPI_MODULE_ENABLED)
typedef struct __SPI_HandleTypeDef SPI_HandleTypeDef;
#endif

#if !defined(ST7789_TX_CHUNK_SIZE) || (ST7789_TX_CHUNK_SIZE < 2U)
#error "ST7789_TX_CHUNK_SIZE deve ser pelo menos 2."
#endif

#if (ST7789_INTERFACE != ST7789_INTERFACE_SPI) && (ST7789_INTERFACE != ST7789_INTERFACE_PARALLEL)
#error "ST7789_INTERFACE deve ser ST7789_INTERFACE_SPI ou ST7789_INTERFACE_PARALLEL."
#endif
/** @}
 */

/** @name Data structures
 *  @{
 */

/**
 * @brief Item of an ST7789 initialization command list.
 */
typedef struct
{
    uint8_t command;          /**< Command sent to the controller. */
    const uint8_t *data;      /**< Data associated with the command. */
    uint8_t length;           /**< Number of bytes in data. */
    uint16_t delay_ms_after;  /**< Delay after sending the command, in milliseconds. */
} ST7789_InitCmd_t;

/**
 * @brief Monochrome bitmap with one bit per pixel.
 */
typedef struct
{
    uint16_t width;       /**< Bitmap width in pixels. */
    uint16_t height;      /**< Bitmap height in pixels. */
    const uint8_t *data;  /**< Bitmap data organized by rows. */
} ST7789_BitmapMono_t;

/**
 * @brief RGB565 color bitmap.
 */
typedef struct
{
    uint16_t width;          /**< Bitmap width in pixels. */
    uint16_t height;         /**< Bitmap height in pixels. */
    const uint16_t *data;    /**< Pixels in RGB565 format. */
} ST7789_BitmapRGB565_t;
/** @}
 */

/** @name Initialization and display control API
 *  @{
 */

/**
 * @brief Initializes the display using the default driver configuration.
 */
HAL_StatusTypeDef ST7789_Init(void);
/**
 * @brief Initializes the display and runs an optional list of extra commands.
 * @param extra_cmds Additional command list.
 * @param extra_count Number of additional commands.
 */
HAL_StatusTypeDef ST7789_InitEx(const ST7789_InitCmd_t *extra_cmds, size_t extra_count);
/**
 * @brief Runs an initialization or configuration command sequence.
 * @param cmds Command list.
 * @param count Number of items in the list.
 */
HAL_StatusTypeDef ST7789_RunCommandList(const ST7789_InitCmd_t *cmds, size_t count);
/**
 * @brief Performs a hardware reset when the RST pin is configured.
 */
HAL_StatusTypeDef ST7789_Reset(void);
/** @brief Turns the display on. */
HAL_StatusTypeDef ST7789_DisplayOn(void);
/** @brief Turns the display off. */
HAL_StatusTypeDef ST7789_DisplayOff(void);
/**
 * @brief Enables or disables display color inversion.
 * @param enable true to invert colors; false to use normal colors.
 */
HAL_StatusTypeDef ST7789_InvertDisplay(bool enable);
/**
 * @brief Controls the backlight when the BL pin is configured.
 * @param enable true to turn it on; false to turn it off.
 */
HAL_StatusTypeDef ST7789_SetBacklight(bool enable);
/**
 * @brief Sets the active graphics area rotation.
 * @param rotation Desired orientation.
 */
HAL_StatusTypeDef ST7789_SetRotation(ST7789_Rotation_t rotation);
/** @}
 */

/** @name Display state getters
 *  @{
 */

/** @brief Returns the current graphics area width, considering rotation. */
uint16_t ST7789_GetWidth(void);
/** @brief Returns the current graphics area height, considering rotation. */
uint16_t ST7789_GetHeight(void);
/** @brief Returns the currently configured rotation. */
ST7789_Rotation_t ST7789_GetRotation(void);
/** @}
 */

/** @name Low-level transfer API
 *  @{
 */

/**
 * @brief Sends a raw command to the ST7789 controller.
 * @param command Command code.
 */
HAL_StatusTypeDef ST7789_WriteCommand(uint8_t command);
/**
 * @brief Sends raw data to the ST7789 controller.
 * @param data Pointer to the data buffer.
 * @param length Number of bytes.
 */
HAL_StatusTypeDef ST7789_WriteData(const uint8_t *data, size_t length);
/**
 * @brief Sends raw data using DMA when available.
 * @param data Pointer to the data buffer.
 * @param length Number of bytes.
 */
HAL_StatusTypeDef ST7789_WriteDataDMA(const uint8_t *data, uint16_t length);
/**
 * @brief Sends a command followed by optional data.
 * @param command Command code.
 * @param data Pointer to the data buffer.
 * @param length Number of bytes in data.
 */
HAL_StatusTypeDef ST7789_WriteCommandData(uint8_t command, const uint8_t *data, size_t length);
/**
 * @brief Sets the display memory window for pixel writing.
 * @param x0 Start X coordinate.
 * @param y0 Start Y coordinate.
 * @param x1 End X coordinate.
 * @param y1 End Y coordinate.
 */
HAL_StatusTypeDef ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
/** @}
 */

/** @name Basic drawing API
 *  @{
 */

/**
 * @brief Draws one pixel on the screen.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
/**
 * @brief Draws a horizontal line.
 * @param x Start X coordinate.
 * @param y Y coordinate.
 * @param w Line width in pixels.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_DrawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
/**
 * @brief Draws a vertical line.
 * @param x X coordinate.
 * @param y Start Y coordinate.
 * @param h Line height in pixels.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_DrawFastVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
/**
 * @brief Draws a rectangle outline.
 * @param x Start X coordinate.
 * @param y Start Y coordinate.
 * @param w Width in pixels.
 * @param h Height in pixels.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
/**
 * @brief Fills a rectangular area with one color.
 * @param x Start X coordinate.
 * @param y Start Y coordinate.
 * @param w Width in pixels.
 * @param h Height in pixels.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
/**
 * @brief Fills a rectangular area using DMA when available.
 * @param x Start X coordinate.
 * @param y Start Y coordinate.
 * @param w Width in pixels.
 * @param h Height in pixels.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_FillRectDMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
/**
 * @brief Fills the entire screen with one color.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_FillScreen(uint16_t color);
/**
 * @brief Fills the entire screen using DMA when available.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_FillScreenDMA(uint16_t color);
/** @}
 */

/** @name Shape drawing API
 *  @{
 */

/**
 * @brief Draws a circle outline.
 * @param x0 Circle center X coordinate.
 * @param y0 Circle center Y coordinate.
 * @param r Circle radius in pixels.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
/**
 * @brief Draws a filled circle.
 * @param x0 Circle center X coordinate.
 * @param y0 Circle center Y coordinate.
 * @param r Circle radius in pixels.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_FillCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
/**
 * @brief Draws a triangle outline.
 * @param x0 First vertex X coordinate.
 * @param y0 First vertex Y coordinate.
 * @param x1 Second vertex X coordinate.
 * @param y1 Second vertex Y coordinate.
 * @param x2 Third vertex X coordinate.
 * @param y2 Third vertex Y coordinate.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_DrawTriangle(uint16_t x0,
                                      uint16_t y0,
                                      uint16_t x1,
                                      uint16_t y1,
                                      uint16_t x2,
                                      uint16_t y2,
                                      uint16_t color);
/**
 * @brief Fills a triangle.
 * @param x0 First vertex X coordinate.
 * @param y0 First vertex Y coordinate.
 * @param x1 Second vertex X coordinate.
 * @param y1 Second vertex Y coordinate.
 * @param x2 Third vertex X coordinate.
 * @param y2 Third vertex Y coordinate.
 * @param color RGB565 color.
 */
HAL_StatusTypeDef ST7789_FillTriangle(uint16_t x0,
                                      uint16_t y0,
                                      uint16_t x1,
                                      uint16_t y1,
                                      uint16_t x2,
                                      uint16_t y2,
                                      uint16_t color);
/** @}
 */

/** @name Image drawing API
 *  @{
 */

/**
 * @brief Writes an RGB565 image to a screen region.
 * @param x Start X coordinate.
 * @param y Start Y coordinate.
 * @param w Image width.
 * @param h Image height.
 * @param image Pointer to RGB565 pixels.
 */
HAL_StatusTypeDef ST7789_WriteImageRGB565(uint16_t x,
                                          uint16_t y,
                                          uint16_t w,
                                          uint16_t h,
                                          const uint16_t *image);
/**
 * @brief Writes an RGB565 image using DMA when available.
 * @param x Start X coordinate.
 * @param y Start Y coordinate.
 * @param w Image width.
 * @param h Image height.
 * @param image Pointer to RGB565 pixels.
 */
HAL_StatusTypeDef ST7789_WriteImageRGB565DMA(uint16_t x,
                                             uint16_t y,
                                             uint16_t w,
                                             uint16_t h,
                                             const uint16_t *image);
/** @}
 */

/** @name Text and bitmap API
 *  @{
 */

/**
 * @brief Returns the character width for the given font.
 * @param font Pointer to the font definition.
 */
uint16_t ST7789_CharWidth(const FontDef_t *font);
/**
 * @brief Returns the character height for the given font.
 * @param font Pointer to the font definition.
 * @return Character height in pixels, or 0 when font is NULL.
 */
uint16_t ST7789_CharHeight(const FontDef_t *font);
/**
 * @brief Calculates the width of a single text line.
 * @param font Pointer to the font definition.
 * @param text Null-terminated text string.
 * @param spacing Extra spacing in pixels inserted between characters.
 * @return Text width in pixels, or 0 when font or text is NULL.
 */
uint16_t ST7789_TextWidth(const FontDef_t *font, const char *text, uint16_t spacing);

/**
 * @brief Draws a single character.
 * @param x Character origin X coordinate.
 * @param y Character origin Y coordinate.
 * @param ch Character to draw.
 * @param font Pointer to the font definition.
 * @param fg Foreground RGB565 color.
 * @param bg Background RGB565 color.
 * @param transparent true to keep background pixels unchanged.
 * @return HAL_OK on success, otherwise an error status.
 */
HAL_StatusTypeDef ST7789_DrawChar(uint16_t x,
                                  uint16_t y,
                                  char ch,
                                  const FontDef_t *font,
                                  uint16_t fg,
                                  uint16_t bg,
                                  bool transparent);
/**
 * @brief Draws a null-terminated text string.
 * @param x Text origin X coordinate.
 * @param y Text origin Y coordinate.
 * @param text Null-terminated text string.
 * @param font Pointer to the font definition.
 * @param fg Foreground RGB565 color.
 * @param bg Background RGB565 color.
 * @param transparent true to keep background pixels unchanged.
 * @param spacing Extra spacing in pixels inserted between characters.
 * @return HAL_OK on success, otherwise an error status.
 */
HAL_StatusTypeDef ST7789_DrawText(uint16_t x,
                                  uint16_t y,
                                  const char *text,
                                  const FontDef_t *font,
                                  uint16_t fg,
                                  uint16_t bg,
                                  bool transparent,
                                  uint16_t spacing);
/**
 * @brief Draws a text string centered inside a rectangular area.
 * @param x Rectangle origin X coordinate.
 * @param y Rectangle origin Y coordinate.
 * @param w Rectangle width in pixels.
 * @param h Rectangle height in pixels.
 * @param text Null-terminated text string.
 * @param font Pointer to the font definition.
 * @param fg Foreground RGB565 color.
 * @param bg Background RGB565 color.
 * @param transparent true to keep background pixels unchanged.
 * @param spacing Extra spacing in pixels inserted between characters.
 * @return HAL_OK on success, otherwise an error status.
 */
HAL_StatusTypeDef ST7789_DrawTextCentered(uint16_t x,
                                          uint16_t y,
                                          uint16_t w,
                                          uint16_t h,
                                          const char *text,
                                          const FontDef_t *font,
                                          uint16_t fg,
                                          uint16_t bg,
                                          bool transparent,
                                          uint16_t spacing);
/**
 * @brief Draws a 1-bit monochrome bitmap.
 * @param x Bitmap origin X coordinate.
 * @param y Bitmap origin Y coordinate.
 * @param bmp Pointer to the monochrome bitmap descriptor.
 * @param fg Foreground RGB565 color used for set bits.
 * @param bg Background RGB565 color used for cleared bits.
 * @param transparent true to skip cleared bits.
 * @return HAL_OK on success, otherwise an error status.
 */
HAL_StatusTypeDef ST7789_DrawBitmapMono(uint16_t x,
                                        uint16_t y,
                                        const ST7789_BitmapMono_t *bmp,
                                        uint16_t fg,
                                        uint16_t bg,
                                        bool transparent);
/**
 * @brief Draws an RGB565 bitmap.
 * @param x Bitmap origin X coordinate.
 * @param y Bitmap origin Y coordinate.
 * @param bmp Pointer to the RGB565 bitmap descriptor.
 * @return HAL_OK on success, otherwise an error status.
 */
HAL_StatusTypeDef ST7789_DrawBitmapRGB565(uint16_t x,
                                          uint16_t y,
                                          const ST7789_BitmapRGB565_t *bmp);
/**
 * @brief Draws an RGB565 bitmap using DMA when available.
 * @param x Bitmap origin X coordinate.
 * @param y Bitmap origin Y coordinate.
 * @param bmp Pointer to the RGB565 bitmap descriptor.
 * @return HAL_OK on success, otherwise an error status.
 */
HAL_StatusTypeDef ST7789_DrawBitmapRGB565DMA(uint16_t x,
                                             uint16_t y,
                                             const ST7789_BitmapRGB565_t *bmp);

/** @}
 */

/** @name DMA support API
 *  @{
 */

/**
 * @brief Waits for the current DMA transfer to finish.
 * @param timeout Maximum wait time in milliseconds.
 */
HAL_StatusTypeDef ST7789_WaitForDma(uint32_t timeout);
/** @brief Returns whether a DMA transfer is currently in progress. */
bool ST7789_IsDmaBusy(void);
#if (ST7789_USE_DMA != 0) && (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
/**
 * @brief SPI transmit-complete callback used by the driver.
 * @param hspi SPI handle received from HAL.
 */
void ST7789_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
/**
 * @brief SPI error callback used by the driver.
 * @param hspi SPI handle received from HAL.
 */
void ST7789_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);
#endif

/** @}
 */

#if (ST7789_ENABLE_TESTS != 0)

/** @name Optional test API
 *  @{
 */

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
/** @}
 */

#endif /* ST7789_ENABLE_TESTS */

#ifdef __cplusplus
}
#endif

/** @}
 */

#endif /* ST7789_H_ */
