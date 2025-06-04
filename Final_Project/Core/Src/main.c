#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "main.h"
#include "Timer.h"
#include "LCD.h"
#include "UART_MODIFIERS.h"
#include "fifo.h"
#include "coil.h"


#define NICKNAME_LEN  16
#define TRACK1_LEN    128
#define TRACK2_LEN    128
#define MAX_FIELD_LEN 64

#define CARD_DATA_SIZE (NICKNAME_LEN + TRACK1_LEN + TRACK2_LEN)

#define NUM_CARDS_ADDR  0x31
#define FIRST_BLOCK_ADDR (NUM_CARDS_ADDR + 1)

#define LCD_NICKNAME_LINE   "Nicknames:"

#define ButtonHeld 0x0001
#define LCD_Reset 0x0001
#define LCD_Start 0x0002

#define EEPROM_I2C_ADDR     0xA0  // 0x50 << 1
#define EEPROM_PAGE_SIZE    64
#define EEPROM_WRITE_DELAY  5     // in ms

#define MAX_NUM_CARDS       10
#define MENU_WAIT           2000  // in ms

#define UART_REC 0x0001

unsigned short sButtonStatus;
unsigned short sLCDflags;

I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

int num_cards;
int recent_press = 0;

int UART_flag = 0;

char nicknames[MAX_NUM_CARDS][NICKNAME_LEN];

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);

//--------BLUE button things
bool Nucleo_button_pressed();
bool Nucleo_button_pushed_verbose();
char ClearScreen[] = { 0x1B, '[', '2' , 'J',0 }; 	// Clear the screen
char CursorHome[] = { 0x1B, '[' , 'H' , 0 }; 	// Home the cursor

char UART_RX_byte;
int UART_received;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	TIMER2_HANDLE();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  UART_received = 1;
  HAL_UART_Receive_IT(&huart2, &UART_RX_byte, 1);
  
}

HAL_StatusTypeDef EEPROM_ReadBuffer(uint16_t memAddress,
                                    uint8_t  *pBuffer,
                                    uint16_t size);

HAL_StatusTypeDef EEPROM_WriteBuffer(uint16_t memAddress,
                                     uint8_t  *pBuffer,
                                     uint16_t size);

HAL_StatusTypeDef EEPROM_ReadByte(uint16_t memAddress,
                                  uint8_t  *pData);

HAL_StatusTypeDef EEPROM_WriteByte(uint16_t memAddress,
                                   uint8_t   data);

void delete_card(int card);

bool long_press = false;
bool button_pressed = false;

typedef enum{
	PUSHED,RELEASED,P2R,R2P
}button_state_t;


void UART_send(char buffer[])
{
    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
 }

// void readLine(char *buf) {
//     int idx = 0;
//     char ch;
//     // Clear buffer just in case
//     for (int i = 0; i < maxLen; i++) {
//     	buf[i] = '\0';
//     }

//     while (1) {

//         // if CR or LF, end input
//     	HAL_UART_Receive(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
// 		if (ch == '\r' || ch == '\n') break;

// 		// store and echo
// 		if (idx < maxLen - 1) {
// 			buf[idx++] = ch;
// 			HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
// 		}

//     }
//     buf[idx] = '\0';
//     // echo newline
//     const char crlf[] = "\r\n";
//     HAL_UART_Transmit(&huart2, (uint8_t *)crlf, 2, HAL_MAX_DELAY);
// }

int line_active = 0;   // 0 is idle, 1 is active
int line_target = 0;
int line_idx = 0;
char* line_buf;
int* line_done;

void readLine(char *buf, int maxLen, int* lineDone) {
  line_active = 1;
  line_target = maxLen;
  line_idx = 0;
  line_buf = buf;
  line_done = lineDone;
}

void readChar(char *buf, int* lineDone) {
  line_active = 1;
  line_target = 1;
  line_idx = 0;
  line_buf = buf;
  line_done = lineDone;
}

int tryReadChar() {
  if (UART_received) {
    UART_received = 0;
    return 1;
  } else {
    return 0;
  }
}

// Sets line_active to 0 when line is fully read
void pollReadLine() {
  if (line_active == 0) return;
  if (!tryReadChar()) {
    // No new char, so main thread continues
    return;    
  }
  // UART was received (buffer has a new char)

  if (line_target == 1) {
    line_buf[0] = UART_RX_byte;
    HAL_UART_Transmit(&huart2, &UART_RX_byte, 1, HAL_MAX_DELAY);
    line_active = 0;
    *line_done = 1;
    return;
  }
  
  if (UART_RX_byte == '\r' || UART_RX_byte == '\n') {
    line_buf[line_idx] = '\0';
    // Echo newline
    UART_send("\r\n");
    line_active = 0;
    *line_done = 1;
    return;
  }

  if (line_idx < line_target - 1) {
    line_buf[line_idx] = UART_RX_byte;
    line_idx++;
    HAL_UART_Transmit(&huart2, &UART_RX_byte, 1, HAL_MAX_DELAY);
  }


}

// void readChar(char *buf) {
// 	char ch;
// 	HAL_UART_Receive_IT(&huart2, (uint8_t *)&ch, 1);
// 	while (1) {
// 		if (UART_flag && UART_REC) {
// 			*buf = ch;
// 			HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
// 			UART_flag &= ~UART_REC;
// 			break;
// 		}
// 	}
// }





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

typedef enum UART_states {
  Welcome,
  AddNickname,
  AddTrack1,
  AddTrack2,
  ViewNicknames,
  ViewCard,
  EditNickname,
  EditTrack1,
  EditTrack2,
  SureDelete,
  NoCards
} UART_states;


typedef enum LCD_states {
  ShowNickname,
  SendTrack1,
  SendTrack2,
  NoNicknames
} LCD_states;


UART_states UART_state = ViewNicknames;
LCD_states LCD_state = ShowNickname;
uint16_t addrs[3];

int main(void){
    int index=0;


    char choice;
    int card_choice;

    int LCD_count=0;
    int standalone = 0;
    uint16_t addrs[3];


    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART2_UART_Init();
    MX_TIM2_Init();

    HAL_UART_Receive_IT(&huart2, &UART_RX_byte, 1);


    //LcdInit();
	//LcdPutS("Card Emulator");

    HAL_Delay(100);
    UART_send(ClearScreen);
    UART_send(CursorHome);

    // char track1_ascii[] = "%7001112000009182^VABALAS/DOVYDAS^000000000000?";
    // //char track1_ascii[] = "%?";
    // char track2_ascii[] = ";7001112000009182=24127990000000000000?";

    HAL_TIM_Base_Start_IT(&htim2);

    FifoInitAll();

    // char helloworld[] = "%HELLO WORLD?";
    // char track2_world[] = ";0123456789?";

    // init_tracks(helloworld, track2_world);

    // UART_send("Attempting to read from EEPROM\n\r");

    // Read the num of cards already stored on the device

    char temp[40];

  EEPROM_ReadByte(NUM_CARDS_ADDR, &num_cards);

  // EEPROM_ReadByte(NUM_CARDS_ADDR, &num_cards);

	// sprintf(temp, "num_cards = %d\n\r\n\r\n\r", num_cards);

	// UART_send(temp);

	UART_state = ViewNicknames;

  int LCD_index = 0;

  // Read all of the nicknames of the cards that are present and add them to the nickname array
  uint16_t temp_addr;

  for (int i = 0; i < num_cards && i < MAX_NUM_CARDS; i++) {
    EEPROM_ReadBuffer(FIRST_BLOCK_ADDR + CARD_DATA_SIZE*i, nicknames[i], NICKNAME_LEN);
  }

  uint16_t next_card_addr = FIRST_BLOCK_ADDR + num_cards*CARD_DATA_SIZE;

  //LCD initialization and clearing
  LcdInit();
  LcdClear();

	while (1){

    pollReadLine();

    // line_active = 0 when idle, 1 when polling
  if (sTimer[MENU_WAIT_TIMER] == 0) {
    switch (UART_state) {
        case Welcome: {
            UART_send("Welcome to the card emulator! Press any key to continue...\n");
            // Wait until we have had a recent interrupt, and then check for a key.
            // TODO
            break;
          }
        case AddNickname: {
          static int AddNickname_read = 0;
          static char add_nickname[NICKNAME_LEN];
          if (!line_active && !AddNickname_read) {
            

            next_card_addr = FIRST_BLOCK_ADDR + num_cards*CARD_DATA_SIZE;

            UART_send(ClearScreen);
            UART_send(CursorHome);
            UART_send("Enter Nickname: ");
            readLine(add_nickname, NICKNAME_LEN, &AddNickname_read);
          }

          if (!line_active && AddNickname_read) {

            uint16_t nickname_write_addr = next_card_addr;

            //writing all the card data to EEPROM
            EEPROM_WriteBuffer(nickname_write_addr, add_nickname,  NICKNAME_LEN);
            sTimer[MENU_WAIT_TIMER]=MENU_WAIT_TIME;
            UART_state = AddTrack1;
            AddNickname_read = 0;
            break;
          }
          break;
        }
        case AddTrack1: {
          static int AddTrack1_read = 0;

          static char add_track1[TRACK1_LEN];
          if(!line_active && !AddTrack1_read){
            next_card_addr = FIRST_BLOCK_ADDR + num_cards*CARD_DATA_SIZE;
            

            UART_send(ClearScreen);
            UART_send(CursorHome);
            UART_send("Enter Track 1 data: ");
            readLine(add_track1, TRACK1_LEN, &AddTrack1_read);
          }
          if (!line_active && AddTrack1_read) {
            uint16_t track1_write_addr = next_card_addr + NICKNAME_LEN;
            EEPROM_WriteBuffer(track1_write_addr, add_track1, TRACK1_LEN);
            UART_state = AddTrack2;

            AddTrack1_read = 0;
              
          }

          break;

        }
        case AddTrack2: {
          static int AddTrack2_read = 0;

          static char add_track2[TRACK2_LEN];

          if (!line_active && !AddTrack2_read){
            next_card_addr = FIRST_BLOCK_ADDR + num_cards*CARD_DATA_SIZE;
            

            UART_send(ClearScreen);
            UART_send(CursorHome);
            UART_send("Enter Track 2 data: ");
            readLine(add_track2, TRACK2_LEN, &AddTrack2_read);
          }
          
          if(!line_active && AddTrack2_read){
            uint16_t track2_write_addr = next_card_addr + NICKNAME_LEN + TRACK1_LEN;
            EEPROM_WriteBuffer(track2_write_addr, add_track2, TRACK2_LEN);
            EEPROM_WriteByte(NUM_CARDS_ADDR, num_cards + 1);

            num_cards++;

            UART_send("Card saved.\r\n");
            HAL_Delay(MENU_WAIT);
            sTimer[MENU_WAIT_TIMER]=MENU_WAIT_TIME;
            UART_state = ViewNicknames;

            AddTrack2_read = 0;
          }
          break;
        }


        case ViewNicknames: {
          static int ViewNicknames_read = 0;
          
          if (!line_active && !ViewNicknames_read) {
            EEPROM_ReadByte(NUM_CARDS_ADDR, &num_cards);
            for (int i = 0; i < num_cards && i < MAX_NUM_CARDS; i++) {
                EEPROM_ReadBuffer(FIRST_BLOCK_ADDR + CARD_DATA_SIZE*i, nicknames[i], NICKNAME_LEN);
            }

            UART_send(ClearScreen);
            UART_send(CursorHome);

            if (num_cards > 0) {
              UART_send("What would you like to do?\n\r");
              UART_send("Add card (A),\n\r");
              UART_send("Edit card (digit)\n\r\n\r");
              //shows all the nicknames
              UART_send(BOLD);
              UART_send(GREEN);
              UART_send("Nicknames:\n\r\n\r");
              UART_send(RESET);
              for (int i = 0; i < num_cards && i < MAX_NUM_CARDS; i++) {
                char temp[10];
                sprintf(temp, "%d   ", i);
                UART_send(temp);
                UART_send(nicknames[i]);
                UART_send("\n\r");
            }

              UART_send("\n\r> ");
              readChar(&choice, &ViewNicknames_read);

          } else {
            UART_state = NoCards;
            break;
          }
        }

          if (!line_active && ViewNicknames_read) {
            if (choice >= '0' && choice <= '9'){
              card_choice = choice - '0';
              
              if (card_choice < num_cards) {
                UART_state = ViewCard;
              } else {
                UART_state = ViewNicknames;
              }
              
            } else if (choice == 'A' || choice == 'a') {
              UART_state = AddNickname;
            } else if (choice == '`') {
              EEPROM_WriteByte(NUM_CARDS_ADDR, 0);
              UART_send(RED);
              UART_send("ADMIN FUNCTION\n\r\n\r");
              UART_send("**NUM_CARDS RESET**\r\n");
              UART_send(RESET);
              sTimer[MENU_WAIT_TIMER] = MENU_WAIT_TIME;
            } else {
              UART_state = ViewNicknames;
            }
            
            ViewNicknames_read = 0;
          }
            
          break;
        }
        
        case NoCards: {
          static int NoCards_read=0;
          
          if(!line_active && !NoCards_read){
            UART_send("Press 'A' to add a card.\n\r\n\r");
            UART_send("> ");

            readChar(&choice, &NoCards_read);
          }
          
          if(!line_active && NoCards_read){
            if (choice == 'A' || choice =='a') {
              UART_state = AddNickname;
              break;
            } else {
              UART_state = NoCards;
              break;
            }

            NoCards_read = 0;
          }
          break;
        }

        case ViewCard: {
          static int ViewCard_read=0;
          if(!line_active && !ViewCard_read){
            // card_choice is the int of the card we want to show info of
            char nickname_shown[NICKNAME_LEN];
            char track1_shown[TRACK1_LEN];
            char track2_shown[TRACK2_LEN];

            uint16_t block_addr = FIRST_BLOCK_ADDR + CARD_DATA_SIZE*card_choice;

            UART_send(ClearScreen);
            UART_send(CursorHome);

            // Read card info from EEPROM
            EEPROM_ReadBuffer(block_addr, nickname_shown, NICKNAME_LEN);
            EEPROM_ReadBuffer(block_addr + NICKNAME_LEN, track1_shown, TRACK1_LEN);
            EEPROM_ReadBuffer(block_addr + NICKNAME_LEN + TRACK1_LEN, track2_shown, TRACK2_LEN);

            UART_send("Press '0' to edit Nickname\n\r");
            UART_send("Press '1' to edit Track 1\n\r");
            UART_send("Press '2' to edit Track 2\n\r");
            UART_send("Press 'D' to delete card.\n\r");
            UART_send("Press any other key to exit\n\r\n\r");

            UART_send("Nickname: ");
            UART_send(nickname_shown);
            UART_send("\n\r");
            UART_send("Track 1: ");
            UART_send(track1_shown);
            UART_send("\n\r");
            UART_send("Track 2: ");
            UART_send(track2_shown);
            UART_send("\n\r");
            UART_send("\n\r> ");

            readChar(&choice, &ViewCard_read);

          }
          
          if(!line_active && ViewCard_read){
            if (choice == '0') {
            UART_state = EditNickname;
            } else if (choice == '1') {
              UART_state = EditTrack1;
            } else if (choice == '2') {
              UART_state = EditTrack2;
            } else if (choice == 'D' || choice == 'd') {
              UART_state = SureDelete;
            } else {
              UART_state = ViewNicknames;
            }
            ViewCard_read = 0;
          }


          break;
        }

        case SureDelete: {
          static int SureDelete_read=0;
          static char delete_choice;
          
          if(!line_active && !SureDelete_read){
            UART_send(ClearScreen);
            UART_send(CursorHome);

            UART_send("Are you sure you want to delete this card?\n\r");
            UART_send("Y/N: ");

            readChar(&delete_choice, &SureDelete_read);
          }

          if(!line_active && SureDelete_read){
            if (delete_choice == 'y' || delete_choice == 'Y') {
              UART_send("\r\n\r\nDeleting Card ");
              delete_card(card_choice);
              UART_send("\r\nCard deleted.");
              sTimer[MENU_WAIT_TIMER] = MENU_WAIT_TIME;
              
              SureDelete_read = 0;
              UART_state = ViewNicknames;
            }
          }
        	break;

        }

        case EditNickname: {
          static int EditNickname_read=0;
          static char new_nickname[NICKNAME_LEN];

          if(!line_active && !EditNickname_read){
            UART_send(ClearScreen);
            UART_send(CursorHome);
            //ask user for new input and save that input through eeprom
            
            UART_send("Enter a new nickname: ");
            readLine(new_nickname, NICKNAME_LEN, &EditNickname_read);
          }

          if(!line_active && EditNickname_read){
            EEPROM_WriteBuffer(FIRST_BLOCK_ADDR + card_choice*CARD_DATA_SIZE, new_nickname, NICKNAME_LEN);
            UART_send("Nickname saved.");
            sTimer[MENU_WAIT_TIMER] = MENU_WAIT_TIME;
            UART_state = ViewNicknames;
            EditNickname_read = 0;
          }

          break;
        }

        case EditTrack1: {
          static int EditTrack1_read=0;
          static char new_track1[TRACK1_LEN];

          if(!line_active && !EditTrack1_read){
            UART_send(ClearScreen);
            UART_send(CursorHome);
          
            UART_send("Enter new track1 data: ");
            readLine(new_track1,TRACK1_LEN,&EditTrack1_read);
          }
          
          if(!line_active && EditTrack1_read){
          EEPROM_WriteBuffer(FIRST_BLOCK_ADDR + card_choice*CARD_DATA_SIZE + NICKNAME_LEN, new_track1, TRACK1_LEN);
          UART_send("Track 1 saved.");
          sTimer[MENU_WAIT_TIMER] = MENU_WAIT_TIME;
          UART_state = ViewNicknames;
            
          EditTrack1_read = 0;
          }
          

          break;
        }

        case EditTrack2: {
          static EditTrack2_read = 0;
          static char new_track2[TRACK2_LEN];

          if (!line_active && !EditTrack2_read) {
            UART_send(ClearScreen);
            UART_send(CursorHome);
          
            UART_send("Enter a new Track 2: ");
            readLine(new_track2, TRACK2_LEN,&EditTrack2_read);
          }

          if (!line_active && EditTrack2_read) {
            EEPROM_WriteBuffer(FIRST_BLOCK_ADDR + card_choice*CARD_DATA_SIZE + NICKNAME_LEN + TRACK1_LEN, new_track2, TRACK2_LEN);
            UART_send("Track 2 saved.");
            sTimer[MENU_WAIT_TIMER] = MENU_WAIT_TIME;
            UART_state = ViewNicknames;
            EditTrack2_read = 0;
          }
        
          break;
        }

        // case Standalone: {
        // 	button_pressed = Nucleo_button_pushed_verbose();
        // 	if (sTimer[BUTTON_HOLD_TIMER] == 0 && long_press) {
        // 		// Button held
        // 	} else if (button_pressed) {
        // 		// Button single pressed
        // 	}
        // }

//        case SendTrackOne: {
//        	init_track(0, track1_str);
//        	transmit_track_one();
//        	// LCD funcs to go along with each step
//        }
//
//        case SendTrackTwo: {
//        	init_track(1, track2_str);
//        	transmit_track_two();
//        	// LCD funcs to go along with each step
//        }

        default:{
//          char nickname[NICKNAME_LEN];
//          char track1[TRACK1_LEN];
//          char track2[TRACK2_LEN];
//          int choice_counter=0;
//          LcdClear();
//          LcdGoto(0,0);
//          LcdPutS("Card Options");
//          LcdGoto(1,0);
//
//          bool button_held = false;
//          bool button_pressed = false;
//          //rotate through the cards
//          if (sTimer[BUTTON_SCAN_TIMER] == 0) {
//            button_pressed = Nucleo_button_pushed_verbose();
//            if (button_held) {
//              // Read tracks from memory, format everything, and send
//              // TODO
//            } else if (button_pressed) {
//              // for (int i = 0; i < num_cards; i++) {
//              EEPROM_ReadBlock((CARD_DATA_SIZE*choice_counter) % MAX_NUM_CARDS, nickname, NICKNAME_LEN);
//              EEPROM_ReadBlock((CARD_DATA_SIZE*choice_counter) % MAX_NUM_CARDS+ NICKNAME_LEN, track1, TRACK1_LEN);
//              EEPROM_ReadBlock((CARD_DATA_SIZE*choice_counter) % MAX_NUM_CARDS + NICKNAME_LEN + TRACK1_LEN, track2, TRACK2_LEN);
//              // }
//              LcdPutS(nickname);
//              choice_counter++;
//            }
//
//            sTimer[BUTTON_SCAN_TIMER] = BUTTON_SCAN_TIME;
//
//          }
//          HAL_Delay(200);
          UART_send("Entered Default state...\n\r");
          break;
        }
        // Invalid input (do nothing)

    }
  }

  if (num_cards == 0) {
    LCD_state = NoNicknames;
    LCD_index = 0;
  }

  // NOTE:
  // MAKE SURE TO INIT AND CLEAR LCD BEFORE WHILE LOOP
  switch (LCD_state) {
    case NoNicknames: {
      LcdGoto(0, 0);
      LcdPutS("No Cards Added");
      LcdGoto(1, 0);
      LcdPutS("Add via UART");

      if (num_cards != 0) {
        LCD_state = ShowNickname;
      }
    }

    case ShowNickname: {
      if (Nucleo_button_pushed_verbose()){
        LcdGoto(0, 0);
        LcdPutS(LCD_NICKNAME_LINE);

        LcdGoto(1, 0);
        LcdPutS(nicknames[LCD_index]);

        // Check for button press type (activate on release);
        if (Nucleo_button_pushed_verbose()) {
          if (sTimer[BUTTON_HOLD_TIMER] > BUTTON_HOLD1_TIME) {
            // Short press
            if (LCD_index == num_cards) {
              LCD_index = 0;
            } else {
              LCD_index++;
            }
            LCD_state = ShowNickname;

          } else if (sTimer[BUTTON_HOLD_TIMER] < BUTTON_HOLD1_TIME && sTimer[BUTTON_HOLD_TIMER] > 0)  {
            // Long press 1
            LCD_state = SendTrack1;
          } else {
            // Long press 2
            LCD_state = SendTrack2;
          }
        }
      }
      break;
    }

    case SendTrack1: {
      uint16_t LCD_block_addr = FIRST_BLOCK_ADDR + CARD_DATA_SIZE*LCD_index;
      char LCD_track1[TRACK1_LEN];
      
      EEPROM_ReadBuffer(LCD_block_addr + NICKNAME_LEN, LCD_track1, TRACK1_LEN);

      LcdGoto(0,0);
      LcdPutS("Sending Track 1");
      
      init_track(0, LCD_track1);
      transmit_track_one();

      HAL_Delay(500);
      LcdGoto(0,0);
      LcdPutS("Track 1 Sent.");
      HAL_Delay(MENU_WAIT);
      
      LCD_state = ShowNickname;
      break;
    }

    case SendTrack2:{
      char LCD_track2[TRACK2_LEN];
      uint16_t LCD_block_addr = FIRST_BLOCK_ADDR + CARD_DATA_SIZE*LCD_index;
      
      EEPROM_ReadBuffer(LCD_block_addr + NICKNAME_LEN + TRACK1_LEN, LCD_track2, TRACK2_LEN);

      LcdGoto(0,0);
      LcdPutS("Sending Track 2");

      init_track(1,LCD_track2);
      transmit_track_two();

      HAL_Delay(500);
      LcdGoto(0,0);
      LcdPutS("Track 2 Sent");
      HAL_Delay(MENU_WAIT);

      LCD_state = ShowNickname;
      break;
    }
  }
}
}

void delete_card(int card) {
  // Check if it's the last card
  // If yes, simply decrement num_cards and write to EEPROM
  int updated_num_cards = num_cards;


  if (card == updated_num_cards - 1) {
	  UART_send(".");
	  EEPROM_WriteByte(NUM_CARDS_ADDR, updated_num_cards - 1);

  } else {
	  // Move the rest of the cards down by 1

	  for (int i = card; i < updated_num_cards - 1; i++) {

		  uint16_t card_addr_to_replace = FIRST_BLOCK_ADDR + i*CARD_DATA_SIZE;
		  uint16_t card_next_addr = card_addr_to_replace + CARD_DATA_SIZE;

		  char temp_nickname[NICKNAME_LEN];
		  char temp_track1[TRACK1_LEN];
		  char temp_track2[TRACK2_LEN];

		  EEPROM_ReadBuffer(card_next_addr, temp_nickname, NICKNAME_LEN);
		  EEPROM_ReadBuffer(card_next_addr + NICKNAME_LEN, temp_track1, TRACK1_LEN);
		  EEPROM_ReadBuffer(card_next_addr + NICKNAME_LEN + TRACK1_LEN, temp_track2, TRACK2_LEN);

		  EEPROM_WriteBuffer(card_addr_to_replace, temp_nickname, NICKNAME_LEN);
		  EEPROM_WriteBuffer(card_addr_to_replace + NICKNAME_LEN, temp_track1, TRACK1_LEN);
		  EEPROM_WriteBuffer(card_addr_to_replace + NICKNAME_LEN + TRACK1_LEN, temp_track2, TRACK2_LEN);

		  UART_send(".");
	  }

	  EEPROM_WriteByte(NUM_CARDS_ADDR, updated_num_cards - 1);
  }


}



bool Nucleo_button_pressed(){
	return 	(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) != PUSHED);
}
bool Nucleo_button_pushed_verbose(){
	static button_state_t state = PUSHED;
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
				take_action=true;
				long_press = false;
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
				long_press = false;
			}
			break;

		case R2P:
			if(Nucleo_button_pressed()){
				state=PUSHED;
				take_action=false;
				sTimer[BUTTON_HOLD_TIMER] = BUTTON_HOLD2_TIME;
			}else{
				state=RELEASED;
				take_action=false;
			}
			break;

	}
	return take_action;

}

// EEPROM 7-bit I²C address when A0/A1/A2=GND → 0b1010 000 = 0x50
#define EEPROM_BASE_ADDR    (0x50 << 1)   // HAL wants the 8-bit address (0xA0)

// Maximum number of data bytes we can send in one page-write (64 bytes per page)
#define EEPROM_PAGE_SIZE    64

// Time (ms) to wait for the internal write cycle to finish (max 5 ms)
// We’ll use 5 ms to be safe.
#define EEPROM_WRITE_CYCLE_TIME  5

// ------------------------------------------------------------------------------------------------
// Write a single byte to EEPROM at 16-bit memory address 'memAddress'.
// ------------------------------------------------------------------------------------------------
HAL_StatusTypeDef EEPROM_WriteByte(uint16_t memAddress, uint8_t data)
{
    uint8_t tx[3];
    // 1) Build the 3-byte Tx buffer: [Addr_H][Addr_L][data]
    tx[0] = (uint8_t)(memAddress >> 8);        // high byte of memory address
    tx[1] = (uint8_t)(memAddress & 0xFF);      // low  byte of memory address
    tx[2] = data;

    // 2) Send 3 bytes in a single I2C transaction
    //    (HAL_I2C_Master_Transmit will send: START, slave_addr+W, tx[0..2], STOP)
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        &hi2c1,
        EEPROM_BASE_ADDR,
        tx,
        3,
        HAL_MAX_DELAY
    );
    if (status != HAL_OK) {
        return status;
    }

    // 3) Wait for the EEPROM’s internal write cycle to finish (≤5 ms)
    HAL_Delay(EEPROM_WRITE_CYCLE_TIME);

    return HAL_OK;
}

// ------------------------------------------------------------------------------------------------
// Read a single byte from EEPROM at 16-bit memory address 'memAddress'.
// ------------------------------------------------------------------------------------------------
HAL_StatusTypeDef EEPROM_ReadByte(uint16_t memAddress, uint8_t *pData)
{
    uint8_t addr[2];
    addr[0] = (uint8_t)(memAddress >> 8);
    addr[1] = (uint8_t)(memAddress & 0xFF);

    // 1) Send the 2-byte memory address (no STOP at end; this is a “restart” scenario)
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        &hi2c1,
        EEPROM_BASE_ADDR,
        addr,      // pointer to address bytes
        2,         // 2 bytes: high and low
        HAL_MAX_DELAY
    );
    if (status != HAL_OK) {
        return status;
    }

    // 2) Perform a repeated-start and read 1 byte from that address
    status = HAL_I2C_Master_Receive(
        &hi2c1,
        EEPROM_BASE_ADDR,
        pData,
        1,         // read exactly 1 byte
        HAL_MAX_DELAY
    );
    return status;
}

// ------------------------------------------------------------------------------------------------
// Write up to 'size' bytes from 'pBuffer' into EEPROM starting at memAddress.
// Splits the write into page-aligned chunks of ≤64 bytes per I²C transaction.
// ------------------------------------------------------------------------------------------------
HAL_StatusTypeDef EEPROM_WriteBuffer(uint16_t memAddress, uint8_t *pBuffer, uint16_t size)
{
    HAL_StatusTypeDef status;
    uint16_t bytesRemaining = size;
    uint16_t currentAddress = memAddress;
    uint8_t  *currentPtr = pBuffer;

    while (bytesRemaining > 0) {
        // 1) Calculate how many bytes to write in this page-write:
        //    We must not cross a 64-byte page boundary.
        uint16_t pageOffset = currentAddress % EEPROM_PAGE_SIZE;
        uint16_t spaceInPage = EEPROM_PAGE_SIZE - pageOffset;
        uint16_t chunkSize = (bytesRemaining < spaceInPage) ? bytesRemaining : spaceInPage;

        // 2) Build a local Tx buffer: [Addr_H][Addr_L][data0…dataN−1]
        //    chunkSize ≤ 64, so total ≤ 66 bytes.
        uint8_t tx[2 + EEPROM_PAGE_SIZE];
        tx[0] = (uint8_t)(currentAddress >> 8);
        tx[1] = (uint8_t)(currentAddress & 0xFF);
        memcpy(&tx[2], currentPtr, chunkSize);

        // 3) Transmit in one I2C call
        status = HAL_I2C_Master_Transmit(
            &hi2c1,
            EEPROM_BASE_ADDR,
            tx,
            2 + chunkSize,    // 2 bytes of address + chunkSize bytes of data
            HAL_MAX_DELAY
        );
        if (status != HAL_OK) {
            return status;
        }

        // 4) Wait for the internal write cycle to finish
        HAL_Delay(EEPROM_WRITE_CYCLE_TIME);

        // 5) Advance pointers/counters
        currentAddress += chunkSize;
        currentPtr     += chunkSize;
        bytesRemaining -= chunkSize;
    }

    return HAL_OK;
}

// ------------------------------------------------------------------------------------------------
// Read 'size' bytes from EEPROM starting at memAddress into pBuffer.
// ------------------------------------------------------------------------------------------------
HAL_StatusTypeDef EEPROM_ReadBuffer(uint16_t memAddress, uint8_t *pBuffer, uint16_t size)
{
    HAL_StatusTypeDef status;
    uint8_t addr[2];
    addr[0] = (uint8_t)(memAddress >> 8);
    addr[1] = (uint8_t)(memAddress & 0xFF);

    // 1) Transmit the 2-byte memory address (no STOP)
    status = HAL_I2C_Master_Transmit(
        &hi2c1,
        EEPROM_BASE_ADDR,
        addr,
        2,
        HAL_MAX_DELAY
    );
    if (status != HAL_OK) {
        return status;
    }

    // 2) Read 'size' bytes in a single I2C transaction
    status = HAL_I2C_Master_Receive(
        &hi2c1,
        EEPROM_BASE_ADDR,
        pBuffer,
        size,
        HAL_MAX_DELAY
    );
    return status;
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10D19CE4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  htim2.Init.Prescaler = 79;
  htim2.Init.CounterMode = TIM_COUNTERMODE_DOWN;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
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
