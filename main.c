
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"
#include "Timer.h"
#include "LCD.h"

#define MAX_FIELD_LEN 64

#define CS GPIO_PIN_6
#define SK GPIO_PIN_5
#define EEPROM_START 0x01
#define EEPROM_EWEN 0x3F
#define EEPROM_WRITE 0x40
#define EEPROM_EWDS 0x0F
#define EEPROM_READ 0x80
#define PRESSED 0

#define ButtonHeld 0x0001
#define LCD_Reset 0x0001
#define LCD_Start 0x0002

unsigned short sButtonStatus;
unsigned short sLCDflags;

SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

uint8_t EE_Data[3];

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM5_Init(void);
//--------BLUE button things
bool Nucleo_button_pressed();
bool Nucleo_button_pushed_verbose();
char ClearScreen[] = { 0x1B, '[', '2' , 'J',0 }; 	// Clear the screen
char CursorHome[] = { 0x1B, '[' , 'H' , 0 }; 	// Home the cursor
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	TIMER2_HANDLE();
}
typedef enum{
	PUSHED,RELEASED,P2R,R2P
}button_state_t;

//void Delay_us(uint32_t us);
void Delay_1_plus_us(void);
void EEPROM_READ_FUN(uint8_t EE_Addr);
void EEPROM_READ_STRING(char *buffer, uint8_t eeprom_chunks);
void EEPROM_SEND_STRING(char *data, int address);
void EEPROM_SEND(char EE_Addr, char EE_Data1, char EE_Data2);


void UART_send(UART_HandleTypeDef *huart, char buffer[])
{
    HAL_UART_Transmit(huart, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
 }

void readLine(char *buf, int maxLen) {
    int idx = 0;
    char ch;
    while (1) {
        HAL_UART_Receive(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
        // if CR or LF, end input
        if (ch == '\r' || ch == '\n') break;
        // store and echo
        if (idx < maxLen - 1) {
            buf[idx++] = ch;
            HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
        }
    }
    buf[idx] = '\0';
    // echo newline
    const char crlf[] = "\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)crlf, 2, HAL_MAX_DELAY);
}

    //EEPROM_READ_FUN(0);
//    char input[]="STRINGTHINGSstrings";
//    EEPROM_SEND_STRING(input);
//    int num_chunks = (strlen(input) % 2 == 0) ? (strlen(input) / 2) : (strlen(input) / 2 + 1);
//    char buffer[20];

//    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
//    HAL_UART_Transmit(&huart2, (uint8_t *)"\n\r", 2, HAL_MAX_DELAY);


//contains the adresses of the card ingo
typedef struct {
    uint16_t nickname_addr;
    uint16_t track1_addr;
    uint16_t track2_addr;
} CardInfo;
//array for the addresses

CardInfo cards[5];// array of card into structs
int card_num = 0;//the number of cards that have been created
int write_address = 100;

void save_metadata_to_eeprom() {
    EEPROM_SEND(0, card_num,0);
    for (int i = 0; i < card_num; i++) {
        EEPROM_SEND(1 + i * 3, cards[i].nickname_addr >> 8, cards[i].nickname_addr & 0xFF);
        EEPROM_SEND(1 + i * 3 + 1, cards[i].track1_addr >> 8, cards[i].track1_addr & 0xFF);
        EEPROM_SEND(1 + i * 3 + 2, cards[i].track2_addr >> 8, cards[i].track2_addr & 0xFF);
    }
}

void load_metadata_from_eeprom() {
    EEPROM_READ_FUN(0);
    card_num = EE_Data[0];
    for (int i = 0; i < card_num; i++) {
        EEPROM_READ_FUN(1 + i * 3);
        cards[i].nickname_addr = (EE_Data[0] << 8) | EE_Data[1];

        EEPROM_READ_FUN(1 + i * 3 + 1);
        cards[i].track1_addr = (EE_Data[0] << 8) | EE_Data[1];

        EEPROM_READ_FUN(1 + i * 3 + 2);
        cards[i].track2_addr = (EE_Data[0] << 8) | EE_Data[1];
    }
}


int main(void){
//    //parallel arrays for 5 cards to access card one, all the data is the zeroth index and etc
//    int card_nn[5];
//    int card_t1[5];
//    int card_t2[5];

    char decision;
    char nickname[MAX_FIELD_LEN];
    char track1[MAX_FIELD_LEN];
    char track2[MAX_FIELD_LEN];
    int LCD_count=0;
    int standalone = 0;
	char buffer[MAX_FIELD_LEN];
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART2_UART_Init();
    MX_TIM5_Init();
    LcdInit();
	LcdPutS("card options");
    HAL_TIM_Base_Start(&htim5);
    HAL_Delay(100);
    UART_send(&huart2, ClearScreen);
    UART_send(&huart2, CursorHome);
    load_metadata_from_eeprom();

	while (1){
		if(standalone==0){
            HAL_UART_Transmit(&huart2, (uint8_t *)"Add new card? (y/n): ", 23, HAL_MAX_DELAY);
			HAL_UART_Receive(&huart2, (uint8_t *)&decision, 1, HAL_MAX_DELAY);
			HAL_UART_Transmit(&huart2, (uint8_t *)&decision, 1, HAL_MAX_DELAY);
			HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);

			if ((decision == 'y' || decision == 'Y')) {
				//ask for track input
                HAL_UART_Transmit(&huart2, (uint8_t *)"Enter nickname: ", 17, HAL_MAX_DELAY);
				readLine(nickname, MAX_FIELD_LEN);
                HAL_UART_Transmit(&huart2, (uint8_t *)"Enter Track1 data: ", 20, HAL_MAX_DELAY);
				readLine(track1, MAX_FIELD_LEN);
                HAL_UART_Transmit(&huart2, (uint8_t *)"Enter Track2 data: ", 20, HAL_MAX_DELAY);
				readLine(track2, MAX_FIELD_LEN);

				//save each card, save the write address and then incrument the write address
				EEPROM_SEND_STRING(nickname,write_address);
//				card_nn[count]=write_address;
//				write_address+= (strlen(nickname)+1)/2;
				cards[card_num].nickname_addr = write_address;
				write_address += (strlen(nickname) + 1 + 1) / 2;
				EEPROM_SEND_STRING(track1,write_address);
                cards[card_num].track1_addr = write_address;
                write_address += (strlen(track1) + 1 + 1) / 2;
				EEPROM_SEND_STRING(track2,write_address);
                cards[card_num].track2_addr = write_address;
                write_address += (strlen(track2) + 1 + 1) / 2;

                card_num++;
				save_metadata_to_eeprom();
				HAL_UART_Transmit(&huart2, (uint8_t *)"Card saved.\r\n", 13, HAL_MAX_DELAY);
			}else {// if (decision != 'n' && decision != 'N')
                HAL_UART_Transmit(&huart2, (uint8_t *)"Thanks for entering your cards\r\n", 32, HAL_MAX_DELAY);
				standalone=1;
			}
		}
		if (Nucleo_button_pushed_verbose()&&standalone==1) {
			if (card_num == 0) continue;
			LcdClear();
			LcdGoto(0,0);
			LcdPutS("card options");
			//EEPROM_READ_STRING(cards[LCD_count].nickname_addr, buffer, MAX_FIELD_LEN);
			int i = 0;
			int addr = cards[LCD_count].nickname_addr;


			while (i < MAX_FIELD_LEN - 1) {
			    EEPROM_READ_FUN(addr++);
			    buffer[i++] = EE_Data[0];
			    if (EE_Data[0] == '\0') break;

			    buffer[i++] = EE_Data[1];
			    if (EE_Data[1] == '\0') break;
			}
			buffer[i] = '\0';
			LcdGoto(1,0);
			LcdPutS(buffer);
			if(card_num!=0){
				LCD_count = (LCD_count + 1) % card_num;
			}else{
				LcdPutS("INVALID NO CARDS");
			}

			HAL_Delay(200);
		}
	}
}

bool Nucleo_button_pressed(){
	return 	(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13) != PRESSED);
}
bool Nucleo_button_pushed_verbose(){
	static button_state_t state = PRESSED;
	bool take_action = false;

	switch(state){
		case PUSHED:
			if(!Nucleo_button_pressed()){
				state=P2R;
				take_action=false;
			}else{
				state=PUSHED;
				take_action=false;
			}
			break;

		case P2R:
			if(!Nucleo_button_pressed()){
				state=RELEASED;
				take_action=false;
			}else{
				state=PUSHED;
				take_action=false;
			}
			break;


		case RELEASED:
			if(Nucleo_button_pressed()){
				state=R2P;
				take_action=false;
			}else{
				state=RELEASED;
				take_action=false;
				sButtonStatus&=~ButtonHeld;
			}
			break;

		case R2P:
			if(Nucleo_button_pressed()){
				state=PUSHED;
				take_action=true;
			}else{
				state=RELEASED;
				take_action=false;
			}
			break;

	}
	return take_action;

}

//------------------------eeprom reading functions------------------
////woulkd not fit because sues chunks not while loop with break condition as null term
//void EEPROM_READ_STRING(char *buffer, uint8_t eeprom_chunks)
//{
//    int i = 0;
//    for (uint8_t addr = 0; addr < eeprom_chunks; addr++) {
//        EEPROM_READ_FUN(addr); // fills EE_Data[0] and EE_Data[1]
//        buffer[i++] = EE_Data[0];
//        buffer[i++] = EE_Data[1];
//    }
//    buffer[i] = '\0'; // Null-terminate the string
//}

//given function for reading 2 chars form eeprom
void EEPROM_READ_FUN(uint8_t EE_Addr)
{
    uint8_t buf[2];

    HAL_GPIO_WritePin(GPIOB, CS, GPIO_PIN_SET);         // Chip Select Enable
    buf[0] = EEPROM_START;
    HAL_SPI_Transmit(&hspi1, buf, 2, 100);

    HAL_SPI_Receive(&hspi1, EE_Data, 3, 100);
    HAL_GPIO_WritePin(GPIOB, CS, GPIO_PIN_RESET);           // Chip Select disable
    Delay_1_plus_us();

    EE_Data[0] <<= 1;
    if (EE_Data[1] & 0x80)
    	EE_Data[0] |= 0x01;

    EE_Data[1] <<= 1;
        if (EE_Data[2] & 0x80)
        EE_Data[1] |= 0x01;
}
//-----------------------------eeprom sending functions
//sends a string by reprtitively calling send and sending 2 chars at a time, all at adjacent adresses
void EEPROM_SEND_STRING(char *data, int address) {
    int len = strlen(data);
    for (int i = 0; i < len; i += 2) {
        char d1 = data[i];
        char d2 = (i+1 < len) ? data[i+1] : 0; // zero-pad if odd length
        int chunk = i / 2;                     // which 2-byte chunk this is
        EEPROM_SEND(address + chunk, d1, d2);
    }
}
//given send to send 2 chars
void EEPROM_SEND(char EE_Addr, char EE_Data1, char EE_Data2)
{
    uint8_t buf[4];

    HAL_GPIO_WritePin(GPIOB, CS, GPIO_PIN_SET);         // Chip Select Enable
    buf[0] = EEPROM_START;
    buf[1] = EEPROM_EWEN; //EEPROM_EWEN;
    HAL_SPI_Transmit(&hspi1, buf, 2, 100);
    HAL_GPIO_WritePin(GPIOB, CS, GPIO_PIN_RESET);       // Chip Select disable

    Delay_1_plus_us();

    HAL_GPIO_WritePin(GPIOB, CS, GPIO_PIN_SET);         // Chip Select Enable
    buf[0] = EEPROM_START;
    buf[1] = (0x40 | (EE_Addr & 0x03F));        // WRITE command OR with 6-bit address
    buf[2] = EE_Data1;
    buf[3] = EE_Data2;
    HAL_SPI_Transmit(&hspi1, buf, 4, 100);
    HAL_GPIO_WritePin(GPIOB, CS, GPIO_PIN_RESET);       // Chip Select disable

    HAL_Delay(20);

    HAL_GPIO_WritePin(GPIOB, CS, GPIO_PIN_SET);         // Chip Select Enable
    buf[0] = EEPROM_START;
    buf[1] = EEPROM_EWDS;
    HAL_SPI_Transmit(&hspi1, buf, 2, 100);
    HAL_GPIO_WritePin(GPIOB, CS, GPIO_PIN_RESET);           // Chip Select disable

    Delay_1_plus_us();

}
//given
void Delay_1_plus_us()
{
    uint32_t time5now = htim5.Instance->CNT;
    while (htim5.Instance->CNT == time5now)
        ; ;
    time5now = htim5.Instance->CNT;

    while (htim5.Instance->CNT == time5now)
        ; ;
}
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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 79;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 4294967295;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV2;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */

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
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB3 PB4 PB5
                           PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
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
