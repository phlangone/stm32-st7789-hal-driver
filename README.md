# ST7789 STM32 HAL Driver

Simple ST7789 TFT display driver for STM32 projects using the HAL library.

The driver provides a small API for display initialization, rotation control, backlight control, pixel drawing, rectangle, circle and triangle primitives, RGB565 image writing, text rendering and basic bitmap drawing.

## Features

- SPI blocking mode
- SPI with optional DMA support
- 8-bit 8080 parallel interface
- Display rotation control
- Backlight control
- RGB565 color support
- Basic drawing primitives
- Rectangle, circle and triangle drawing
- Text drawing support
- RGB565 image writing
- Optional internal GPIO initialization
- Optional display test functions

## Repository structure

```text
st7789-stm32/
├── Inc/
│   ├── st7789.h
│   ├── st7789_conf.h
│   └── fonts.h
│
├── Src/
│   ├── st7789.c
│   └── fonts.c
│
├── README.md
├── LICENSE
└── .gitignore
```

## Adding the driver to STM32CubeIDE

1. Copy the files from `Inc/` to your project `Core/Inc/` folder.
2. Copy the files from `Src/` to your project `Core/Src/` folder.
3. Open `st7789_conf.h` and adjust the interface, display size and pin mapping.
4. Configure the required GPIOs, SPI and DMA in STM32CubeMX when needed.
5. Include the driver in your application:

```c
#include "st7789.h"
```

6. Initialize the display after HAL, system clock and peripherals:

```c
ST7789_Init();
```

## Configuration

All hardware-specific settings are placed in `st7789_conf.h`.

Select the display interface:

```c
#define ST7789_INTERFACE ST7789_INTERFACE_SPI
```

or:

```c
#define ST7789_INTERFACE ST7789_INTERFACE_PARALLEL
```

Configure the display size:

```c
#define ST7789_RAM_WIDTH     240U
#define ST7789_RAM_HEIGHT    320U
#define ST7789_PANEL_WIDTH   240U
#define ST7789_PANEL_HEIGHT  320U
```

Configure the default rotation, color order and inversion mode:

```c
#define ST7789_DEFAULT_ROTATION  ST7789_ROTATION_0
#define ST7789_COLOR_ORDER       ST7789_MADCTL_BGR
#define ST7789_INVERTED          1
```

Enable or disable DMA support:

```c
#define ST7789_USE_DMA 0
```

Keep `ST7789_USE_DMA` disabled when the project does not configure SPI with DMA in STM32CubeMX. Enable it only when using the SPI interface with a configured TX DMA channel:

```c
#define ST7789_USE_DMA 1
```

Optional display test functions can be enabled during board bring-up:

```c
#define ST7789_ENABLE_TESTS 1
```

After validating the display, disable them again:

```c
#define ST7789_ENABLE_TESTS 0
```

## GPIO configuration

GPIOs can be configured either in STM32CubeMX or directly by the driver.

To configure the GPIOs in STM32CubeMX, disable internal GPIO initialization:

```c
#define ST7789_INIT_GPIO 0
```

To let the driver initialize the GPIOs defined in `st7789_conf.h`, enable:

```c
#define ST7789_INIT_GPIO 1
```

When internal GPIO initialization is enabled, make sure the GPIO clock macro matches the ports used by your display:

```c
#define ST7789_GPIO_CLK_ENABLE()       \
    do {                               \
        __HAL_RCC_GPIOA_CLK_ENABLE();  \
        __HAL_RCC_GPIOB_CLK_ENABLE();  \
        __HAL_RCC_GPIOC_CLK_ENABLE();  \
    } while (0)
```

## SPI blocking mode

Configure the SPI peripheral in STM32CubeMX.

Recommended basic settings:

- Master mode
- 8-bit data size
- MOSI and SCK enabled
- Suitable baud rate for your display module
- GPIO outputs for `CS`, `DC`, `RST` and optionally `BL`

In `st7789_conf.h`:

```c
#define ST7789_INTERFACE  ST7789_INTERFACE_SPI
#define ST7789_USE_DMA    0
#define ST7789_SPI_HANDLE hspi2
```

Example:

```c
#include "st7789.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI2_Init();

    ST7789_Init();

    ST7789_FillScreen(ST7789_COLOR565(0, 0, 0));
    ST7789_FillRect(20, 20, 100, 60, ST7789_COLOR565(255, 0, 0));

    while (1)
    {
    }
}
```

## SPI with DMA

Enable the SPI TX DMA channel in STM32CubeMX.

Make sure the generated project includes:

- SPI initialization
- DMA initialization
- DMA interrupt configuration

In `st7789_conf.h`:

```c
#define ST7789_INTERFACE  ST7789_INTERFACE_SPI
#define ST7789_USE_DMA    1
#define ST7789_SPI_HANDLE hspi2
```

Example:

```c
#include "st7789.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI2_Init();

    ST7789_Init();

    ST7789_FillRectDMA(0,
                       0,
                       ST7789_GetWidth(),
                       ST7789_GetHeight(),
                       ST7789_COLOR565(0, 0, 255));

    ST7789_WaitForDma(HAL_MAX_DELAY);

    while (1)
    {
    }
}
```

Forward the HAL SPI callbacks to the driver only when `ST7789_USE_DMA` is enabled:

```c
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    ST7789_SPI_TxCpltCallback(hspi);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    ST7789_SPI_ErrorCallback(hspi);
}
```

You can check the DMA state with:

```c
if (!ST7789_IsDmaBusy())
{
    ST7789_FillRectDMA(10, 10, 50, 50, ST7789_COLOR565(0, 255, 0));
}
```

## 8-bit parallel mode

In parallel mode, the driver uses an 8080-style 8-bit interface with control signals and data lines.

Configure the GPIOs in STM32CubeMX or enable `ST7789_INIT_GPIO` in `st7789_conf.h`.

In `st7789_conf.h`:

```c
#define ST7789_INTERFACE ST7789_INTERFACE_PARALLEL
#define ST7789_USE_DMA   0
```

Configure the control pins:

```c
#define ST7789_CS_PORT GPIOB
#define ST7789_CS_PIN  GPIO_PIN_0

#define ST7789_RS_PORT GPIOA
#define ST7789_RS_PIN  GPIO_PIN_4

#define ST7789_WR_PORT GPIOA
#define ST7789_WR_PIN  GPIO_PIN_1

#define ST7789_RD_PORT GPIOA
#define ST7789_RD_PIN  GPIO_PIN_0
```

Configure the data pins:

```c
#define ST7789_D0_PORT GPIOA
#define ST7789_D0_PIN  GPIO_PIN_9

#define ST7789_D1_PORT GPIOC
#define ST7789_D1_PIN  GPIO_PIN_7

#define ST7789_D2_PORT GPIOA
#define ST7789_D2_PIN  GPIO_PIN_10

#define ST7789_D3_PORT GPIOB
#define ST7789_D3_PIN  GPIO_PIN_3

#define ST7789_D4_PORT GPIOB
#define ST7789_D4_PIN  GPIO_PIN_5

#define ST7789_D5_PORT GPIOB
#define ST7789_D5_PIN  GPIO_PIN_4

#define ST7789_D6_PORT GPIOB
#define ST7789_D6_PIN  GPIO_PIN_10

#define ST7789_D7_PORT GPIOA
#define ST7789_D7_PIN  GPIO_PIN_8
```

Example:

```c
#include "st7789.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();

    ST7789_Init();

    ST7789_FillScreen(ST7789_COLOR565(0, 0, 0));
    ST7789_DrawPixel(10, 10, ST7789_COLOR565(255, 255, 255));
    ST7789_FillRect(30, 30, 120, 80, ST7789_COLOR565(255, 0, 0));

    while (1)
    {
    }
}
```

DMA functions are not implemented for the parallel interface. If desired, they can fallback to blocking functions:

```c
#define ST7789_PARALLEL_DMA_FALLBACK_BLOCKING 1
```

## Basic drawing

```c
ST7789_FillScreen(ST7789_COLOR565(0, 0, 0));
ST7789_DrawPixel(10, 10, ST7789_COLOR565(255, 255, 255));
ST7789_DrawFastHLine(20, 40, 100, ST7789_COLOR565(255, 0, 0));
ST7789_DrawFastVLine(20, 40, 100, ST7789_COLOR565(0, 255, 0));
ST7789_DrawRect(10, 10, 80, 40, ST7789_COLOR565(255, 255, 255));
ST7789_FillRect(100, 50, 60, 60, ST7789_COLOR565(0, 0, 255));
ST7789_DrawCircle(80, 160, 30, ST7789_COLOR565(255, 255, 0));
ST7789_FillCircle(160, 160, 30, ST7789_COLOR565(0, 255, 255));
ST7789_DrawTriangle(20, 250, 100, 200, 180, 250, ST7789_COLOR565(255, 0, 255));
ST7789_FillTriangle(120, 250, 180, 200, 230, 250, ST7789_COLOR565(0, 255, 0));
```

## Text drawing

```c
ST7789_DrawText(10,
                10,
                "Hello ST7789",
                &Font_11x18,
                ST7789_COLOR565(255, 255, 255),
                ST7789_COLOR565(0, 0, 0),
                false,
                1);
```

Use transparent background when you do not want the function to repaint inactive font pixels:

```c
ST7789_DrawText(10,
                40,
                "Transparent text",
                &Font_11x18,
                ST7789_COLOR565(255, 255, 0),
                ST7789_COLOR565(0, 0, 0),
                true,
                1);
```

## RGB565 images

```c
extern const uint16_t image_data[];

ST7789_WriteImageRGB565(0, 0, 100, 100, image_data);
```

With DMA:

```c
extern const uint16_t image_data[];

ST7789_WriteImageRGB565DMA(0, 0, 100, 100, image_data);
ST7789_WaitForDma(HAL_MAX_DELAY);
```

## Optional display tests

The driver can compile optional test helpers for quick display validation.

Enable them in `st7789_conf.h`:

```c
#define ST7789_ENABLE_TESTS 1
```

Use the test functions from your application after `ST7789_Init()`:

```c
ST7789_Init();

ST7789_Test_ColorBars();
HAL_Delay(1000);

ST7789_Test_Shapes();
HAL_Delay(1000);

ST7789_Test_Rotation();
HAL_Delay(1000);

ST7789_Test_Full(&Font_11x18);
```

Available test helpers:

```c
ST7789_Test_ColorBars();
ST7789_Test_Shapes();
ST7789_Test_Rotation();
ST7789_Test_Text(&Font_11x18);
ST7789_Test_Full(&Font_11x18);
```

You may pass `NULL` to `ST7789_Test_Full()` to skip the text test:

```c
ST7789_Test_Full(NULL);
```

Disable the tests after validating the display:

```c
#define ST7789_ENABLE_TESTS 0
```

## Troubleshooting

### Build error related to `SPI_HandleTypeDef`

If your project does not use SPI with DMA and you see errors related to `SPI_HandleTypeDef`, keep DMA support disabled:

```c
#define ST7789_USE_DMA 0
```

Only enable it when SPI and TX DMA are configured in STM32CubeMX.

### Shapes appear correctly, but text appears mirrored

If geometric primitives look correct but text appears horizontally mirrored, the issue is probably related to the bit order expected by the selected font data. Check the bit-reading logic used by the font renderer and confirm that the font table stores the leftmost pixel in the expected bit position.

### Everything appears mirrored or rotated incorrectly

If text and shapes are both mirrored or rotated incorrectly, adjust:

```c
#define ST7789_DEFAULT_ROTATION ST7789_ROTATION_0
```

and review the `MADCTL` configuration used by the selected rotation.

### Wrong colors

If red and blue appear swapped, try changing the color order:

```c
#define ST7789_COLOR_ORDER ST7789_MADCTL_RGB
```

or:

```c
#define ST7789_COLOR_ORDER ST7789_MADCTL_BGR
```

### Parallel interface shows corrupted pixels

Check the data bus mapping carefully. `D0` on the display must match `ST7789_D0_*`, `D1` must match `ST7789_D1_*`, and so on up to `D7`.

## Notes

- SPI and DMA peripherals must be configured in STM32CubeMX when used.
- Keep `ST7789_USE_DMA` disabled when DMA is not configured.
- GPIOs may be configured in STM32CubeMX or internally by the driver.
- Image data must be in RGB565 format.
- Text functions require a compatible `FontDef_t` font definition.
- The configuration file must match your hardware wiring.
- Optional test functions are intended for validation and can be disabled after bring-up.

## License

MIT
