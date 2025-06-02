#define _FIFO_C

#include "fifo.h"

void FifoInit(int index){
    Putpts[index] = Getpts[index] = &Fifos[index][0];
}

void FifoInitAll() {
    for (int i = 0; i < NUM_FIFOS; i++) {
        FifoInit(i);
    }
}

int GetFifo(int index, char *dataPt){
    if (Putpts[index] == Getpts[index])
    return 0; // Fifo empty
    else{
    *dataPt = *Getpts[index]++; // post increment after assignment
    if (Getpts[index] == &Fifos[index][FIFO_SIZE])
        Getpts[index] = Fifos[index];
        return -1; // successfully
    }
}

int FifoIsEmpty(int index) {
    return (Putpts[index] == Getpts[index]);
}

int PutFifo(int index, char data){
    char *Ppt; // Put pointer temporary use
    Ppt = Putpts[index]; // Save a copy of Putpt
    *Ppt++ = data; // Put into Fifo
    //HAL_UART_Transmit(&huart2, (uint8_t *)(Ppt-1), 1, HAL_MAX_DELAY); // echo the rec'd char
    //HAL_UART_Transmit(&huart2, (uint8_t *)"\n\r", 2, HAL_MAX_DELAY);
    if (Ppt == &Fifos[index][FIFO_SIZE])
    Ppt = &Fifos[index][0]; // wrap to Fifo top
    if (Ppt == Getpts[index]){
    //HAL_UART_Transmit(&huart2, (uint8_t *)"Received but FIFO full!\n\r",25, HAL_MAX_DELAY);
    return 0;
    }else{
    Putpts[index] = Ppt;
    //HAL_UART_Transmit(&huart2, (uint8_t *)"Received OK.\n\r", 15, HAL_MAX_DELAY);
    return -1; // successfully
    }
}

