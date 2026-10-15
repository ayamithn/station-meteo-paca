#include "setup.h"
#include "board_pins.h"
#include "stm32l4xx_hal.h"

// Handles attendus par les pilotes du prof (déclarés "extern" dans lib/cesi_grove)
I2C_HandleTypeDef hi2c1;          // bus I2C : RTC et BME680
SPI_HandleTypeDef hspi1;          // bus SPI : carte SD

extern UART_HandleTypeDef huart2; // défini dans board.c (liaison série vers le PC)

// --- I2C1 : PB8 = SCL, PB9 = SDA, environ 100 kHz ---
static void init_i2c1(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_I2C1_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
  gpio.Mode      = GPIO_MODE_AF_OD;          // open-drain, obligatoire en I2C
  gpio.Pull      = GPIO_PULLUP;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &gpio);

  hi2c1.Instance              = I2C1;
  hi2c1.Init.Timing           = 0x00000E14;  // 100 kHz pour une horloge de 4 MHz (testé avec la RTC)
  hi2c1.Init.OwnAddress1      = 0;
  hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2      = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
  HAL_I2C_Init(&hi2c1);
  HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
}

// --- SPI1 pour la carte SD : PA5 = SCK, PA6 = MISO, PA7 = MOSI, CS = PB5 ---
static void init_spi1(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_SPI1_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &gpio);

  // Chip Select de la SD : sortie, au repos à l'état haut
  gpio.Pin       = SD_CS_PIN;
  gpio.Mode      = GPIO_MODE_OUTPUT_PP;
  gpio.Alternate = 0;
  HAL_GPIO_Init(SD_CS_PORT, &gpio);
  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);

  hspi1.Instance               = SPI1;
  hspi1.Init.Mode              = SPI_MODE_MASTER;
  hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;      // mode SPI 0, celui des cartes SD
  hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
  hspi1.Init.NSS               = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;  // environ 250 kHz (lent = fiable)
  hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial     = 7;
  hspi1.Init.CRCLength         = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
  HAL_SPI_Init(&hspi1);
}

// Appelée une seule fois dans main(), juste après Board_Init()
void Global_Init(void)
{
  init_i2c1();
  init_spi1();
}

// --- UART4 du GPS : PA0 et PA1 en fonction alternative 8 ---
// HAL_UART_Init() appelle cette fonction ; GroveGPS_Init() ne configure pas les broches.
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART4)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_UART4_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOA, &gpio);
  }
  // USART2 est déjà configuré à la main dans board.c : rien à faire ici
}

// Redirige printf() vers la liaison série (le pilote SD affiche ses diagnostics avec printf)
int _write(int fd, char *ptr, int len)
{
  (void)fd;
  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
  return len;
}