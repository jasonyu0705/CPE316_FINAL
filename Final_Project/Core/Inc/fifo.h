#ifndef INC_FIFO_H_
#define INC_FIFO_H_

#ifdef _FIFO_C
    #define SCOPE
#else
    #define SCOPE extern
#endif


#define FIFO_SIZE 	600 			// size of longest track
#define NUM_FIFOS 	2	

//for FIFO use
SCOPE unsigned short flags;

SCOPE char Buffer[1];

SCOPE volatile char* Putpts[NUM_FIFOS];
SCOPE volatile char* Getpts[NUM_FIFOS];

SCOPE volatile char Fifos[NUM_FIFOS][FIFO_SIZE];

SCOPE void FifoInit(int index);
SCOPE void FifoInitAll();
SCOPE int GetFifo(int index, char *dataPt);
SCOPE int FifoIsEmpty(int index);
SCOPE int PutFifo(int index, char data);

#undef SCOPE
#endif