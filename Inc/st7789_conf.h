/**
 * @file st7789_conf.h
 * @brief User configuration file for the ST7789 display driver.
 *
 * This file contains the compile-time configuration used by the ST7789 driver:
 * selected bus interface, panel geometry, rotation offsets, GPIO mapping,
 * timeout values, optional DMA behavior and optional GPIO initialization.
 *
 * Edit this file to match the display module and the STM32CubeMX pinout used
 * by the project.
 */

#ifndef ST7789_CONF_H_
#define ST7789_CONF_H_

/**
 * @defgroup ST7789_Configuration ST7789 Driver Configuration
 * @brief Compile-time options used by the ST7789 driver.
 * @{
 */

/** @name Interface selection
 *  @{
 */

/**
 * @brief Physical display interface selection.
 *
 * Use @ref ST7789_INTERFACE_SPI for 4-wire SPI or
 * @ref ST7789_INTERFACE_PARALLEL for the 8-bit 8080 parallel bus.
 */
#define ST7789_INTERFACE_SPI       1U
#define ST7789_INTERFACE_PARALLEL  2U

#ifndef ST7789_INTERFACE
#define ST7789_INTERFACE ST7789_INTERFACE_PARALLEL
#endif
/** @}
 */

/** @name Display geometry
 *  @{
 */

/** @brief Controller RAM size and visible panel area. */
#define ST7789_RAM_WIDTH           240U
#define ST7789_RAM_HEIGHT          320U
#define ST7789_PANEL_WIDTH         240U
#define ST7789_PANEL_HEIGHT        320U

/** @brief Offsets applied for each rotation: 0, 90, 180 and 270 degrees. */
#define ST7789_X_OFFSET_0          0U
#define ST7789_Y_OFFSET_0          0U
#define ST7789_X_OFFSET_90         0U
#define ST7789_Y_OFFSET_90         0U
#define ST7789_X_OFFSET_180        0U
#define ST7789_Y_OFFSET_180        0U
#define ST7789_X_OFFSET_270        0U
#define ST7789_Y_OFFSET_270        0U
/** @}
 */

/** @name Default display behavior
 *  @{
 */

#define ST7789_DEFAULT_ROTATION    ST7789_ROTATION_0
#define ST7789_COLOR_ORDER         ST7789_MADCTL_BGR
#define ST7789_INVERTED            0
#define ST7789_TIMEOUT_MS          100U
#define ST7789_TX_CHUNK_SIZE       256U
#define ST7789_DMA_TIMEOUT_MS      100U
/** @}
 */

/** @name Optional driver features
 *  @{
 */

/**
 * @brief Enables DMA-related API callbacks and transfer support.
 *
 * Keep this disabled when the project does not configure SPI with DMA in
 * CubeMX. Enable it only when the selected interface is SPI and the HAL SPI
 * module is available.
 */
#define ST7789_USE_DMA             0

/**
 * @brief Enables optional display test functions.
 *
 * Enable this only during board bring-up or display validation.
 */
#define ST7789_ENABLE_TESTS        1

/**
 * @brief Defines DMA function behavior when the parallel interface is used.
 *
 * When enabled, DMA functions use the blocking implementation as fallback.
 */
#define ST7789_PARALLEL_DMA_FALLBACK_BLOCKING  0
/** @}
 */

/** @name SPI interface pin mapping
 *  @{
 */

/**
 * @brief Signal configuration used by the SPI interface.
 *
 * ST7789_SPI_HANDLE must be the handle name, without the & operator.
 */
#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
#define ST7789_SPI_HANDLE          hspi2

#define ST7789_CS_PORT             GPIOA
#define ST7789_CS_PIN              GPIO_PIN_4

#define ST7789_DC_PORT             GPIOB
#define ST7789_DC_PIN              GPIO_PIN_0
#endif
/** @}
 */

/** @name 8080 parallel interface pin mapping
 *  @{
 */

/**
 * @brief Signal configuration used by the 8-bit 8080 parallel interface.
 *
 * The RS signal is equivalent to D/C: 0 for command and 1 for data.
 */
#if (ST7789_INTERFACE == ST7789_INTERFACE_PARALLEL)
#define ST7789_CS_PORT             GPIOB
#define ST7789_CS_PIN              GPIO_PIN_0

#define ST7789_RS_PORT             GPIOA
#define ST7789_RS_PIN              GPIO_PIN_4

#define ST7789_WR_PORT             GPIOA
#define ST7789_WR_PIN              GPIO_PIN_1

#define ST7789_RD_PORT             GPIOA
#define ST7789_RD_PIN              GPIO_PIN_0

#define ST7789_D0_PORT             GPIOA
#define ST7789_D0_PIN              GPIO_PIN_9
#define ST7789_D1_PORT             GPIOC
#define ST7789_D1_PIN              GPIO_PIN_7
#define ST7789_D2_PORT             GPIOA
#define ST7789_D2_PIN              GPIO_PIN_10
#define ST7789_D3_PORT             GPIOB
#define ST7789_D3_PIN              GPIO_PIN_3
#define ST7789_D4_PORT             GPIOB
#define ST7789_D4_PIN              GPIO_PIN_5
#define ST7789_D5_PORT             GPIOB
#define ST7789_D5_PIN              GPIO_PIN_4
#define ST7789_D6_PORT             GPIOB
#define ST7789_D6_PIN              GPIO_PIN_10
#define ST7789_D7_PORT             GPIOA
#define ST7789_D7_PIN              GPIO_PIN_8
#endif
/** @}
 */

/** @name Optional common control pins
 *  @{
 */

/** @brief Optional common pins. Set the port to NULL when not used. */
#define ST7789_RST_PORT            GPIOC //GPIOB
#define ST7789_RST_PIN             GPIO_PIN_1   //GPIO_PIN_5

#define ST7789_BL_PORT             NULL //GPIOA
#define ST7789_BL_PIN              0U   //GPIO_PIN_8
#define ST7789_BL_ACTIVE_STATE     GPIO_PIN_SET
/** @}
 */

/** @name Optional GPIO initialization
 *  @{
 */

/** @brief Enables internal initialization for the configured GPIOs. */
#define ST7789_INIT_GPIO           1

/** @brief Macro responsible for enabling the clocks of the GPIO ports used by the display. */
#define ST7789_GPIO_CLK_ENABLE()       \
    do {                               \
        __HAL_RCC_GPIOA_CLK_ENABLE();  \
        __HAL_RCC_GPIOB_CLK_ENABLE();  \
        __HAL_RCC_GPIOC_CLK_ENABLE();  \
    } while (0)

#define ST7789_GPIO_MODE           GPIO_MODE_OUTPUT_PP
#define ST7789_GPIO_PULL           GPIO_NOPULL
#define ST7789_GPIO_SPEED          GPIO_SPEED_FREQ_MEDIUM
/** @}
 */

/** @}
 */

#endif /* ST7789_CONF_H_ */
