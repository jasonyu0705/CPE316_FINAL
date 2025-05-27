/*
 * Timer.h
 *
 *  Created on: May 18, 2025
 *      Author: peterh17
 */

#ifndef INC_TIMER_H_
#define INC_TIMER_H_

#ifdef _TIMER_C
   #define SCOPE
#else
   #define SCOPE extern
#endif

#define NUMBER_OF_TIMERS			3

#define SEL_INPUT_TIMER				0
#define PASS_TRY_TIMER				1
#define NORMAL_RUN_TIMER			2

#define SEL_INPUT_TIME				150
#define PASS_TRY_TIME				1500
#define NORMAL_RUN_TIME				80

SCOPE unsigned short sTimer[NUMBER_OF_TIMERS];

SCOPE void TIMER2_HANDLE(void);

#undef SCOPE
#endif /* INC_TIMER_H_ */
