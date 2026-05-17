/**
 * @file st7789.c
 * @brief ST7789 TFT display driver implementation.
 *
 * This module implements display communication, initialization, graphics
 * window control, drawing primitives, text writing, bitmap writing and DMA
 * transfers when available. Pin and interface configuration is centralized in
 * @ref st7789_conf.h.
 */

#include "st7789.h"

#include <stddef.h>

#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
extern SPI_HandleTypeDef ST7789_SPI_HANDLE;
#endif

/**
 * @brief Internal ST7789 driver context.
 *
 * Holds handles, pins, dimensions, offsets, rotation, inversion state and
 * timeout values used by the public API.
 */
typedef struct
{
#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *dc_port;
    uint16_t dc_pin;
#else
    GPIO_TypeDef *rs_port;
    uint16_t rs_pin;
    GPIO_TypeDef *wr_port;
    uint16_t wr_pin;
    GPIO_TypeDef *rd_port;
    uint16_t rd_pin;
    GPIO_TypeDef *d_port[8];
    uint16_t d_pin[8];
#endif

    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;
    GPIO_TypeDef *bl_port;
    uint16_t bl_pin;
    GPIO_PinState bl_active_state;

    uint16_t ram_width;
    uint16_t ram_height;
    uint16_t panel_width;
    uint16_t panel_height;
    uint16_t x_offset[4];
    uint16_t y_offset[4];
    uint16_t width;
    uint16_t height;
    ST7789_Rotation_t rotation;
    uint8_t madctl_color_order;
    bool inverted;
    uint32_t timeout_ms;
} ST7789_HandleTypeDef;

static ST7789_HandleTypeDef st7789 =
{
#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
    .hspi = &ST7789_SPI_HANDLE,
    .dc_port = ST7789_DC_PORT,
    .dc_pin = ST7789_DC_PIN,
#else
    .rs_port = ST7789_RS_PORT,
    .rs_pin = ST7789_RS_PIN,
    .wr_port = ST7789_WR_PORT,
    .wr_pin = ST7789_WR_PIN,
    .rd_port = ST7789_RD_PORT,
    .rd_pin = ST7789_RD_PIN,
    .d_port = {
        ST7789_D0_PORT, ST7789_D1_PORT, ST7789_D2_PORT, ST7789_D3_PORT,
        ST7789_D4_PORT, ST7789_D5_PORT, ST7789_D6_PORT, ST7789_D7_PORT
    },
    .d_pin = {
        ST7789_D0_PIN, ST7789_D1_PIN, ST7789_D2_PIN, ST7789_D3_PIN,
        ST7789_D4_PIN, ST7789_D5_PIN, ST7789_D6_PIN, ST7789_D7_PIN
    },
#endif
    .cs_port = ST7789_CS_PORT,
    .cs_pin = ST7789_CS_PIN,
    .rst_port = ST7789_RST_PORT,
    .rst_pin = ST7789_RST_PIN,
    .bl_port = ST7789_BL_PORT,
    .bl_pin = ST7789_BL_PIN,
    .bl_active_state = ST7789_BL_ACTIVE_STATE,
    .ram_width = ST7789_RAM_WIDTH,
    .ram_height = ST7789_RAM_HEIGHT,
    .panel_width = ST7789_PANEL_WIDTH,
    .panel_height = ST7789_PANEL_HEIGHT,
    .x_offset = {
        ST7789_X_OFFSET_0,
        ST7789_X_OFFSET_90,
        ST7789_X_OFFSET_180,
        ST7789_X_OFFSET_270
    },
    .y_offset = {
        ST7789_Y_OFFSET_0,
        ST7789_Y_OFFSET_90,
        ST7789_Y_OFFSET_180,
        ST7789_Y_OFFSET_270
    },
    .width = ST7789_PANEL_WIDTH,
    .height = ST7789_PANEL_HEIGHT,
    .rotation = ST7789_DEFAULT_ROTATION,
    .madctl_color_order = ST7789_COLOR_ORDER,
    .inverted = (ST7789_INVERTED != 0),
    .timeout_ms = ST7789_TIMEOUT_MS
};

/** @brief Internal DMA transfer operation modes. */
typedef enum
{
    ST7789_DMA_MODE_NONE = 0U,
    ST7789_DMA_MODE_DATA,
    ST7789_DMA_MODE_FILL,
    ST7789_DMA_MODE_IMAGE
} ST7789_DmaMode_t;

/** @brief Internal state of an ongoing DMA transfer. */
typedef struct
{
    ST7789_DmaMode_t mode;
    const uint8_t *data;
    const uint16_t *image;
    uint32_t total_pixels;
    volatile uint32_t sent_pixels;
    volatile bool busy;
    volatile HAL_StatusTypeDef status;
} ST7789_DmaContext_t;

static ST7789_DmaContext_t st7789_dma =
{
    .mode = ST7789_DMA_MODE_NONE,
    .data = NULL,
    .image = NULL,
    .total_pixels = 0U,
    .sent_pixels = 0U,
    .busy = false,
    .status = HAL_OK
};

static uint8_t st7789_dma_buffer[ST7789_TX_CHUNK_SIZE];

static inline void ST7789_Bus_Select(void)
{
    HAL_GPIO_WritePin(st7789.cs_port, st7789.cs_pin, GPIO_PIN_RESET);
}

static inline void ST7789_Bus_Unselect(void)
{
    HAL_GPIO_WritePin(st7789.cs_port, st7789.cs_pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef ST7789_Core_Validate(void)
{
    if ((st7789.cs_port == NULL) ||
        (st7789.panel_width == 0U) ||
        (st7789.panel_height == 0U))
    {
        return HAL_ERROR;
    }

#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
    if ((st7789.hspi == NULL) || (st7789.dc_port == NULL))
    {
        return HAL_ERROR;
    }
#else
    if ((st7789.rs_port == NULL) ||
        (st7789.wr_port == NULL) ||
        (st7789.rd_port == NULL))
    {
        return HAL_ERROR;
    }

    for (uint8_t i = 0U; i < 8U; i++)
    {
        if (st7789.d_port[i] == NULL)
        {
            return HAL_ERROR;
        }
    }
#endif

    if (st7789.ram_width == 0U)
    {
        st7789.ram_width = ST7789_RAM_WIDTH;
    }

    if (st7789.ram_height == 0U)
    {
        st7789.ram_height = ST7789_RAM_HEIGHT;
    }

    if (st7789.timeout_ms == 0U)
    {
        st7789.timeout_ms = ST7789_TIMEOUT_MS;
    }

    return HAL_OK;
}

#if ST7789_INIT_GPIO
static HAL_StatusTypeDef ST7789_GPIO_InitPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (GPIOx == NULL)
    {
        return HAL_OK;
    }

    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = ST7789_GPIO_MODE;
    GPIO_InitStruct.Pull = ST7789_GPIO_PULL;
    GPIO_InitStruct.Speed = ST7789_GPIO_SPEED;

    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
    return HAL_OK;
}

static HAL_StatusTypeDef ST7789_GPIO_Init(void)
{
    ST7789_GPIO_CLK_ENABLE();

    (void)ST7789_GPIO_InitPin(st7789.cs_port, st7789.cs_pin);
    (void)ST7789_GPIO_InitPin(st7789.rst_port, st7789.rst_pin);
    (void)ST7789_GPIO_InitPin(st7789.bl_port, st7789.bl_pin);

#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
    (void)ST7789_GPIO_InitPin(st7789.dc_port, st7789.dc_pin);
#else
    (void)ST7789_GPIO_InitPin(st7789.rs_port, st7789.rs_pin);
    (void)ST7789_GPIO_InitPin(st7789.wr_port, st7789.wr_pin);
    (void)ST7789_GPIO_InitPin(st7789.rd_port, st7789.rd_pin);

    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
        (void)ST7789_GPIO_InitPin(st7789.d_port[bit], st7789.d_pin[bit]);
    }
#endif

    ST7789_Bus_Unselect();

#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
    HAL_GPIO_WritePin(st7789.dc_port, st7789.dc_pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(st7789.wr_port, st7789.wr_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(st7789.rd_port, st7789.rd_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(st7789.rs_port, st7789.rs_pin, GPIO_PIN_SET);
#endif

    if (st7789.rst_port != NULL)
    {
        HAL_GPIO_WritePin(st7789.rst_port, st7789.rst_pin, GPIO_PIN_SET);
    }

    if (st7789.bl_port != NULL)
    {
        HAL_GPIO_WritePin(st7789.bl_port,
                          st7789.bl_pin,
                          (st7789.bl_active_state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    return HAL_OK;
}
#endif

#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
static inline void ST7789_Bus_CommandMode(void)
{
    HAL_GPIO_WritePin(st7789.dc_port, st7789.dc_pin, GPIO_PIN_RESET);
}

static inline void ST7789_Bus_DataMode(void)
{
    HAL_GPIO_WritePin(st7789.dc_port, st7789.dc_pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef ST7789_Bus_Write(const uint8_t *data, size_t length)
{
    size_t offset = 0U;

    if ((data == NULL) && (length > 0U))
    {
        return HAL_ERROR;
    }

    while (offset < length)
    {
        uint16_t chunk = (uint16_t)((length - offset) > 65535U ? 65535U : (length - offset));
        HAL_StatusTypeDef status = HAL_SPI_Transmit(st7789.hspi, (uint8_t *)&data[offset], chunk, st7789.timeout_ms);
        if (status != HAL_OK)
        {
            return status;
        }

        offset += chunk;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef ST7789_Bus_WriteDMA(const uint8_t *data, uint16_t length)
{
    return HAL_SPI_Transmit_DMA(st7789.hspi, (uint8_t *)data, length);
}
#else
static inline void ST7789_Bus_CommandMode(void)
{
    HAL_GPIO_WritePin(st7789.rs_port, st7789.rs_pin, GPIO_PIN_RESET);
}

static inline void ST7789_Bus_DataMode(void)
{
    HAL_GPIO_WritePin(st7789.rs_port, st7789.rs_pin, GPIO_PIN_SET);
}

static void ST7789_ParallelWrite8(uint8_t value)
{
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
        HAL_GPIO_WritePin(st7789.d_port[bit],
                          st7789.d_pin[bit],
                          ((value & (uint8_t)(1U << bit)) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    HAL_GPIO_WritePin(st7789.wr_port, st7789.wr_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(st7789.wr_port, st7789.wr_pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef ST7789_Bus_Write(const uint8_t *data, size_t length)
{
    if ((data == NULL) && (length > 0U))
    {
        return HAL_ERROR;
    }

    HAL_GPIO_WritePin(st7789.rd_port, st7789.rd_pin, GPIO_PIN_SET);

    for (size_t i = 0U; i < length; i++)
    {
        ST7789_ParallelWrite8(data[i]);
    }

    return HAL_OK;
}

static HAL_StatusTypeDef ST7789_Bus_WriteDMA(const uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
    return HAL_ERROR;
}
#endif

static void ST7789_DMA_Finish(HAL_StatusTypeDef status)
{
    ST7789_Bus_Unselect();
    st7789_dma.status = status;
    st7789_dma.busy = false;
    st7789_dma.mode = ST7789_DMA_MODE_NONE;
    st7789_dma.data = NULL;
    st7789_dma.image = NULL;
    st7789_dma.total_pixels = 0U;
    st7789_dma.sent_pixels = 0U;
}

static HAL_StatusTypeDef ST7789_DMA_StartNextChunk(void)
{
#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
    uint32_t remaining_pixels;
    uint32_t burst_pixels;
    uint16_t dma_length;

    if ((st7789_dma.mode != ST7789_DMA_MODE_FILL) &&
        (st7789_dma.mode != ST7789_DMA_MODE_IMAGE))
    {
        ST7789_DMA_Finish(HAL_OK);
        return HAL_OK;
    }

    if (st7789_dma.sent_pixels >= st7789_dma.total_pixels)
    {
        ST7789_DMA_Finish(HAL_OK);
        return HAL_OK;
    }

    remaining_pixels = st7789_dma.total_pixels - st7789_dma.sent_pixels;
    burst_pixels = remaining_pixels;

    if (burst_pixels > (ST7789_TX_CHUNK_SIZE / 2U))
    {
        burst_pixels = (ST7789_TX_CHUNK_SIZE / 2U);
    }

    dma_length = (uint16_t)(burst_pixels * 2U);

    if (st7789_dma.mode == ST7789_DMA_MODE_IMAGE)
    {
        for (uint32_t i = 0U; i < burst_pixels; i++)
        {
            uint16_t pixel = st7789_dma.image[st7789_dma.sent_pixels + i];
            st7789_dma_buffer[(2U * i)] = (uint8_t)(pixel >> 8);
            st7789_dma_buffer[(2U * i) + 1U] = (uint8_t)(pixel & 0xFFU);
        }
    }

    HAL_StatusTypeDef status = ST7789_Bus_WriteDMA(st7789_dma_buffer, dma_length);
    if (status != HAL_OK)
    {
        ST7789_DMA_Finish(status);
        return status;
    }

    st7789_dma.sent_pixels += burst_pixels;
    return HAL_OK;
#else
    ST7789_DMA_Finish(HAL_ERROR);
    return HAL_ERROR;
#endif
}

static void ST7789_SwapInt32(int32_t *a, int32_t *b)
{
    int32_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static HAL_StatusTypeDef ST7789_DrawPixelClipped(int32_t x, int32_t y, uint16_t color)
{
    if ((x < 0) ||
        (y < 0) ||
        (x >= (int32_t)ST7789_GetWidth()) ||
        (y >= (int32_t)ST7789_GetHeight()))
    {
        return HAL_OK;
    }

    return ST7789_DrawPixel((uint16_t)x, (uint16_t)y, color);
}

static HAL_StatusTypeDef ST7789_DrawFastHLineClipped(int32_t x,
                                                     int32_t y,
                                                     int32_t w,
                                                     uint16_t color)
{
    int32_t x_end;

    if ((w <= 0) ||
        (y < 0) ||
        (y >= (int32_t)ST7789_GetHeight()))
    {
        return HAL_OK;
    }

    x_end = x + w - 1;

    if ((x_end < 0) || (x >= (int32_t)ST7789_GetWidth()))
    {
        return HAL_OK;
    }

    if (x < 0)
    {
        x = 0;
    }

    if (x_end >= (int32_t)ST7789_GetWidth())
    {
        x_end = (int32_t)ST7789_GetWidth() - 1;
    }

    return ST7789_DrawFastHLine((uint16_t)x,
                                (uint16_t)y,
                                (uint16_t)(x_end - x + 1),
                                color);
}

static HAL_StatusTypeDef ST7789_DrawLineBresenham(int32_t x0,
                                                  int32_t y0,
                                                  int32_t x1,
                                                  int32_t y1,
                                                  uint16_t color)
{
    int32_t dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    int32_t dy = (y0 < y1) ? (y1 - y0) : (y0 - y1);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx - dy;

    while (true)
    {
        HAL_StatusTypeDef status = ST7789_DrawPixelClipped(x0, y0, color);
        if (status != HAL_OK)
        {
            return status;
        }

        if ((x0 == x1) && (y0 == y1))
        {
            break;
        }

        int32_t e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_WriteCommand(uint8_t command)
{
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    ST7789_Bus_Select();
    ST7789_Bus_CommandMode();
    status = ST7789_Bus_Write(&command, 1U);
    ST7789_Bus_Unselect();

    return status;
}

HAL_StatusTypeDef ST7789_WriteData(const uint8_t *data, size_t length)
{
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((data == NULL) && (length > 0U))
    {
        return HAL_ERROR;
    }

    ST7789_Bus_Select();
    ST7789_Bus_DataMode();
    status = ST7789_Bus_Write(data, length);
    ST7789_Bus_Unselect();

    return status;
}

HAL_StatusTypeDef ST7789_WriteCommandData(uint8_t command, const uint8_t *data, size_t length)
{
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((data == NULL) && (length > 0U))
    {
        return HAL_ERROR;
    }

    ST7789_Bus_Select();

    ST7789_Bus_CommandMode();
    status = ST7789_Bus_Write(&command, 1U);
    if (status != HAL_OK)
    {
        ST7789_Bus_Unselect();
        return status;
    }

    if (length > 0U)
    {
        ST7789_Bus_DataMode();
        status = ST7789_Bus_Write(data, length);
    }

    ST7789_Bus_Unselect();
    return status;
}

HAL_StatusTypeDef ST7789_Reset(void)
{
    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    ST7789_Bus_Unselect();

#if (ST7789_INTERFACE == ST7789_INTERFACE_PARALLEL)
    HAL_GPIO_WritePin(st7789.rd_port, st7789.rd_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(st7789.wr_port, st7789.wr_pin, GPIO_PIN_SET);
#endif

    if (st7789.rst_port == NULL)
    {
        return HAL_OK;
    }

    HAL_GPIO_WritePin(st7789.rst_port, st7789.rst_pin, GPIO_PIN_SET);
    HAL_Delay(5U);
    HAL_GPIO_WritePin(st7789.rst_port, st7789.rst_pin, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(st7789.rst_port, st7789.rst_pin, GPIO_PIN_SET);
    HAL_Delay(120U);

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_RunCommandList(const ST7789_InitCmd_t *cmds, size_t count)
{
    if ((cmds == NULL) && (count > 0U))
    {
        return HAL_ERROR;
    }

    for (size_t i = 0U; i < count; i++)
    {
        HAL_StatusTypeDef status = ST7789_WriteCommandData(cmds[i].command, cmds[i].data, cmds[i].length);
        if (status != HAL_OK)
        {
            return status;
        }

        if (cmds[i].delay_ms_after > 0U)
        {
            HAL_Delay(cmds[i].delay_ms_after);
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_SetBacklight(bool enable)
{
    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (st7789.bl_port == NULL)
    {
        return HAL_OK;
    }

    HAL_GPIO_WritePin(st7789.bl_port,
                      st7789.bl_pin,
                      enable ? st7789.bl_active_state
                             : (st7789.bl_active_state == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET));

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_DisplayOn(void)
{
    return ST7789_WriteCommand(ST7789_CMD_DISPON);
}

HAL_StatusTypeDef ST7789_DisplayOff(void)
{
    return ST7789_WriteCommand(ST7789_CMD_DISPOFF);
}

HAL_StatusTypeDef ST7789_InvertDisplay(bool enable)
{
    st7789.inverted = enable;
    return ST7789_WriteCommand(enable ? ST7789_CMD_INVON : ST7789_CMD_INVOFF);
}

HAL_StatusTypeDef ST7789_SetRotation(ST7789_Rotation_t rotation)
{
    uint8_t madctl = st7789.madctl_color_order;
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    st7789.rotation = rotation;

    switch (rotation)
    {
        case ST7789_ROTATION_0:
            st7789.width = st7789.panel_width;
            st7789.height = st7789.panel_height;
            break;

        case ST7789_ROTATION_90:
            madctl |= (ST7789_MADCTL_MV | ST7789_MADCTL_MX);
            st7789.width = st7789.panel_height;
            st7789.height = st7789.panel_width;
            break;

        case ST7789_ROTATION_180:
            madctl |= (ST7789_MADCTL_MX | ST7789_MADCTL_MY);
            st7789.width = st7789.panel_width;
            st7789.height = st7789.panel_height;
            break;

        case ST7789_ROTATION_270:
            madctl |= (ST7789_MADCTL_MV | ST7789_MADCTL_MY);
            st7789.width = st7789.panel_height;
            st7789.height = st7789.panel_width;
            break;

        default:
            return HAL_ERROR;
    }

    status = ST7789_WriteCommandData(ST7789_CMD_MADCTL, &madctl, 1U);
    HAL_Delay(1U);
    return status;
}

HAL_StatusTypeDef ST7789_InitEx(const ST7789_InitCmd_t *extra_cmds, size_t extra_count)
{
    uint8_t color_mode = ST7789_COLOR_MODE_16BIT;
    HAL_StatusTypeDef status = ST7789_Core_Validate();

    if (status != HAL_OK)
    {
        return status;
    }

#if ST7789_INIT_GPIO
    status = ST7789_GPIO_Init();
    if (status != HAL_OK)
    {
        return status;
    }
#endif

    status = ST7789_SetBacklight(false);
    if (status != HAL_OK)
    {
        return status;
    }

    status = ST7789_Reset();
    if (status != HAL_OK)
    {
        return status;
    }

    status = ST7789_WriteCommand(ST7789_CMD_SLPOUT);
    if (status != HAL_OK)
    {
        return status;
    }
    HAL_Delay(120U);

    if ((extra_cmds != NULL) && (extra_count > 0U))
    {
        status = ST7789_RunCommandList(extra_cmds, extra_count);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    status = ST7789_WriteCommandData(ST7789_CMD_COLMOD, &color_mode, 1U);
    if (status != HAL_OK)
    {
        return status;
    }
    HAL_Delay(10U);

    status = ST7789_SetRotation(st7789.rotation);
    if (status != HAL_OK)
    {
        return status;
    }

    status = ST7789_InvertDisplay(st7789.inverted);
    if (status != HAL_OK)
    {
        return status;
    }

    status = ST7789_WriteCommand(ST7789_CMD_NORON);
    if (status != HAL_OK)
    {
        return status;
    }
    HAL_Delay(10U);

    status = ST7789_WriteCommand(ST7789_CMD_DISPON);
    if (status != HAL_OK)
    {
        return status;
    }
    HAL_Delay(100U);

    return ST7789_SetBacklight(true);
}

HAL_StatusTypeDef ST7789_Init(void)
{
    return ST7789_InitEx(NULL, 0U);
}

uint16_t ST7789_GetWidth(void)
{
    return st7789.width;
}

uint16_t ST7789_GetHeight(void)
{
    return st7789.height;
}

ST7789_Rotation_t ST7789_GetRotation(void)
{
    return st7789.rotation;
}

HAL_StatusTypeDef ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint16_t xo;
    uint16_t yo;
    uint16_t xs;
    uint16_t xe;
    uint16_t ys;
    uint16_t ye;
    uint16_t ram_x_limit;
    uint16_t ram_y_limit;
    uint8_t data[4];
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((x0 > x1) || (y0 > y1) || (x1 >= st7789.width) || (y1 >= st7789.height))
    {
        return HAL_ERROR;
    }

    xo = st7789.x_offset[(uint8_t)st7789.rotation];
    yo = st7789.y_offset[(uint8_t)st7789.rotation];

    xs = x0 + xo;
    xe = x1 + xo;
    ys = y0 + yo;
    ye = y1 + yo;

    if ((st7789.rotation == ST7789_ROTATION_90) ||
        (st7789.rotation == ST7789_ROTATION_270))
    {
        ram_x_limit = st7789.ram_height;
        ram_y_limit = st7789.ram_width;
    }
    else
    {
        ram_x_limit = st7789.ram_width;
        ram_y_limit = st7789.ram_height;
    }

    if ((xs >= ram_x_limit) || (xe >= ram_x_limit) ||
        (ys >= ram_y_limit) || (ye >= ram_y_limit))
    {
        return HAL_ERROR;
    }

    data[0] = (uint8_t)(xs >> 8);
    data[1] = (uint8_t)(xs & 0xFFU);
    data[2] = (uint8_t)(xe >> 8);
    data[3] = (uint8_t)(xe & 0xFFU);
    status = ST7789_WriteCommandData(ST7789_CMD_CASET, data, 4U);
    if (status != HAL_OK)
    {
        return status;
    }

    data[0] = (uint8_t)(ys >> 8);
    data[1] = (uint8_t)(ys & 0xFFU);
    data[2] = (uint8_t)(ye >> 8);
    data[3] = (uint8_t)(ye & 0xFFU);
    status = ST7789_WriteCommandData(ST7789_CMD_RASET, data, 4U);
    if (status != HAL_OK)
    {
        return status;
    }

    return ST7789_WriteCommand(ST7789_CMD_RAMWR);
}

HAL_StatusTypeDef ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t data[2];

    if ((x >= st7789.width) || (y >= st7789.height))
    {
        return HAL_ERROR;
    }

    if (ST7789_SetAddressWindow(x, y, x, y) != HAL_OK)
    {
        return HAL_ERROR;
    }

    data[0] = (uint8_t)(color >> 8);
    data[1] = (uint8_t)(color & 0xFFU);

    return ST7789_WriteData(data, 2U);
}

HAL_StatusTypeDef ST7789_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t burst[ST7789_TX_CHUNK_SIZE];
    uint32_t total_pixels;
    uint32_t sent_pixels = 0U;
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((w == 0U) || (h == 0U))
    {
        return HAL_OK;
    }

    if ((x >= st7789.width) || (y >= st7789.height))
    {
        return HAL_ERROR;
    }

    if ((x + w) > st7789.width)
    {
        w = st7789.width - x;
    }

    if ((y + h) > st7789.height)
    {
        h = st7789.height - y;
    }

    status = ST7789_SetAddressWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    if (status != HAL_OK)
    {
        return status;
    }

    for (uint32_t i = 0U; i < (ST7789_TX_CHUNK_SIZE / 2U); i++)
    {
        burst[(2U * i)] = (uint8_t)(color >> 8);
        burst[(2U * i) + 1U] = (uint8_t)(color & 0xFFU);
    }

    total_pixels = (uint32_t)w * (uint32_t)h;

    ST7789_Bus_Select();
    ST7789_Bus_DataMode();

    while (sent_pixels < total_pixels)
    {
        uint32_t burst_pixels = total_pixels - sent_pixels;
        if (burst_pixels > (ST7789_TX_CHUNK_SIZE / 2U))
        {
            burst_pixels = (ST7789_TX_CHUNK_SIZE / 2U);
        }

        status = ST7789_Bus_Write(burst, burst_pixels * 2U);
        if (status != HAL_OK)
        {
            ST7789_Bus_Unselect();
            return status;
        }

        sent_pixels += burst_pixels;
    }

    ST7789_Bus_Unselect();
    return HAL_OK;
}

HAL_StatusTypeDef ST7789_DrawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    return ST7789_FillRect(x, y, w, 1U, color);
}

HAL_StatusTypeDef ST7789_DrawFastVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    return ST7789_FillRect(x, y, 1U, h, color);
}

HAL_StatusTypeDef ST7789_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    HAL_StatusTypeDef status;

    if ((w == 0U) || (h == 0U))
    {
        return HAL_OK;
    }

    status = ST7789_DrawFastHLine(x, y, w, color);
    if (status != HAL_OK)
    {
        return status;
    }

    if (h > 1U)
    {
        status = ST7789_DrawFastHLine(x, (uint16_t)(y + h - 1U), w, color);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    status = ST7789_DrawFastVLine(x, y, h, color);
    if ((status == HAL_OK) && (w > 1U))
    {
        status = ST7789_DrawFastVLine((uint16_t)(x + w - 1U), y, h, color);
    }

    return status;
}

HAL_StatusTypeDef ST7789_FillScreen(uint16_t color)
{
    return ST7789_FillRect(0U, 0U, st7789.width, st7789.height, color);
}

HAL_StatusTypeDef ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
    int32_t f;
    int32_t dd_f_x;
    int32_t dd_f_y;
    int32_t x;
    int32_t y;

    if (r == 0U)
    {
        return ST7789_DrawPixelClipped((int32_t)x0, (int32_t)y0, color);
    }

    f = 1 - (int32_t)r;
    dd_f_x = 1;
    dd_f_y = -2 * (int32_t)r;
    x = 0;
    y = (int32_t)r;

    (void)ST7789_DrawPixelClipped((int32_t)x0, (int32_t)y0 + (int32_t)r, color);
    (void)ST7789_DrawPixelClipped((int32_t)x0, (int32_t)y0 - (int32_t)r, color);
    (void)ST7789_DrawPixelClipped((int32_t)x0 + (int32_t)r, (int32_t)y0, color);
    (void)ST7789_DrawPixelClipped((int32_t)x0 - (int32_t)r, (int32_t)y0, color);

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            dd_f_y += 2;
            f += dd_f_y;
        }

        x++;
        dd_f_x += 2;
        f += dd_f_x;

        (void)ST7789_DrawPixelClipped((int32_t)x0 + x, (int32_t)y0 + y, color);
        (void)ST7789_DrawPixelClipped((int32_t)x0 - x, (int32_t)y0 + y, color);
        (void)ST7789_DrawPixelClipped((int32_t)x0 + x, (int32_t)y0 - y, color);
        (void)ST7789_DrawPixelClipped((int32_t)x0 - x, (int32_t)y0 - y, color);
        (void)ST7789_DrawPixelClipped((int32_t)x0 + y, (int32_t)y0 + x, color);
        (void)ST7789_DrawPixelClipped((int32_t)x0 - y, (int32_t)y0 + x, color);
        (void)ST7789_DrawPixelClipped((int32_t)x0 + y, (int32_t)y0 - x, color);
        (void)ST7789_DrawPixelClipped((int32_t)x0 - y, (int32_t)y0 - x, color);
    }

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_FillCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
    int32_t f;
    int32_t dd_f_x;
    int32_t dd_f_y;
    int32_t x;
    int32_t y;

    if (r == 0U)
    {
        return ST7789_DrawPixelClipped((int32_t)x0, (int32_t)y0, color);
    }

    (void)ST7789_DrawFastHLineClipped((int32_t)x0 - (int32_t)r,
                                      (int32_t)y0,
                                      (int32_t)(2U * r) + 1,
                                      color);

    f = 1 - (int32_t)r;
    dd_f_x = 1;
    dd_f_y = -2 * (int32_t)r;
    x = 0;
    y = (int32_t)r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            dd_f_y += 2;
            f += dd_f_y;
        }

        x++;
        dd_f_x += 2;
        f += dd_f_x;

        (void)ST7789_DrawFastHLineClipped((int32_t)x0 - x, (int32_t)y0 + y, (2 * x) + 1, color);
        (void)ST7789_DrawFastHLineClipped((int32_t)x0 - x, (int32_t)y0 - y, (2 * x) + 1, color);
        (void)ST7789_DrawFastHLineClipped((int32_t)x0 - y, (int32_t)y0 + x, (2 * y) + 1, color);
        (void)ST7789_DrawFastHLineClipped((int32_t)x0 - y, (int32_t)y0 - x, (2 * y) + 1, color);
    }

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_DrawTriangle(uint16_t x0,
                                      uint16_t y0,
                                      uint16_t x1,
                                      uint16_t y1,
                                      uint16_t x2,
                                      uint16_t y2,
                                      uint16_t color)
{
    HAL_StatusTypeDef status;

    status = ST7789_DrawLineBresenham((int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1, color);
    if (status != HAL_OK)
    {
        return status;
    }

    status = ST7789_DrawLineBresenham((int32_t)x1, (int32_t)y1, (int32_t)x2, (int32_t)y2, color);
    if (status != HAL_OK)
    {
        return status;
    }

    return ST7789_DrawLineBresenham((int32_t)x2, (int32_t)y2, (int32_t)x0, (int32_t)y0, color);
}

HAL_StatusTypeDef ST7789_FillTriangle(uint16_t x0,
                                      uint16_t y0,
                                      uint16_t x1,
                                      uint16_t y1,
                                      uint16_t x2,
                                      uint16_t y2,
                                      uint16_t color)
{
    int32_t sx0 = (int32_t)x0;
    int32_t sy0 = (int32_t)y0;
    int32_t sx1 = (int32_t)x1;
    int32_t sy1 = (int32_t)y1;
    int32_t sx2 = (int32_t)x2;
    int32_t sy2 = (int32_t)y2;
    HAL_StatusTypeDef status;

    if (sy0 > sy1)
    {
        ST7789_SwapInt32(&sy0, &sy1);
        ST7789_SwapInt32(&sx0, &sx1);
    }

    if (sy1 > sy2)
    {
        ST7789_SwapInt32(&sy1, &sy2);
        ST7789_SwapInt32(&sx1, &sx2);
    }

    if (sy0 > sy1)
    {
        ST7789_SwapInt32(&sy0, &sy1);
        ST7789_SwapInt32(&sx0, &sx1);
    }

    if (sy0 == sy2)
    {
        int32_t min_x = sx0;
        int32_t max_x = sx0;

        if (sx1 < min_x) { min_x = sx1; }
        if (sx2 < min_x) { min_x = sx2; }
        if (sx1 > max_x) { max_x = sx1; }
        if (sx2 > max_x) { max_x = sx2; }

        return ST7789_DrawFastHLineClipped(min_x, sy0, max_x - min_x + 1, color);
    }

    for (int32_t y = sy0; y <= sy1; y++)
    {
        int32_t a;
        int32_t b;

        if (sy1 == sy0)
        {
            break;
        }

        a = sx0 + (((sx1 - sx0) * (y - sy0)) / (sy1 - sy0));
        b = sx0 + (((sx2 - sx0) * (y - sy0)) / (sy2 - sy0));

        if (a > b)
        {
            ST7789_SwapInt32(&a, &b);
        }

        status = ST7789_DrawFastHLineClipped(a, y, b - a + 1, color);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    for (int32_t y = sy1; y <= sy2; y++)
    {
        int32_t a;
        int32_t b;

        if (sy2 == sy1)
        {
            break;
        }

        a = sx1 + (((sx2 - sx1) * (y - sy1)) / (sy2 - sy1));
        b = sx0 + (((sx2 - sx0) * (y - sy0)) / (sy2 - sy0));

        if (a > b)
        {
            ST7789_SwapInt32(&a, &b);
        }

        status = ST7789_DrawFastHLineClipped(a, y, b - a + 1, color);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_WriteImageRGB565(uint16_t x,
                                          uint16_t y,
                                          uint16_t w,
                                          uint16_t h,
                                          const uint16_t *image)
{
    uint8_t burst[ST7789_TX_CHUNK_SIZE];
    uint32_t total_pixels;
    uint32_t sent_pixels = 0U;
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((image == NULL) || (w == 0U) || (h == 0U))
    {
        return HAL_ERROR;
    }

    if ((((uint32_t)x + (uint32_t)w) > st7789.width) ||
        (((uint32_t)y + (uint32_t)h) > st7789.height))
    {
        return HAL_ERROR;
    }

    status = ST7789_SetAddressWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    if (status != HAL_OK)
    {
        return status;
    }

    total_pixels = (uint32_t)w * (uint32_t)h;

    ST7789_Bus_Select();
    ST7789_Bus_DataMode();

    while (sent_pixels < total_pixels)
    {
        uint32_t burst_pixels = total_pixels - sent_pixels;
        if (burst_pixels > (ST7789_TX_CHUNK_SIZE / 2U))
        {
            burst_pixels = (ST7789_TX_CHUNK_SIZE / 2U);
        }

        for (uint32_t i = 0U; i < burst_pixels; i++)
        {
            uint16_t pixel = image[sent_pixels + i];
            burst[(2U * i)] = (uint8_t)(pixel >> 8);
            burst[(2U * i) + 1U] = (uint8_t)(pixel & 0xFFU);
        }

        status = ST7789_Bus_Write(burst, burst_pixels * 2U);
        if (status != HAL_OK)
        {
            ST7789_Bus_Unselect();
            return status;
        }

        sent_pixels += burst_pixels;
    }

    ST7789_Bus_Unselect();
    return HAL_OK;
}

HAL_StatusTypeDef ST7789_WriteDataDMA(const uint8_t *data, uint16_t length)
{
#if (ST7789_INTERFACE == ST7789_INTERFACE_PARALLEL)
#if ST7789_PARALLEL_DMA_FALLBACK_BLOCKING
    return ST7789_WriteData(data, length);
#else
    (void)data;
    (void)length;
    return HAL_ERROR;
#endif
#else
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((data == NULL) && (length > 0U))
    {
        return HAL_ERROR;
    }

    if (length == 0U)
    {
        return HAL_OK;
    }

    if (st7789_dma.busy)
    {
        return HAL_BUSY;
    }

    st7789_dma.mode = ST7789_DMA_MODE_DATA;
    st7789_dma.data = data;
    st7789_dma.image = NULL;
    st7789_dma.total_pixels = 0U;
    st7789_dma.sent_pixels = 0U;
    st7789_dma.status = HAL_BUSY;
    st7789_dma.busy = true;

    ST7789_Bus_Select();
    ST7789_Bus_DataMode();

    status = ST7789_Bus_WriteDMA(data, length);
    if (status != HAL_OK)
    {
        ST7789_DMA_Finish(status);
        return status;
    }

    return HAL_OK;
#endif
}

HAL_StatusTypeDef ST7789_FillRectDMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
#if (ST7789_INTERFACE == ST7789_INTERFACE_PARALLEL)
#if ST7789_PARALLEL_DMA_FALLBACK_BLOCKING
    return ST7789_FillRect(x, y, w, h, color);
#else
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
    return HAL_ERROR;
#endif
#else
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((w == 0U) || (h == 0U))
    {
        return HAL_OK;
    }

    if (st7789_dma.busy)
    {
        return HAL_BUSY;
    }

    if ((x >= st7789.width) || (y >= st7789.height))
    {
        return HAL_ERROR;
    }

    if ((x + w) > st7789.width)
    {
        w = st7789.width - x;
    }

    if ((y + h) > st7789.height)
    {
        h = st7789.height - y;
    }

    status = ST7789_SetAddressWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    if (status != HAL_OK)
    {
        return status;
    }

    for (uint32_t i = 0U; i < (ST7789_TX_CHUNK_SIZE / 2U); i++)
    {
        st7789_dma_buffer[(2U * i)] = (uint8_t)(color >> 8);
        st7789_dma_buffer[(2U * i) + 1U] = (uint8_t)(color & 0xFFU);
    }

    st7789_dma.mode = ST7789_DMA_MODE_FILL;
    st7789_dma.data = NULL;
    st7789_dma.image = NULL;
    st7789_dma.total_pixels = (uint32_t)w * (uint32_t)h;
    st7789_dma.sent_pixels = 0U;
    st7789_dma.status = HAL_BUSY;
    st7789_dma.busy = true;

    ST7789_Bus_Select();
    ST7789_Bus_DataMode();

    return ST7789_DMA_StartNextChunk();
#endif
}

HAL_StatusTypeDef ST7789_FillScreenDMA(uint16_t color)
{
    return ST7789_FillRectDMA(0U, 0U, st7789.width, st7789.height, color);
}

HAL_StatusTypeDef ST7789_WriteImageRGB565DMA(uint16_t x,
                                             uint16_t y,
                                             uint16_t w,
                                             uint16_t h,
                                             const uint16_t *image)
{
#if (ST7789_INTERFACE == ST7789_INTERFACE_PARALLEL)
#if ST7789_PARALLEL_DMA_FALLBACK_BLOCKING
    return ST7789_WriteImageRGB565(x, y, w, h, image);
#else
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)image;
    return HAL_ERROR;
#endif
#else
    HAL_StatusTypeDef status;

    if (ST7789_Core_Validate() != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((image == NULL) || (w == 0U) || (h == 0U))
    {
        return HAL_ERROR;
    }

    if (st7789_dma.busy)
    {
        return HAL_BUSY;
    }

    if ((((uint32_t)x + (uint32_t)w) > st7789.width) ||
        (((uint32_t)y + (uint32_t)h) > st7789.height))
    {
        return HAL_ERROR;
    }

    status = ST7789_SetAddressWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    if (status != HAL_OK)
    {
        return status;
    }

    st7789_dma.mode = ST7789_DMA_MODE_IMAGE;
    st7789_dma.data = NULL;
    st7789_dma.image = image;
    st7789_dma.total_pixels = (uint32_t)w * (uint32_t)h;
    st7789_dma.sent_pixels = 0U;
    st7789_dma.status = HAL_BUSY;
    st7789_dma.busy = true;

    ST7789_Bus_Select();
    ST7789_Bus_DataMode();

    return ST7789_DMA_StartNextChunk();
#endif
}

uint16_t ST7789_CharWidth(const FontDef_t *font)
{
    return (font == NULL) ? 0U : font->width;
}

uint16_t ST7789_CharHeight(const FontDef_t *font)
{
    return (font == NULL) ? 0U : font->height;
}

uint16_t ST7789_TextWidth(const FontDef_t *font, const char *text, uint16_t spacing)
{
    uint16_t width = 0U;

    if ((font == NULL) || (text == NULL))
    {
        return 0U;
    }

    while (*text != '\0')
    {
        if (*text == '\n')
        {
            break;
        }

        width = (uint16_t)(width + font->width);

        if (text[1] != '\0')
        {
            width = (uint16_t)(width + spacing);
        }

        text++;
    }

    return width;
}

HAL_StatusTypeDef ST7789_DrawChar(uint16_t x,
                                  uint16_t y,
                                  char ch,
                                  const FontDef_t *font,
                                  uint16_t fg,
                                  uint16_t bg,
                                  bool transparent)
{
    uint32_t glyph_index;

    if ((font == NULL) || (font->data == NULL))
    {
        return HAL_ERROR;
    }

    if ((font->width == 0U) || (font->width > 32U))
    {
        return HAL_ERROR;
    }

    if (((uint8_t)ch < 32U) || ((uint8_t)ch > 126U))
    {
        ch = '?';
    }

    glyph_index = ((uint32_t)((uint8_t)ch - 32U)) * (uint32_t)font->height;

    for (uint16_t row = 0U; row < font->height; row++)
    {
        uint32_t row_bits;

        if (font->storage == FONT_STORAGE_16)
        {
            const uint16_t *rows16 = (const uint16_t *)font->data;
            row_bits = (uint32_t)rows16[glyph_index + row];
        }
        else
        {
            const uint32_t *rows32 = (const uint32_t *)font->data;
            row_bits = rows32[glyph_index + row];
        }

        for (uint16_t col = 0U; col < font->width; col++)
        {
            uint32_t bit = (uint32_t)1UL << (font->width - 1U - col);

            if ((row_bits & bit) != 0UL)
            {
                (void)ST7789_DrawPixel((uint16_t)(x + col), (uint16_t)(y + row), fg);
            }
            else if (!transparent)
            {
                (void)ST7789_DrawPixel((uint16_t)(x + col), (uint16_t)(y + row), bg);
            }
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_DrawText(uint16_t x,
                                  uint16_t y,
                                  const char *text,
                                  const FontDef_t *font,
                                  uint16_t fg,
                                  uint16_t bg,
                                  bool transparent,
                                  uint16_t spacing)
{
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    if ((font == NULL) || (text == NULL))
    {
        return HAL_ERROR;
    }

    while (*text != '\0')
    {
        if (*text == '\n')
        {
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + font->height + 2U);
        }
        else
        {
            (void)ST7789_DrawChar(cursor_x, cursor_y, *text, font, fg, bg, transparent);
            cursor_x = (uint16_t)(cursor_x + font->width + spacing);
        }

        text++;
    }

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_DrawTextCentered(uint16_t x,
                                          uint16_t y,
                                          uint16_t w,
                                          uint16_t h,
                                          const char *text,
                                          const FontDef_t *font,
                                          uint16_t fg,
                                          uint16_t bg,
                                          bool transparent,
                                          uint16_t spacing)
{
    uint16_t draw_x = x;
    uint16_t draw_y = y;
    uint16_t text_w;
    uint16_t text_h;

    if ((font == NULL) || (text == NULL))
    {
        return HAL_ERROR;
    }

    text_w = ST7789_TextWidth(font, text, spacing);
    text_h = font->height;

    if (w > text_w)
    {
        draw_x = (uint16_t)(x + ((w - text_w) / 2U));
    }

    if (h > text_h)
    {
        draw_y = (uint16_t)(y + ((h - text_h) / 2U));
    }

    return ST7789_DrawText(draw_x, draw_y, text, font, fg, bg, transparent, spacing);
}

HAL_StatusTypeDef ST7789_DrawBitmapMono(uint16_t x,
                                        uint16_t y,
                                        const ST7789_BitmapMono_t *bmp,
                                        uint16_t fg,
                                        uint16_t bg,
                                        bool transparent)
{
    uint16_t stride_bytes;

    if ((bmp == NULL) || (bmp->data == NULL))
    {
        return HAL_ERROR;
    }

    stride_bytes = (uint16_t)((bmp->width + 7U) / 8U);

    for (uint16_t row = 0U; row < bmp->height; row++)
    {
        for (uint16_t col = 0U; col < bmp->width; col++)
        {
            uint32_t byte_index = (uint32_t)row * stride_bytes + (col / 8U);
            uint8_t bit_mask = (uint8_t)(0x80U >> (col % 8U));

            if ((bmp->data[byte_index] & bit_mask) != 0U)
            {
                (void)ST7789_DrawPixel((uint16_t)(x + col), (uint16_t)(y + row), fg);
            }
            else if (!transparent)
            {
                (void)ST7789_DrawPixel((uint16_t)(x + col), (uint16_t)(y + row), bg);
            }
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef ST7789_DrawBitmapRGB565(uint16_t x,
                                          uint16_t y,
                                          const ST7789_BitmapRGB565_t *bmp)
{
    if ((bmp == NULL) || (bmp->data == NULL) || (bmp->width == 0U) || (bmp->height == 0U))
    {
        return HAL_ERROR;
    }

    return ST7789_WriteImageRGB565(x, y, bmp->width, bmp->height, bmp->data);
}

HAL_StatusTypeDef ST7789_DrawBitmapRGB565DMA(uint16_t x,
                                             uint16_t y,
                                             const ST7789_BitmapRGB565_t *bmp)
{
    if ((bmp == NULL) || (bmp->data == NULL) || (bmp->width == 0U) || (bmp->height == 0U))
    {
        return HAL_ERROR;
    }

    return ST7789_WriteImageRGB565DMA(x, y, bmp->width, bmp->height, bmp->data);
}

HAL_StatusTypeDef ST7789_WaitForDma(uint32_t timeout)
{
    uint32_t tickstart = HAL_GetTick();

    while (ST7789_IsDmaBusy())
    {
        if (timeout != HAL_MAX_DELAY)
        {
            if ((HAL_GetTick() - tickstart) > timeout)
            {
                return HAL_TIMEOUT;
            }
        }
    }

    return st7789_dma.status;
}

bool ST7789_IsDmaBusy(void)
{
    return st7789_dma.busy;
}

void ST7789_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
    if ((!st7789_dma.busy) || (st7789.hspi != hspi))
    {
        return;
    }

    if (st7789_dma.mode == ST7789_DMA_MODE_DATA)
    {
        ST7789_DMA_Finish(HAL_OK);
    }
    else
    {
        (void)ST7789_DMA_StartNextChunk();
    }
#else
    (void)hspi;
#endif
}

void ST7789_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
#if (ST7789_INTERFACE == ST7789_INTERFACE_SPI)
    if ((!st7789_dma.busy) || (st7789.hspi != hspi))
    {
        return;
    }

    ST7789_DMA_Finish(HAL_ERROR);
#else
    (void)hspi;
#endif
}
