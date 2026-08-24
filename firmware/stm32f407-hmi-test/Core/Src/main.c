/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
	KEY_EVENT_NONE = 0,
	 KEY_EVENT_UP,
	 kEY_EVENT_DOWN,
	 KEY_EVENT_RIGHT,
	 KEY_EVENT_ENTER
} KeyEvent_t;

typedef struct {
	uint8_t last_raw_state;
	uint8_t stable_state;
	uint8_t stable_count;
} KeyDebounce_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define KEY_DEBOUNCE_SAMPLES 3U

#define ILI9341_CMD_SOFTWARE_RESET          0x01U
#define ILI9341_CMD_SLEEP_OUT               0x11U
#define ILI9341_CMD_DISPLAY_OFF             0x28U
#define ILI9341_CMD_DISPLAY_ON              0x29U
#define ILI9341_CMD_COLUMN_ADDRESS_SET      0x2AU
#define ILI9341_CMD_PAGE_ADDRESS_SET        0x2BU
#define ILI9341_CMD_MEMORY_WRITE            0x2CU
#define ILI9341_CMD_MEMORY_ACCESS_CONTROL   0x36U
#define ILI9341_CMD_PIXEL_FORMAT_SET        0x3AU

#define LCD_WIDTH   240U
#define LCD_HEIGHT  320U



#define LCD_COLOR_BLACK   0x0000U
#define LCD_COLOR_RED     0xF800U
#define LCD_COLOR_GREEN   0x07E0U
#define LCD_COLOR_BLUE    0x001FU
#define LCD_COLOR_WHITE   0xFFFFU
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */
volatile uint8_t key_up_pressed = 0;
volatile uint8_t key_down_pressed = 0;
volatile uint8_t key_left_pressed = 0;
volatile uint8_t key_right_pressed = 0;
volatile uint8_t key_ok_pressed = 0;

static KeyDebounce_t key_up_db = {0};
static KeyDebounce_t key_down_db = {0};
static KeyDebounce_t key_left_db = {0};
static KeyDebounce_t key_right_db = {0};
static KeyDebounce_t key_ok_db = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
static uint8_t Key_ReadDebounced(KeyDebounce_t *state,
							GPIO_TypeDef  *gpio_port, uint16_t gpio_pin);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint8_t Key_ReadDebounced(
    KeyDebounce_t *state,
    GPIO_TypeDef *gpio_port,
    uint16_t gpio_pin)
{
    uint8_t raw_state;

    if (HAL_GPIO_ReadPin(
            gpio_port,
            gpio_pin) == GPIO_PIN_RESET) {
        raw_state = 1U;
    } else {
        raw_state = 0U;
    }

    if (raw_state == state->last_raw_state) {
        if (state->stable_count < KEY_DEBOUNCE_SAMPLES) {
            state->stable_count++;
        }
    } else {
        state->last_raw_state = raw_state;
        state->stable_count = 1U;
    }

    if (state->stable_count >= KEY_DEBOUNCE_SAMPLES) {
        state->stable_state = raw_state;
    }

    return state->stable_state;
}

static void LCD_Select(void)
{
    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_RESET);
}

static void LCD_Unselect(void)
{
    HAL_GPIO_WritePin(
        LCD_CS_GPIO_Port,
        LCD_CS_Pin,
        GPIO_PIN_SET);
}

static void LCD_CommandMode(void)
{
    HAL_GPIO_WritePin(
        LCD_DC_GPIO_Port,
        LCD_DC_Pin,
        GPIO_PIN_RESET);
}

static void LCD_DataMode(void)
{
    HAL_GPIO_WritePin(
        LCD_DC_GPIO_Port,
        LCD_DC_Pin,
        GPIO_PIN_SET);
}

static void LCD_HardwareReset(void)
{
    HAL_GPIO_WritePin(
        LCD_RST_GPIO_Port,
        LCD_RST_Pin,
        GPIO_PIN_RESET);

    HAL_Delay(20);

    HAL_GPIO_WritePin(
        LCD_RST_GPIO_Port,
        LCD_RST_Pin,
        GPIO_PIN_SET);

    HAL_Delay(120);
}

static void LCD_SPI_WriteByte(uint8_t value)
{
    if (HAL_SPI_Transmit(
            &hspi2,
            &value,
            1U,
            HAL_MAX_DELAY) != HAL_OK) {
        Error_Handler();
    }
}

static void LCD_WriteCommand(uint8_t command)
{
    LCD_CommandMode();
    LCD_Select();

    LCD_SPI_WriteByte(command);

    LCD_Unselect();
}

static void LCD_WriteData(uint8_t data)
{
    LCD_DataMode();
    LCD_Select();

    LCD_SPI_WriteByte(data);

    LCD_Unselect();
}


static void LCD_Init(void)
{
    LCD_Unselect();

    LCD_HardwareReset();

    LCD_WriteCommand(
        ILI9341_CMD_SOFTWARE_RESET);

    HAL_Delay(150);

    LCD_WriteCommand(
        ILI9341_CMD_DISPLAY_OFF);

    LCD_WriteCommand(
        ILI9341_CMD_PIXEL_FORMAT_SET);

    LCD_WriteData(0x55U);

    LCD_WriteCommand(
        ILI9341_CMD_MEMORY_ACCESS_CONTROL);

    LCD_WriteData(0x48U);

    LCD_WriteCommand(
        ILI9341_CMD_SLEEP_OUT);

    HAL_Delay(120);

    LCD_WriteCommand(
        ILI9341_CMD_DISPLAY_ON);

    HAL_Delay(20);
}



static void LCD_SetAddressWindow(
    uint16_t x_start,
    uint16_t y_start,
    uint16_t x_end,
    uint16_t y_end)
{
    LCD_WriteCommand(
        ILI9341_CMD_COLUMN_ADDRESS_SET);

    LCD_WriteData((uint8_t)(x_start >> 8));
    LCD_WriteData((uint8_t)(x_start & 0xFFU));
    LCD_WriteData((uint8_t)(x_end >> 8));
    LCD_WriteData((uint8_t)(x_end & 0xFFU));

    LCD_WriteCommand(
        ILI9341_CMD_PAGE_ADDRESS_SET);

    LCD_WriteData((uint8_t)(y_start >> 8));
    LCD_WriteData((uint8_t)(y_start & 0xFFU));
    LCD_WriteData((uint8_t)(y_end >> 8));
    LCD_WriteData((uint8_t)(y_end & 0xFFU));

    LCD_WriteCommand(
        ILI9341_CMD_MEMORY_WRITE);
}


static void LCD_FillScreen(uint16_t color)
{
    uint32_t pixel_count;
    uint32_t pixel_index;
    uint8_t color_high;
    uint8_t color_low;

    pixel_count =
        (uint32_t)LCD_WIDTH * LCD_HEIGHT;

    color_high =
        (uint8_t)(color >> 8);

    color_low =
        (uint8_t)(color & 0xFFU);

    LCD_SetAddressWindow(
        0U,
        0U,
        LCD_WIDTH - 1U,
        LCD_HEIGHT - 1U);

    LCD_DataMode();
    LCD_Select();

    for (pixel_index = 0U;
         pixel_index < pixel_count;
         pixel_index++) {
        LCD_SPI_WriteByte(color_high);
        LCD_SPI_WriteByte(color_low);
    }

    LCD_Unselect();
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
LCD_Init();
LCD_FillScreen(LCD_COLOR_RED);
LCD_FillScreen(LCD_COLOR_BLACK);
LCD_FillScreen(LCD_COLOR_WHITE);
LCD_FillScreen(LCD_COLOR_BLUE);
LCD_FillScreen(LCD_COLOR_GREEN);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  key_up_pressed = Key_ReadDebounced(
	      &key_up_db,
	      KEY_UP_GPIO_Port,
	      KEY_UP_Pin);

	  key_down_pressed = Key_ReadDebounced(
	      &key_down_db,
	      KEY_DOWN_GPIO_Port,
	      KEY_DOWN_Pin);

	  key_left_pressed = Key_ReadDebounced(
	      &key_left_db,
	      KEY_LEFT_GPIO_Port,
	      KEY_LEFT_Pin);

	  key_right_pressed = Key_ReadDebounced(
	      &key_right_db,
	      KEY_RIGHT_GPIO_Port,
	      KEY_RIGHT_Pin);

	  key_ok_pressed = Key_ReadDebounced(
	      &key_ok_db,
	      KEY_OK_GPIO_Port,
	      KEY_OK_Pin);

	  HAL_Delay(10);

	  HAL_Delay(10);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_1LINE;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : LCD_DC_Pin LCD_CS_Pin */
  GPIO_InitStruct.Pin = LCD_DC_Pin|LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RST_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY_UP_Pin KEY_DOWN_Pin KEY_LEFT_Pin KEY_RIGHT_Pin */
  GPIO_InitStruct.Pin = KEY_UP_Pin|KEY_DOWN_Pin|KEY_LEFT_Pin|KEY_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_OK_Pin */
  GPIO_InitStruct.Pin = KEY_OK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY_OK_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
