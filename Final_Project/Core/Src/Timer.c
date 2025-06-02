#define _TIMER_C

#include "TIMER.h"

void TIMER2_HANDLE(void)
{
	unsigned short sIndex;
	//__disable_irq();

	for (sIndex=0; sIndex<NUMBER_OF_TIMERS; sIndex++)
	{
		if (sTimer[sIndex] != 0)
			sTimer[sIndex]--;
	}
	//__enable_irq();
}
