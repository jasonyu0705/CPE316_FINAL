#define _COIL_C

#include "coil.h"
#include "timer.h"

int coil_polarity;

const uint8_t track1_lut[128] = {
		[' '] = 0x40,
		['!'] = 0x01, ['"'] = 0x02, ['#'] = 0x43, ['$'] = 0x04,
		['%'] = 0x45, ['&'] = 0x46, ['\'']= 0x07, ['('] = 0x08,
		[')'] = 0x49, ['*'] = 0x4A, ['+'] = 0x0B, [','] = 0x4C,
		['-'] = 0x0D, ['.'] = 0x0E, ['/'] = 0x4F,
		['0'] = 0x10, ['1'] = 0x51, ['2'] = 0x52, ['3'] = 0x13,
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
		[']'] = 0x3D, ['^'] = 0x3E, ['_'] = 0x7F
		// everything else defaults to 0x00 (you can treat that as "invalid")
	};

const uint8_t track2_lut_rev[128] = {
    ['0'] = 0x01,  // orig 0x10 (10000b) → rev 00001b
    ['1'] = 0x10,  // orig 0x01 (00001b) → rev 10000b
    ['2'] = 0x08,  // orig 0x02 (00010b) → rev 01000b
    ['3'] = 0x19,  // orig 0x13 (10011b) → rev 11001b
    ['4'] = 0x04,  // orig 0x04 (00100b) → rev 00100b
    ['5'] = 0x15,  // orig 0x15 (10101b) → rev 10101b
    ['6'] = 0x0D,  // orig 0x16 (10110b) → rev 01101b
    ['7'] = 0x1C,  // orig 0x07 (00111b) → rev 11100b
    ['8'] = 0x02,  // orig 0x08 (01000b) → rev 00010b
    ['9'] = 0x13,  // orig 0x19 (11001b) → rev 10011b

    [';'] = 0x1A,  // orig 0x0B (01011b) → rev 11010b
    ['='] = 0x16,  // orig 0x0D (01101b) → rev 10110b
    ['?'] = 0x1F,  // orig 0x1F (11111b) → rev 11111b (palindrome)

    // everything else = 0x00 (invalid)
};

const uint8_t track2_lut[128] = {
    ['0'] = 0x10,  // 10000b
    ['1'] = 0x01,  // 00001b
    ['2'] = 0x02,  // 00010b
    ['3'] = 0x13,  // 10011b
    ['4'] = 0x04,  // 00100b
    ['5'] = 0x15,  // 10101b
    ['6'] = 0x16,  // 10110b
    ['7'] = 0x07,  // 00111b
    ['8'] = 0x08,  // 01000b
    ['9'] = 0x19,  // 11001b

    [';'] = 0x0B,  // 01011b (Start sentinel)
    ['='] = 0x0D,  // 01101b (Field separator)
    ['?'] = 0x1F,  // 11111b (End sentinel)

    // everything else = 0x00 (invalid or undefined)
};

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
        HAL_GPIO_WritePin(S_PORT, SOLENOID_2, GPIO_PIN_SET);
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

//void load_track_two_rev(char* track, int length) {
//// i = length−1 … 0 reverses the character order
//	for (int i = length - 1; i >= 0; i--) {
//		// But still send each 5-bit code LSB first:
//		for (int j = 0; j < 5; j++) {
//			PutFifo(1, (track[i] >> j) & 1);
//		}
//	}
//}

void load_track_two_rev(char* track, int length){
	for (int i = length - 1; i >= 0; i--) {
		for (int j = 0; j < 5; j++) {
			PutFifo(1, (track[i] >> (4 - j)) & 1);
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
//size_t appendTrack1LRC(uint8_t *codes, size_t length) {
//    // 1) XOR-fold the 6 data-bit columns
//    uint8_t P[6] = {0,0,0,0,0,0};
//    for (size_t i = 0; i < length; i++) {
//        uint8_t data = codes[i] & 0x3F;         // lower 6 bits = data
//        for (int j = 0; j < 6; j++)
//            P[j] ^= (data >> j) & 1;
//    }
//    // 2) Pick LRC data bits so each column has odd parity
//    uint8_t lrc_data = 0;
//    for (int j = 0; j < 6; j++)
//        if ((P[j] ^ 1) & 1)
//            lrc_data |= (1 << j);
//
//    // 3) Compute LRC’s own parity bit (so its 7 bits are odd)
//    int ones = __builtin_popcount(lrc_data);
//    uint8_t p = (ones % 2 == 0) ? 1 : 0;
//
//    // 4) Form the full 7-bit code and append
//    uint8_t lrc_code = (p << 6) | lrc_data;
//    codes[length] = lrc_code;
//    return length + 1;
//}
//
//// --- Track 2 (5-bit codes: 4 data bits + 1 parity) ---
//size_t appendTrack2LRC(uint8_t *codes, size_t length) {
//    // 1) XOR-fold the 4 data-bit columns
//    uint8_t P[4] = {0,0,0,0};
//    for (size_t i = 0; i < length; i++) {
//        uint8_t data = codes[i] & 0x0F;         // lower 4 bits = data
//        for (int j = 0; j < 4; j++)
//            P[j] ^= (data >> j) & 1;
//    }
//    // 2) Pick LRC data bits so each column has odd parity
//    uint8_t lrc_data = 0;
//    for (int j = 0; j < 4; j++)
//        if ((P[j] ^ 1) & 1)
//            lrc_data |= (1 << j);
//
//    // 3) Compute LRC’s own parity bit (so its 5 bits are odd)
//    int ones = __builtin_popcount(lrc_data);
//    uint8_t p = (ones % 2 == 0) ? 1 : 0;
//
//    // 4) Form the full 5-bit code and append
//    uint8_t lrc_code = (p << 4) | lrc_data;
//    codes[length] = lrc_code;
//    return length + 1;
//}

int appendTrack1LRC(char* codes, int length) {
    int cols[6] = {0, 0, 0, 0, 0, 0};

    // XOR each column
    for (int i = 0; i < length; i++) {
        for (int j = 0; j < 6; j++) {
            cols[j] ^= (codes[i] >> (5 - j)) & 1;
        }
    }

    // Count number of 1s in LRC_data
    // We want an odd number of ones after LRC parity
    // So if num_1s is even, LRC parity = 1, else 0
    int num_ones = 0;
    for (int i = 0; i < 6; i++) {
        if (cols[i] == 1) {
            num_ones++;
        }
    }
    int LRC_parity;
    if ((num_ones % 2) == 0) {
        // Even, so LRC parity is 1
        LRC_parity = 1;
    } else {
        LRC_parity = 0;
    }

    // Form actual LRC value
    unsigned char LRC_val = 0;

    // Insert parity bit
    LRC_val |= (LRC_parity << 6);

    // Add col vals
    for (int i = 0; i < 6; i++) {
        LRC_val |= (cols[i] << (5-i));
    }

    codes[length] = LRC_val;
    return length + 1;
}


int appendTrack2LRC(char* codes, int length) {
    int cols[4] = {0, 0, 0, 0};

    // XOR each column
    for (int i = 0; i < length; i++) {
        for (int j = 0; j < 4; j++) {
            cols[j] ^= (codes[i] >> (3 - j)) & 1;
        }
    }

    // Count number of 1s in LRC_data
    // We want an odd number of ones after LRC parity
    // So if num_1s is even, LRC parity = 1, else 0
    int num_ones = 0;
    for (int i = 0; i < 4; i++) {
        if (cols[i] == 1) {
            num_ones++;
        }
    }
    int LRC_parity;
    if ((num_ones % 2) == 0) {
        // Even, so LRC parity is 1
        LRC_parity = 1;
    } else {
        LRC_parity = 0;
    }

    // Form actual LRC value
    unsigned char LRC_val = 0;

    // Insert parity bit
    LRC_val |= (LRC_parity << 4);

    // Add col vals
    for (int i = 0; i < 4; i++) {
        LRC_val |= (cols[i] << (3-i));
    }

    codes[length] = LRC_val;
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
    send_zeros(0, 25);
    set_polarity_low();
    //delay_between_tracks(FULL_BLOCK_TIME_TRACK_1 * 25);
    HAL_Delay(1500);
    send_zeros(1, 25);
    send_track(1);
    send_zeros(1, 25);
}

// time in 0.1ms units
void delay_between_tracks(int time) {
	sTimer[BETWEEN_TRACK_TIMER] = time;
	while (sTimer[BETWEEN_TRACK_TIMER] != 0);
}

void init_tracks(char* track1_str, char* track2_str) {
    set_track(1, track1_str);
    set_track(2, track2_str);
    
    track1_len = convert_track_one(track1, track1_len);
    track2_len = convert_track_two(track2, track2_len);

    track1_len = appendTrack1LRC(track1, track1_len);
    track2_len = appendTrack2LRC(track2, track2_len);
}

void init_track(int index, char* track_str) {
	if (index == 0) {
		set_track(1, track_str);
		track1_len = convert_track_one(track1, track1_len);
		track1_len = appendTrack1LRC(track1, track1_len);
	} else {
		set_track(2, track_str);
		track2_len = convert_track_two(track2, track2_len);
		track2_len = appendTrack2LRC(track2, track2_len);
	}
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
		load_track_one(track1, track1_len);
	    //load_track_two(track2, track2_len);
		load_track_two_rev(track2, track2_len);
		send_both_tracks();

		HAL_Delay(50);
		set_polarity_low();

        HAL_Delay(delay);
}

void transmit_track_one() {
	load_track_one(track1, track1_len);
	send_zeros(0,25);
	send_track(0);
	send_zeros(0,25);
	set_polarity_low();
}

void transmit_track_two() {
	load_track_two_rev(track2, track2_len);
	send_zeros(1, 25);
	send_track(1);
	send_zeros(1, 25);
	set_polarity_low();
}


