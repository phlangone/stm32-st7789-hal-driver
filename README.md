# ST7789 STM32 Driver

Biblioteca simples para controle de displays TFT com controlador **ST7789** em projetos STM32 usando HAL.

A proposta da biblioteca é fornecer uma API direta, sem `handle` público, permitindo inicializar o display, controlar rotação, backlight, desenhar pixels, preencher áreas, escrever imagens RGB565, textos e bitmaps simples.

A biblioteca suporta três formas principais de uso:

- SPI em modo bloqueante;
- SPI com DMA;
- Interface paralela 8080 de 8 bits.

## Objetivos da biblioteca

- Facilitar o uso de displays ST7789 em projetos STM32.
- Centralizar a configuração do display no arquivo `st7789_conf.h`.
- Permitir uso tanto por SPI quanto por barramento paralelo.
- Oferecer funções básicas de desenho para aplicações simples.
- Permitir aceleração por DMA quando o display estiver usando SPI.
- Manter uma API simples para uso direto no `main.c` ou em módulos gráficos externos.

## Arquivos principais

| Arquivo | Descrição |
|---|---|
| `st7789.h` | API pública da biblioteca |
| `st7789.c` | Implementação do driver |
| `st7789_conf.h` | Configuração de interface, pinos, dimensões e opções |
| `fonts.h` | Definições de fontes usadas pelas funções de texto |

## Recursos disponíveis

- Inicialização do display.
- Reset via GPIO.
- Controle de backlight.
- Configuração de rotação.
- Escrita de comandos e dados.
- Escrita de pixels.
- Desenho de linhas horizontais e verticais.
- Desenho de retângulos.
- Preenchimento de retângulos.
- Preenchimento da tela.
- Escrita de imagens RGB565.
- Escrita de textos.
- Escrita de bitmaps monocromáticos.
- Escrita de bitmaps RGB565.
- Escrita com DMA no modo SPI.

## Configuração geral

A configuração principal da biblioteca fica no arquivo `st7789_conf.h`.

Nesse arquivo é possível selecionar a interface física utilizada pelo display.

Para usar SPI:

```c
#define ST7789_INTERFACE ST7789_INTERFACE_SPI
```

Para usar interface paralela 8080 de 8 bits:

```c
#define ST7789_INTERFACE ST7789_INTERFACE_PARALLEL
```

Também é nesse arquivo que são configurados:

- dimensões do display;
- offsets por rotação;
- ordem de cor RGB ou BGR;
- estado inicial de inversão;
- timeout de comunicação;
- tamanho dos blocos de transmissão;
- pinos de controle;
- pinos de dados no modo paralelo;
- opção de inicialização interna dos GPIOs.

## Configuração pelo STM32CubeMX

A biblioteca pode ser usada junto com projetos gerados pelo **STM32CubeMX** ou **STM32CubeIDE**.

No caso do modo SPI, recomenda-se configurar o periférico SPI pelo CubeMX, selecionando:

- SPI em modo `Full-Duplex Master` ou `Transmit Only Master`;
- tamanho de dado em 8 bits;
- baud rate adequado ao display;
- polaridade e fase conforme o módulo utilizado;
- pino SCK;
- pino MOSI;
- opcionalmente o DMA para transmissão.

Os pinos de controle, como `CS`, `DC`/`RS`, `RST` e `BL`, também podem ser configurados pelo CubeMX como saídas digitais.

Se preferir configurar os GPIOs pelo CubeMX, basta desabilitar a inicialização interna da biblioteca no arquivo `st7789_conf.h`:

```c
#define ST7789_INIT_GPIO 0
```

Se quiser que a própria biblioteca inicialize os GPIOs informados no arquivo de configuração, mantenha:

```c
#define ST7789_INIT_GPIO 1
```

Nesse caso, também é necessário garantir que a macro de clock dos GPIOs esteja correta:

```c
#define ST7789_GPIO_CLK_ENABLE()       \
    do {                               \
        __HAL_RCC_GPIOA_CLK_ENABLE();  \
        __HAL_RCC_GPIOB_CLK_ENABLE();  \
        __HAL_RCC_GPIOC_CLK_ENABLE();  \
    } while (0)
```

## Uso em modo SPI normal

No modo SPI normal, a comunicação é feita de forma bloqueante usando `HAL_SPI_Transmit`.

No `st7789_conf.h`, selecione a interface SPI:

```c
#define ST7789_INTERFACE ST7789_INTERFACE_SPI
```

Configure o handle SPI usado pela biblioteca:

```c
#define ST7789_SPI_HANDLE hspi2
```

Configure os pinos de controle:

```c
#define ST7789_CS_PORT GPIOA
#define ST7789_CS_PIN  GPIO_PIN_4

#define ST7789_DC_PORT GPIOB
#define ST7789_DC_PIN  GPIO_PIN_0

#define ST7789_RST_PORT GPIOC
#define ST7789_RST_PIN  GPIO_PIN_1

#define ST7789_BL_PORT NULL
#define ST7789_BL_PIN  0U
```

Exemplo simples de uso:

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
    ST7789_DrawRect(10, 10, 140, 100, ST7789_COLOR565(255, 255, 255));

    while (1)
    {
    }
}
```

## Uso em modo SPI com DMA

No modo SPI com DMA, a transmissão dos dados pode ser feita de forma não bloqueante.

A configuração inicial é semelhante ao SPI normal, porém o DMA de transmissão do SPI deve ser habilitado no **STM32CubeMX**.

No CubeMX, configure:

- o periférico SPI;
- o canal DMA de transmissão do SPI;
- as interrupções relacionadas ao DMA;
- a interrupção do SPI, se necessário para o projeto.

No `st7789_conf.h`, selecione SPI:

```c
#define ST7789_INTERFACE ST7789_INTERFACE_SPI
#define ST7789_SPI_HANDLE hspi2
```

Também é possível ajustar o tamanho do bloco de transmissão:

```c
#define ST7789_TX_CHUNK_SIZE 256U
```

Exemplo usando preenchimento com DMA:

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

    ST7789_FillScreen(ST7789_COLOR565(0, 0, 0));

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

Para que o DMA funcione corretamente, as callbacks da HAL devem chamar as callbacks da biblioteca:

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

Também é possível verificar se ainda existe uma transmissão DMA em andamento:

```c
if (!ST7789_IsDmaBusy())
{
    ST7789_FillRectDMA(10, 10, 50, 50, ST7789_COLOR565(0, 255, 0));
}
```

## Uso em modo paralelo 8080 de 8 bits

No modo paralelo, o display usa sinais de controle e oito linhas de dados.

No `st7789_conf.h`, selecione a interface paralela:

```c
#define ST7789_INTERFACE ST7789_INTERFACE_PARALLEL
```

Configure os pinos de controle:

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

Configure os pinos de dados:

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

Exemplo simples de uso:

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
    ST7789_DrawFastHLine(20, 150, 200, ST7789_COLOR565(0, 255, 0));

    while (1)
    {
    }
}
```

No modo paralelo, as funções DMA podem usar fallback bloqueante se a opção estiver habilitada:

```c
#define ST7789_PARALLEL_DMA_FALLBACK_BLOCKING 1
```

Se essa opção estiver desabilitada, funções como `ST7789_FillRectDMA` retornam erro no modo paralelo:

```c
#define ST7789_PARALLEL_DMA_FALLBACK_BLOCKING 0
```

## Exemplo com rotação

```c
ST7789_SetRotation(ST7789_ROTATION_90);

ST7789_FillScreen(ST7789_COLOR565(0, 0, 0));
ST7789_FillRect(10, 10, 100, 50, ST7789_COLOR565(255, 0, 0));
```

As dimensões atuais do display podem ser consultadas com:

```c
uint16_t width = ST7789_GetWidth();
uint16_t height = ST7789_GetHeight();
```

## Exemplo com texto

```c
ST7789_FillScreen(ST7789_COLOR565(0, 0, 0));

ST7789_DrawText(10,
                10,
                "Hello ST7789",
                &Font_11x18,
                ST7789_COLOR565(255, 255, 255),
                ST7789_COLOR565(0, 0, 0),
                false,
                1);
```

Para texto com fundo transparente:

```c
ST7789_DrawText(10,
                40,
                "Texto transparente",
                &Font_11x18,
                ST7789_COLOR565(0, 255, 0),
                ST7789_COLOR565(0, 0, 0),
                true,
                1);
```

## Exemplo com imagem RGB565

```c
extern const uint16_t image_data[];

ST7789_WriteImageRGB565(0, 0, 100, 100, image_data);
```

Com DMA no modo SPI:

```c
extern const uint16_t image_data[];

ST7789_WriteImageRGB565DMA(0, 0, 100, 100, image_data);
ST7789_WaitForDma(HAL_MAX_DELAY);
```

## Cores RGB565

A biblioteca fornece a macro `ST7789_COLOR565` para converter valores RGB de 8 bits para RGB565:

```c
uint16_t red   = ST7789_COLOR565(255, 0, 0);
uint16_t green = ST7789_COLOR565(0, 255, 0);
uint16_t blue  = ST7789_COLOR565(0, 0, 255);
uint16_t white = ST7789_COLOR565(255, 255, 255);
uint16_t black = ST7789_COLOR565(0, 0, 0);
```

## Observações importantes

- O periférico SPI deve ser configurado previamente pelo CubeMX ou manualmente.
- O DMA, quando usado, também deve ser configurado previamente pelo CubeMX.
- Os GPIOs podem ser configurados pelo CubeMX ou pela própria biblioteca.
- Para deixar os GPIOs sob responsabilidade do CubeMX, use `ST7789_INIT_GPIO` como `0`.
- Para deixar a biblioteca inicializar os GPIOs, use `ST7789_INIT_GPIO` como `1`.
- No modo SPI com DMA, é necessário repassar as callbacks da HAL para a biblioteca.
- No modo paralelo, a escrita é feita por GPIO usando o protocolo 8080 de 8 bits.
- As imagens devem estar em formato RGB565.
- As funções de texto dependem de fontes compatíveis com `FontDef_t`.

## Sequência básica recomendada

```c
HAL_Init();
SystemClock_Config();

MX_GPIO_Init();
MX_DMA_Init();
MX_SPI2_Init();

ST7789_Init();

ST7789_FillScreen(ST7789_COLOR565(0, 0, 0));
```

No modo paralelo, `MX_DMA_Init()` e `MX_SPIx_Init()` não são necessários, a menos que o projeto use esses periféricos para outras finalidades.

## Licença

Defina aqui a licença do projeto conforme sua necessidade.

Exemplo:

```text
MIT License
```
