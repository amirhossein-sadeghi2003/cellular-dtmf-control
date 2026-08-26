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
#include "gfx.h"
#include "ui.h"
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

#define LCD_WIDTH       240U
#define LCD_HEIGHT      320U


#define UI_SCREEN_WIDTH       240U
#define UI_SCREEN_HEIGHT      320U

#define UI_STATUS_BAR_HEIGHT  22U
#define UI_TITLE_BOTTOM_Y     43U
#define UI_CONTENT_TOP        (UI_TITLE_BOTTOM_Y + 10U)
#define UI_FOOTER_LINE_Y      285U



#define LCD_COLOR_BLACK   0x0000U
#define LCD_COLOR_RED     0xF800U
#define LCD_COLOR_GREEN   0x07E0U
#define LCD_COLOR_BLUE    0x001FU
#define LCD_COLOR_WHITE   0xFFFFU
#define LCD_COLOR_GRAY  0x8410U



#define UI_MENU_ITEM_COUNT  7U

#define UI_MENU_FRAME_X     10U
#define UI_MENU_FRAME_Y     49U
#define UI_MENU_FRAME_WIDTH 220U
#define UI_MENU_FRAME_HEIGHT 222U

#define UI_MENU_ITEM_X      16U
#define UI_MENU_FIRST_Y     55U
#define UI_MENU_ITEM_WIDTH  208U
#define UI_MENU_ITEM_HEIGHT 28U
#define UI_MENU_ROW_STEP    30U
#define UI_MENU_TEXT_X      28U
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

static uint8_t key_up_previous = 0U;
static uint8_t key_down_previous = 0U;
static uint8_t key_left_previous = 0U;
static uint8_t key_right_previous = 0U;
static uint8_t key_ok_previous = 0U;




static KeyDebounce_t key_up_db = {0};
static KeyDebounce_t key_down_db = {0};
static KeyDebounce_t key_left_db = {0};
static KeyDebounce_t key_right_db = {0};
static KeyDebounce_t key_ok_db = {0};


static const uint8_t font_5x7[26][7] = {
    {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U}, /* A */
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x11U, 0x1EU}, /* B */
    {0x0EU, 0x11U, 0x10U, 0x10U, 0x10U, 0x11U, 0x0EU}, /* C */
    {0x1EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x1EU}, /* D */
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x1FU}, /* E */
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x10U}, /* F */
    {0x0EU, 0x11U, 0x10U, 0x17U, 0x11U, 0x11U, 0x0FU}, /* G */
    {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U}, /* H */
    {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x1FU}, /* I */
    {0x01U, 0x01U, 0x01U, 0x01U, 0x11U, 0x11U, 0x0EU}, /* J */
    {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x11U}, /* K */
    {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU}, /* L */
    {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x11U}, /* M */
    {0x11U, 0x19U, 0x15U, 0x13U, 0x11U, 0x11U, 0x11U}, /* N */
    {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU}, /* O */
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x10U}, /* P */
    {0x0EU, 0x11U, 0x11U, 0x11U, 0x15U, 0x12U, 0x0DU}, /* Q */
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x11U}, /* R */
    {0x0FU, 0x10U, 0x10U, 0x0EU, 0x01U, 0x01U, 0x1EU}, /* S */
    {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U}, /* T */
    {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU}, /* U */
    {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0AU, 0x04U}, /* V */
    {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x15U, 0x0AU}, /* W */
    {0x11U, 0x11U, 0x0AU, 0x04U, 0x0AU, 0x11U, 0x11U}, /* X */
    {0x11U, 0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x04U}, /* Y */
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x1FU}  /* Z */
};



static const char * const ui_menu_items[UI_MENU_ITEM_COUNT] =
{
    "STATUS",
    "CALL",
    "DTMF",
    "NETWORK",
    "DISPLAY",
    "DIAGNOSTICS",
    "SETTINGS"
};



static UiModel ui_model;

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


static void LCD_FillRectangle(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color)
{
    uint32_t pixel_count;
    uint32_t pixel_index;
    uint8_t color_high;
    uint8_t color_low;

    if ((width == 0U) || (height == 0U)) {
        return;
    }

    if ((x >= LCD_WIDTH) || (y >= LCD_HEIGHT)) {
        return;
    }

    if ((uint32_t)x + width > LCD_WIDTH) {
        width = (uint16_t)(LCD_WIDTH - x);
    }

    if ((uint32_t)y + height > LCD_HEIGHT) {
        height = (uint16_t)(LCD_HEIGHT - y);
    }

    pixel_count =
        (uint32_t)width * height;

    color_high =
        (uint8_t)(color >> 8);

    color_low =
        (uint8_t)(color & 0xFFU);

    LCD_SetAddressWindow(
        x,
        y,
        (uint16_t)(x + width - 1U),
        (uint16_t)(y + height - 1U));

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

static void LCD_DrawCharacter(
    uint16_t x,
    uint16_t y,
    char character,
    uint16_t color,
    uint8_t scale)
{
    uint8_t character_index;
    uint8_t row;
    uint8_t column;
    uint8_t bit_mask;

    if (scale == 0U) {
        return;
    }

    if ((character < 'A') || (character > 'Z')) {
        return;
    }

    character_index =
        (uint8_t)(character - 'A');

    for (row = 0U; row < 7U; row++) {
        for (column = 0U; column < 5U; column++) {
            bit_mask =
                (uint8_t)(1U << (4U - column));

            if ((font_5x7[character_index][row]
                    & bit_mask) != 0U) {
                LCD_FillRectangle(
                    (uint16_t)(
                        x + ((uint16_t)column * scale)),
                    (uint16_t)(
                        y + ((uint16_t)row * scale)),
                    scale,
                    scale,
                    color);
            }
        }
    }
}




static void LCD_DrawString(
    uint16_t x,
    uint16_t y,
    const char *text,
    uint16_t color,
    uint8_t scale)
{
    uint16_t cursor_x;

    if ((text == NULL) || (scale == 0U)) {
        return;
    }

    cursor_x = x;

    while (*text != '\0') {
        LCD_DrawCharacter(
            cursor_x,
            y,
            *text,
            color,
            scale);

        cursor_x = (uint16_t)(
            cursor_x + ((uint16_t)6U * scale));

        text++;
    }
}







static void UI_DrawMenuItem(
    uint8_t item_index,
    uint8_t selected)
{
    uint16_t item_y;
    uint16_t background_color;

    if (item_index >= UI_MENU_ITEM_COUNT)
    {
        return;
    }

    item_y = UI_MENU_FIRST_Y
           + ((uint16_t)item_index * UI_MENU_ROW_STEP);

    if (selected != 0U)
    {
        background_color = LCD_COLOR_BLUE;
    }
    else
    {
        background_color = LCD_COLOR_BLACK;
    }

    LCD_FillRectangle(
        UI_MENU_ITEM_X,
        item_y,
        UI_MENU_ITEM_WIDTH,
        UI_MENU_ITEM_HEIGHT,
        background_color);

    LCD_DrawString(
        UI_MENU_TEXT_X,
        item_y + 7U,
        ui_menu_items[item_index],
        LCD_COLOR_WHITE,
        2U);
}



static void UI_DrawMainMenu(uint8_t selected_item)
{
    uint8_t item_index;

    if (selected_item >= UI_MENU_ITEM_COUNT)
    {
        selected_item = 0U;
    }

    LCD_FillScreen(LCD_COLOR_BLACK);

    /* Status bar */
    LCD_FillRectangle(
        0U,
        0U,
        UI_SCREEN_WIDTH,
        UI_STATUS_BAR_HEIGHT,
        LCD_COLOR_GRAY);

    LCD_DrawString(
        6U,
        7U,
        "MODEM READY",
        LCD_COLOR_BLACK,
        1U);

    LCD_DrawString(
        210U,
        7U,
        "USER",
        LCD_COLOR_BLACK,
        1U);

    /* Page title */
    LCD_DrawString(
        24U,
        26U,
        "CELLULAR CONTROL",
        LCD_COLOR_WHITE,
        2U);

    /* Main menu frame */


    /* Menu items */
    for (item_index = 0U;
         item_index < UI_MENU_ITEM_COUNT;
         item_index++)
    {
        UI_DrawMenuItem(
            item_index,
            item_index == selected_item);
    }

    /* Footer */
    LCD_FillRectangle(
        0U,
        UI_FOOTER_LINE_Y,
        UI_SCREEN_WIDTH,
        1U,
        LCD_COLOR_GRAY);

    LCD_DrawString(
        66U,
        297U,
        "UP DOWN  OK SELECT",
        LCD_COLOR_WHITE,
        1U);
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
  gfxInit();
  uiModelInit(&ui_model);

uiInit(&ui_model);

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

	  if ((key_up_pressed != 0U) &&
	      (key_up_previous == 0U)) {
	      (void)uiHandleKey(UI_KEY_UP);
	  }

	  if ((key_down_pressed != 0U) &&
	      (key_down_previous == 0U)) {
	      (void)uiHandleKey(UI_KEY_DOWN);
	  }

	  if ((key_left_pressed != 0U) &&
	      (key_left_previous == 0U)) {
	      (void)uiHandleKey(UI_KEY_LEFT);
	  }

	  if ((key_right_pressed != 0U) &&
	      (key_right_previous == 0U)) {
	      (void)uiHandleKey(UI_KEY_RIGHT);
	  }

	  if ((key_ok_pressed != 0U) &&
	      (key_ok_previous == 0U)) {
	      (void)uiHandleKey(UI_KEY_ENTER);
	  }

	  key_up_previous = key_up_pressed;
	  key_down_previous = key_down_pressed;
	  key_left_previous = key_left_pressed;
	  key_right_previous = key_right_pressed;
	  key_ok_previous = key_ok_pressed;

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
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
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
