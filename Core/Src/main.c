#include "main.h"
#include <stdint.h>
#include <stddef.h>
#define SOLENOID	GPIO_PIN_6		// Pin B6
#define S_PORT     GPIOB
#define FIFO_SIZE 601 // size of longest track
#define LENGTH1 60
#define LENGTH2 39

// #define FifoTrackOneSize 601 // 69 for track one and \0--> null terminator  |  60 bits time 7 is 600
// #define FifoTrackTwoSize 391 // 30 for track 2 plus \0--> null terminator  |  39 bits time 7 is 390

#define NUM_FIFOS 2

int coil_polarity = 0;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

//for FIFO use
char Buffer[1];
unsigned short flags;
char *Putpt; // FIFO Put pointer
char *Getpt; // FIFO Get pointer

char* Putpts[NUM_FIFOS];
char* Getpts[NUM_FIFOS];

char* Fifos[NUM_FIFOS][FIFO_SIZE];

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);  
static void MX_USART3_UART_Init(void);
//%B1234567890123456^DOE/JOHN           ^25051234567890000000?  <-- track 1 example
//;1234567890123456=25051234567890000000? <-- track 2 example

// only ASCII 0x20–0x7F are used on Track 1; adjust size if you need more
static const uint8_t track1_lut[128] = {
    [' '] = 0x40,
    ['!'] = 0x01, ['"'] = 0x02, ['#'] = 0x43, ['$'] = 0x04,
    ['%'] = 0x45, ['&'] = 0x46, ['\'']= 0x07, ['('] = 0x08,
    [')'] = 0x49, ['*'] = 0x4A, ['+'] = 0x0B, [','] = 0x4C,
    ['-'] = 0x0D, ['.'] = 0x0E, ['/'] = 0x4F,
    ['0'] =   , ['1'] = 0x51, ['2'] = 0x52, ['3'] = 0x13,
    ['4'] = 0x54, ['5'] = 0x15, ['6'] = 0x16, ['7'] = 0x57,
    ['8'] = 0x58, ['9'] = 0x19,
    [':'] = 0x1A, [';'] = 0x5B, ['<'] = 0x1C, ['='] = 0x5D,
    ['>'] = 0x5E, ['?'] = 0x1F, ['@'] = 0x20,
    ['A'] = 0x61, ['B'] = 0x62, ['C'] = 0x23, ['D'] = 0x64,
    ['E'] = 0x25, ['F'] = 0x26, ['G'] = 0x67, ['H'] = 0x68,
    ['I'] = 0x29, ['J'] = 0x2A, ['K'] = 0x6B, ['L'] = 0x2C,
    ['M'] = 0x6D, ['N'] = 0x6E, ['O'] = 0x2F, ['P'] = 0x70,
    ['Q'] = 0x31, ['R'] = 0x32, ['S'] = 0x73, ['T'] = 0x34,
    ['U'] = 0x75, ['V'] = 0x76, ['W'] = 0x37, ['X'] = 0x38,
    ['Y'] = 0x79, ['Z'] = 0x7A, ['['] = 0x3B, ['\\']= 0x7C,
    [']'] = 0x3D, ['^'] = 0x3E, ['_'] = 0x7F,
    // everything else defaults to 0x00 (you can treat that as "invalid")
};

static const uint8_t track2_lut[128] = {
    ['0'] = 0x10,  // data=0x0, parity=1
    ['1'] = 0x01,  // data=0x1, parity=0
    ['2'] = 0x02,  // data=0x2, parity=0
    ['3'] = 0x13,  // data=0x3, parity=1
    ['4'] = 0x04,  // data=0x4, parity=0
    ['5'] = 0x15,  // data=0x5, parity=1
    ['6'] = 0x16,  // data=0x6, parity=1
    ['7'] = 0x07,  // data=0x7, parity=0
    ['8'] = 0x08,  // data=0x8, parity=0
    ['9'] = 0x19,  // data=0x9, parity=1
    [';'] = 0x0B,  // start sentinel, data=0xB, parity=0
    ['='] = 0x0D,  // field separator, data=0xD, parity=0
    ['?'] = 0x1F,  // end sentinel, data=0xF, parity=1
    // everything else = 0x00 (invalid)
};


void toggle_coil_polarity() {
  coil_polarity = !coil_polarity;
  HAL_GPIO_WritePin(S_PORT, SOLENOID, coil_polarity ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void send_one() {
  toggle_coil_polarity();
  // schedule to toggle at half_way of timer
  sTimer[HALF_BLOCK] = HALF_BLOCK_TIME;
  sTimer[FULL_BLOCK] = FULL_BLOCK_TIME;
}

void send_zero() {
  toggle_coil_polarity();
  sTimer[FULL_BLOCK] = FULL_BLOCK_TIME;
}

void send_track(int index) {
  while (!FifoIsEmpty(index)) {
    if (sTimer[HALF_BLOCK] == 0) {
      toggle_coil_polarity();
    }
    
    if (sTimer[FULL_BLOCK] == 0) {
      if (GetFifo(0, Buffer) == 0) {
        send_zero();
      } else {
        send_one();
      }
    }
  }
}
//iterating through each character and then for each character 7 bits (disregard the MSB)
void load_track_one(char* track, int length) {
  char temp[FIFO_SIZE];
  for (int i = 0;  i < length;  i++) {2
    temp[k]=track1_lut[track[k]];
  }

  for (int i = 0;  i < length;  i++) {
    for (int j = 0; j < 7; j++) {
      putFifo(0, (track[i] >> j) & 1);
    }
  }
}

void load_track_two(char* track, int length) {
  char temp[FIFO_SIZE];
  for (int k = 0; k < length; k++) {
    // Convert to BYTE mapping of characters for magnetic tracks (from ASCII)
    temp[k] = track2_lut[track[k]];
  }

  // Queue each bit of the converted BYTEs in the 
  for (int i = 0;  i < length;  i++) {
      for (int j = 0; j < 5; j++) {
        putFifo(0, (temp[i] >> j) & 1);
      }
  } 
}

void FifoInit(int index){
  Putpt[index] = Getpt[index] = &Fifos[index][0];
}

int GetFifo(int index, char *dataPt){
  if (Putpt[index] == Getpt[index])
    return 0; // Fifo empty
  else{
    *dataPt[index] = *(Getpt[index])++; // post increment after assignment
    if (Getpt[index] == &Fifos[index][FIFO_SIZE])
      Getpt[index] = Fifos[index];
      return -1; // successfully
  }
}

int FifoIsEmpty(int index) {
  return !(Putpt[index] == Getpt[index]);
}

int PutFifo(int index, char data){
  char *Ppt; // Put pointer temporary use
  Ppt = Putpt[index]; // Save a copy of Putpt
  *Ppt++ = data; // Put into Fifo
  //HAL_UART_Transmit(&huart2, (uint8_t *)(Ppt-1), 1, HAL_MAX_DELAY); // echo the rec'd char
  //HAL_UART_Transmit(&huart2, (uint8_t *)"\n\r", 2, HAL_MAX_DELAY);
  if (Ppt == &Fifos[index][FIFO_SIZE])
    Ppt = &Fifo[0]; // wrap to Fifo top
  if (Ppt == Getpt[index]){
    //HAL_UART_Transmit(&huart2, (uint8_t *)"Received but FIFO full!\n\r",25, HAL_MAX_DELAY);
    return 0;
  }else{
    Putpt[index] = Ppt;
    //HAL_UART_Transmit(&huart2, (uint8_t *)"Received OK.\n\r", 15, HAL_MAX_DELAY);
    return -1; // successfully
  }
}

int main(void)
{

  HAL_Init();

  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();

  while (1)
  {
	  HAL_Delay(10);
	  HAL_GPIO_WritePin(GPIOB, SOLENOID, 1);
	  HAL_Delay(10);
	  HAL_GPIO_WritePin(GPIOB, SOLENOID, 0);

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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 3999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_DOWN;
  htim2.Init.Period = 19;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV2;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_8
                          |GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC0 PC1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA4 PA8
                           PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_8
                          |GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB3 PB4 PB5
                           PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

#ifdef  USE_FULL_ASSERT
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
