#define _COIL_C

#include "coil.h"

void reverse_string(char *input) {
    int len = strlen(input);
    for (int i = 0; i < len/2; i++) {
        char temp = input[i];
        input[i] = input[len - 1-i];
        input[len - 1-i] = temp;
    }
}

void toggle_coil_polarity() {
coil_polarity = !coil_polarity;
    if (coil_polarity) {
        HAL_GPIO_WritePin(S_PORT, SOLENOID_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(S_PORT, SOLENOID_2, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(S_PORT, SOLENOID_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(S_PORT, SOLENOID_2, GPIO_PIN_RESET);
    }
}

void set_polarity_low() {
    coil_polarity = 0;
    HAL_GPIO_WritePin(S_PORT, SOLENOID_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(S_PORT, SOLENOID_2, GPIO_PIN_RESET);
}

void send_one(int index) {
    toggle_coil_polarity();
    // schedule to toggle at half_way of timer
    if (index == 0) {
        sTimer[HALF_BLOCK] = HALF_BLOCK_TIME_TRACK_1;
        sTimer[FULL_BLOCK] = FULL_BLOCK_TIME_TRACK_1;
    } else {
        sTimer[HALF_BLOCK] = HALF_BLOCK_TIME_TRACK_2;
        sTimer[FULL_BLOCK] = FULL_BLOCK_TIME_TRACK_2;
    }
    while (sTimer[HALF_BLOCK] != 0);
    toggle_coil_polarity();
}

void send_zero(int index) {
    toggle_coil_polarity();
    if (index == 0){
        sTimer[FULL_BLOCK] = FULL_BLOCK_TIME_TRACK_1;
    } else {
        sTimer[FULL_BLOCK] = FULL_BLOCK_TIME_TRACK_2;
    }
}

void send_track(int index) {

    while (!FifoIsEmpty(index)) {

    if (sTimer[FULL_BLOCK] == 0) {
        GetFifo(index, Buffer);
        if (Buffer[0] == 0) {
        send_zero(index);
        } else {
        send_one(index);
        }
    }
    }
}

void send_zeros(int track, int count){
    if (track == 0) {
        for (int i = 0; i < count; i++) {
            while(sTimer[FULL_BLOCK] != 0);
            send_zero(0);

        }
    } else {
        for (int i = 0; i < count; i++) {
            while(sTimer[FULL_BLOCK] != 0);
            send_zero(1);

        }
    }
}

//iterating through each character and then for each character 7 bits (disregard the MSB)
void load_track_one(char* track, int length) {
    //char t1[length];
    for (int i = 0;  i < length;  i++) {
        for (int j = 0; j < 7; j++) {
            PutFifo(0, (track[i] >> j) & 1);
        }
    }
}

void load_track_two(char* track, int length) {
//char t2[length];
    for (int i = 0;  i < length;  i++) {
        for (int j = 0; j < 5; j++) {
        PutFifo(1, (track[i] >> j) & 1);
        }
    }
}

int convert_track_one(char* track, int length) {
    //char t1[length + 1];
    for (int k = 0;  k < length;  k++) {
        track[k]=track1_lut[track[k]];
    }
    return length;
}

int convert_track_two(char* track, int length) {
    //char t2[length + 1];

    for (int k = 0; k < length; k++) {
    // Convert to BYTE mapping of characters for magnetic tracks (from ASCII)
    track[k] = track2_lut[track[k]];
    }

    return length;
}

// --- Track 1 (7-bit codes: 6 data bits + 1 parity) ---
size_t appendTrack1LRC(uint8_t *codes, size_t length) {
    // 1) XOR-fold the 6 data-bit columns
    uint8_t P[6] = {0,0,0,0,0,0};
    for (size_t i = 0; i < length; i++) {
        uint8_t data = codes[i] & 0x3F;         // lower 6 bits = data
        for (int j = 0; j < 6; j++)
            P[j] ^= (data >> j) & 1;
    }
    // 2) Pick LRC data bits so each column has odd parity
    uint8_t lrc_data = 0;
    for (int j = 0; j < 6; j++)
        if ((P[j] ^ 1) & 1)
            lrc_data |= (1 << j);

    // 3) Compute LRC’s own parity bit (so its 7 bits are odd)
    int ones = __builtin_popcount(lrc_data);
    uint8_t p = (ones % 2 == 0) ? 1 : 0;

    // 4) Form the full 7-bit code and append
    uint8_t lrc_code = (p << 6) | lrc_data;
    codes[length] = lrc_code;
    return length + 1;
}

// --- Track 2 (5-bit codes: 4 data bits + 1 parity) ---
size_t appendTrack2LRC(uint8_t *codes, size_t length) {
    // 1) XOR-fold the 4 data-bit columns
    uint8_t P[4] = {0,0,0,0};
    for (size_t i = 0; i < length; i++) {
        uint8_t data = codes[i] & 0x0F;         // lower 4 bits = data
        for (int j = 0; j < 4; j++)
            P[j] ^= (data >> j) & 1;
    }
    // 2) Pick LRC data bits so each column has odd parity
    uint8_t lrc_data = 0;
    for (int j = 0; j < 4; j++)
        if ((P[j] ^ 1) & 1)
            lrc_data |= (1 << j);

    // 3) Compute LRC’s own parity bit (so its 5 bits are odd)
    int ones = __builtin_popcount(lrc_data);
    uint8_t p = (ones % 2 == 0) ? 1 : 0;

    // 4) Form the full 5-bit code and append
    uint8_t lrc_code = (p << 4) | lrc_data;
    codes[length] = lrc_code;
    return length + 1;
}

// Returns -1 if valid, else returns invalid char
char validate_track(char* track, int length, int track_num) {
    if (track_num == 0) {
        for (int i = 0; i < length; i++) {
            if (track1_lut[track[i]] == 0x00) {
                // invalid char
                return track[i];
            }
        }
    } else {
        for (int i = 0; i < length; i++) {
            if (track2_lut[track[i]] == 0x00) {
                // invalid char
                return track[i];
            }
        }
    }
    return -1;
}

// Removes whitespace (track2) and character returns. Returns new length
int clean_track(char* track, int length, int track_num) {
    int curr = 0;
    if (track_num == 0){
        for (int i = 0; i < length; i++) {
            if (track[i] == '\r' || track[i] == '\n' || track[i] == '\t') {
                // characters to be cleaned
            } else {
                track[curr] = track[i];
                curr++;
            }
        }
    } else {
        for (int i = 0; i < length; i++) {
            if (track[i] == ' ' || track[i] == '\r' || track[i] == '\n' || track[i] == '\t') {
                // characters to be cleaned
            } else {
                track[curr] = track[i];
                curr++;
            }
        }
    }
    return curr;
}

void send_both_tracks() {
    send_zeros(0, 25);
    send_track(0);
    send_zeros(0, 28);
    send_zeros(1, 25);
    send_track(1);
    send_zeros(1, 25);
}

void init_tracks(char* track1_str, char* track2_str) {
    set_track(1, track1_str);
    set_track(2, track2_str);
    
    track1_len = convert_track_one(track1, track1_len);
    track2_len = convert_track_one(track2, track2_len);

    track1_len = appendTrack1LRC(track1, track1_len);
    track2_len = appendTrack2LRC(track2, track2_len);
}

// Assigns tracks to variables used for transmission
void set_track(int track_num, char* track_str) {
    if (track_num == 1) {
        strcpy(track1, track_str);
        track1_len = strlen(track_str);
    } else if (track_num == 2) {
        strcpy(track2, track_str);
        track2_len = strlen(track_str);
    }
}

// Desired tracks must be initialized prior to calling this function
// int delay - delay in ms between sends
void transmit_both_tracks(int delay) {
        HAL_Delay(delay);
		load_track_one(track1, track1_len);
	    load_track_two(track2, track2_len);
		send_both_tracks();

		HAL_Delay(50);
		set_polarity_low();
}