#ifndef INC_TIMER_H_
#define INC_TIMER_H_

#ifdef _TIMER_C
   #define SCOPE
#else
   #define SCOPE extern
#endif

#define NUMBER_OF_TIMERS			6

#define FULL_BLOCK					0
#define HALF_BLOCK					1
#define BUTTON_SCAN_TIMER			2
#define BUTTON_HOLD_TIMER			3
#define BETWEEN_TRACK_TIMER		4
#define MENU_WAIT_TIMER          5

#define FULL_BLOCK_TIME_TRACK_1				16		// Time in 100us / 0.1ms
#define HALF_BLOCK_TIME_TRACK_1				FULL_BLOCK_TIME_TRACK_1 / 2

#define FULL_BLOCK_TIME_TRACK_2				44
#define HALF_BLOCK_TIME_TRACK_2				FULL_BLOCK_TIME_TRACK_2 / 2

#define BUTTON_SCAN_TIME					100
#define BUTTON_HOLD1_TIME					15000    // 1.5 sec
#define BUTTON_HOLD2_TIME              30000    // 3.0 sec
#define MENU_WAIT_TIME                 20000

SCOPE volatile unsigned short sTimer[NUMBER_OF_TIMERS];

SCOPE void TIMER2_HANDLE(void);

#undef SCOPE
#endif /* INC_TIMER_H_ */
