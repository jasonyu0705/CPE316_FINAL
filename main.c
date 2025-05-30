
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"
#include "Timer.h"
#include "LCD.h"

typedef long unsigned int uint16_t;
typedef unsigned int uint8_t;

#define NICKNAME_LEN  16
#define TRACK1_LEN    128
#define TRACK2_LEN    128
#define MAX_FIELD_LEN 64

#define NUM_CARDS_ADDR  31

// #define CS GPIO_PIN_6
// #define SK GPIO_PIN_5
// #define EEPROM_START 0x01
// #define EEPROM_EWEN 0x3F
// #define EEPROM_WRITE 0x40
// #define EEPROM_EWDS 0x0F
// #define EEPROM_READ 0x80
// #define PRESSED 0

#define NICKNAME_SIZE 16
#define TRACK1_SIZE 128 
#define TRACK2_SIZE 128

#define ButtonHeld 0x0001
#define LCD_Reset 0x0001
#define LCD_Start 0x0002

#define EEPROM_I2C_ADDR     0xA0  // 0x50 << 1
#define EEPROM_PAGE_SIZE    64
#define EEPROM_WRITE_DELAY  5     // in ms

#define MAX_NUM_CARDS       10
#define MENU_WAIT           2000  // in ms

unsigned short sButtonStatus;
unsigned short sLCDflags;

SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

int num_cards;
int recent_press = 0;

char nicknames[MAX_NUM_CARDS][NICKNAME_SIZE];

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

void EEPROM_WriteByte(uint16_t addr, uint8_t data);
void EEPROM_WriteBlock(uint16_t addr, uint8_t *data, uint16_t len);
void EEPROM_ReadByte(uint16_t addr, uint8_t *data);
void EEPROM_ReadBlock(uint16_t addr, uint8_t *data, uint16_t len);
// //void Delay_us(uint32_t us);
// void Delay_1_plus_us(void);
// void EEPROM_READ_FUN(uint8_t EE_Addr);
// void EEPROM_READ_STRING(char *buffer, uint8_t eeprom_chunks);
// void EEPROM_SEND_STRING(char *data, int address);
// void EEPROM_SEND(char EE_Addr, char EE_Data1, char EE_Data2);


void UART_send(char buffer[])
{
    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
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





// Data structure info:
/*
** Addr 0 **
31 bytes - optional password location
uint_8t  - # of saved cards
----------------------------
** Start of Card Data (addr 31 0x1F) **
16 bytes - nickname
128 bytes - track 1
128 bytes - track 2

*/
typedef struct CardData{
  char nickname[16];
  char track1[128];
  char track2[128];
};

typedef enum {
  Welcome,
  AddCard,
  ViewNicknames,
  ViewCard,
  EditNickname,
  EditTrack1,
  EditTrack2
} states;

states state = Welcome;
uint16_t addrs[3];

int main(void){
    int index=0;
    

    char choice;
    int card_choice;

    int LCD_count=0;
    int standalone = 0;
    uint16_t addrs[3];    
    
	char buffer[MAX_FIELD_LEN];
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART2_UART_Init();
    MX_TIM5_Init();
    LcdInit();
	  LcdPutS("Card Emulator");
    HAL_TIM_Base_Start(&htim5);
    HAL_Delay(100);
    UART_send(&huart2, ClearScreen);
    UART_send(&huart2, CursorHome);

    // Read the num of cards already stored on the device
    EEPROM_ReadByte(NUM_CARDS_ADDR, &num_cards);

    // Read all of the nicknames of the cards that are present and add them to the nickname array
    uint16_t temp_addr;
    for (int i = 0; i < num_cards; i++) {
      EEPROM_ReadBlock(NUM_CARDS_ADDR + 272*i, nicknames[i], 16);
    }

    uint16_t next_card_addr = NUM_CARDS_ADDR + 1 + num_cards*272;

	while (1){
    
    switch (state) {
        case Welcome: {
            UART_send("Welcome to the card emulator! Press any key to continue...\n");
            // Wait until we have had a recent interrupt, and then check for a key.
            // TODO

          }
        case AddCard: {
          struct CardData card;
          UART_send("Enter Nickname: ");
          readLine(card.nickname, NICKNAME_LEN);

          UART_send("Enter Track 1 data: ");
          readLine(card.track1, TRACK1_LEN);
          
          UART_send("Enter Track 2 data: ");
          readLine(card.track2, TRACK2_LEN);
          
          uint16_t nickname_write_addr = next_card_addr;
          uint16_t track1_write_addr = nickname_write_addr + 16;
          uint16_t track2_write_addr = track1_write_addr + 128;
          addrs[0]=nickname_write_addr;
          addrs[1]=track1_write_addr;
          addrs[2]=track2_write_addr;
          
          //writing all the card data to EEPROM
          EEPROM_WriteBlock(nickname_write_addr, card.nickname,  16);
          EEPROM_WriteBlock(track1_write_addr, card.track1, 128);
          EEPROM_WriteBlock(track2_write_addr, card.track2, 128);

          EEPROM_WriteByte(NUM_CARDS_ADDR, num_cards + 1);
          num_cards++;
          next_card_addr += 272;
          
          UART_send("Card saved.\r\n");
        }

        case ViewNicknames: {
          if (num_cards > 0) {
            UART_send("What would you like to do?\n\
                      Add card (A),\n\
                      Edit card (digit)\n\n");    
            //shows all the nicknames
            UART_send("Nicknames: ");
              for (int i = 0; i < num_cards; i++) {
                  UART_send("%d   ", i);
                  UART_send(nicknames[i]);
                  UART_send("\n");
              }
              readLine(&choice, 1);
              if (choice >= '0' && choice <= '9'){
                card_choice = choice - '0';
                state = ViewCard;
              } else {
                state = ViewNicknames;
              }
              break;
              
          } else {
            UART_send("Press 'A' to add a card.\n\n\
                       > ");

            readLine(&choice, 1);
            if (choice == 'A' || 'a') {
              state = AddCard;
              break;
            }
          }
            
        }
        
        case ViewCard: {    
            // card_choice is the int of the card we want to show info of
            char nickname_shown[NICKNAME_LEN];
            char track1_shown[TRACK1_LEN];
            char track2_shown[TRACK2_LEN];

            uint16_t block_addr = NUM_CARDS_ADDR + 272*card_choice;

            // Read card info from EEPROM
            EEPROM_ReadBlock(block_addr, nickname_shown, NICKNAME_LEN);
            EEPROM_ReadBlock(block_addr + NICKNAME_LEN, track1_shown, TRACK1_LEN);
            EEPROM_ReadBlock(block_addr + NICKNAME_LEN + TRACK1_LEN, track2_shown, TRACK2_LEN);

            UART_send("Press '0' to edit Nickname\n");
            UART_send("Press '1' to edit Track 1\n");
            UART_send("Press '2' to edit Track 2\n");
            UART_send("Press any other key to exit\n\n");

            UART_send("Nickname: ");
            UART_send(nickname_shown);
            UART_send("Track 1: ");
            UART_send(track1_shown);
            UART_send("Track 2: ");
            UART_send(track2_shown);

            readLine(&choice, 1);

            if (choice == '0') {
              state = EditNickname;
            } else if (choice == '1') {
              state = EditTrack1;
            } else if (choice == '2') {
              state = EditTrack2;
            } else {
              state = ViewNicknames;
            }

            break;
        }

        case EditNickname: {
          //ask user for new input and save that input through eeprom
          char new_nickname[NICKNAME_LEN];
          UART_send("Enter a new nickname: ");
          readLine(new_nickname, NICKNAME_LEN);
          EEPROM_WriteBlock(NUM_CARDS_ADDR + card_choice*272, new_nickname, NICKNAME_LEN);
          UART_send("Nickname saved.");
          HAL_Delay(MENU_WAIT);
          state = ViewNicknames;
          break;
        }

        case EditTrack1: {
          char new_track1[TRACK1_LEN];
          UART_send("Enter new track1 data: ");
          readLine(new_track1,TRACK1_LEN);
          EEPROM_WriteBlock(NUM_CARDS_ADDR + card_choice*272 + NICKNAME_LEN, new_track1, TRACK1_LEN);
          UART_send("Track 1 saved.");
          HAL_Delay(MENU_WAIT);
          state = ViewNicknames;
          break;
        }

        case EditTrack2: {
          char new_track2[TRACK2_LEN];
          UART_send("Enter a new Track 2: ");
          readLine(new_track2, TRACK2_LEN);
          EEPROM_WriteBlock(NUM_CARDS_ADDR + card_choice*272 + NICKNAME_LEN + TRACK1_LEN, new_track2, TRACK2_LEN);
          UART_send("Track 2 saved.");
          HAL_Delay(MENU_WAIT);
          state = ViewNicknames;
          break;
        }
        
        default:{
          char nickname[NICKNAME_LEN];
          char track1[TRACK1_LEN];
          char track2[TRACK2_LEN];
          int choice_counter=0;
          LcdClear();
          LcdGoto(0,0);
          LcdPutS("Card Options");
          LcdGoto(1,0);

          bool button_held = false;
          bool button_pressed = false;
          //rotate through the cards 
          if (sTimer[BUTTON_SCAN_TIMER] == 0) {
            button_pressed = Nucleo_button_pushed_verbose();
            if (button_held) {
              // Read tracks from memory, format everything, and send
              // TODO
            } else if (button_pressed) {
              // for (int i = 0; i < num_cards; i++) {
              EEPROM_ReadBlock((272*choice_counter) % MAX_NUM_CARDS, nickname, NICKNAME_LEN);
              EEPROM_ReadBlock((272*choice_counter) % MAX_NUM_CARDS+ NICKNAME_LEN, track1, TRACK1_LEN);
              EEPROM_ReadBlock((272*choice_counter) % MAX_NUM_CARDS + NICKNAME_LEN + TRACK1_LEN, track2, TRACK2_LEN);
              // }
              LcdPutS(nickname);
              choice_counter++;
            }

            sTimer[BUTTON_SCAN_TIMER] = BUTTON_SCAN_TIME;
            
          }
          HAL_Delay(200);
          break;
        }
        // Invalid input (do nothing)

    }
  }
}




bool Nucleo_button_pressed(){
	return 	(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) != PRESSED);
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




void EEPROM_WriteByte(uint16_t mem_addr, uint8_t data) {
    HAL_I2C_Mem_Write(&hi2c1, EEPROM_I2C_ADDR, mem_addr, I2C_MEMADD_SIZE_16BIT,&data, 1, HAL_MAX_DELAY);
    HAL_Delay(EEPROM_WRITE_DELAY);
}

void EEPROM_ReadByte(uint16_t mem_addr, uint8_t *data) {
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_I2C_ADDR, mem_addr, I2C_MEMADD_SIZE_16BIT,data, 1, HAL_MAX_DELAY);
}

void EEPROM_WriteBlock(uint16_t mem_addr, uint8_t *data, uint16_t len) {
    while (len > 0) {
        // Calculate how many bytes remain in current EEPROM page
        uint16_t bytes_left_in_page = EEPROM_PAGE_SIZE - (mem_addr % EEPROM_PAGE_SIZE);
        uint16_t chunk_size = (len < bytes_left_in_page) ? len : bytes_left_in_page;

        HAL_I2C_Mem_Write(&hi2c1, EEPROM_I2C_ADDR, mem_addr, I2C_MEMADD_SIZE_16BIT,data, chunk_size, HAL_MAX_DELAY);
        HAL_Delay(EEPROM_WRITE_DELAY);  // Wait for write cycle to complete

        mem_addr += chunk_size;
        data +=   chunk_size;
        len -= chunk_size;
    }
}

void EEPROM_ReadBlock(uint16_t mem_addr, uint8_t *data, uint16_t len) {
    HAL_I2C_Mem_Read(&hi2c1, EEPROM_I2C_ADDR, mem_addr, I2C_MEMADD_SIZE_16BIT, data, len, HAL_MAX_DELAY);
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
