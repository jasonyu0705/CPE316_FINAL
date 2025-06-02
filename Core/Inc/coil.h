#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "fifo.h"

#ifndef INC_COIL_H_
#define INC_COIL_H_

#ifdef _COIL_C
    #define SCOPE
#else
    #define SCOPE extern
#endif

#define SOLENOID_1	GPIO_PIN_0		// Pin A0
#define	SOLENOID_2  GPIO_PIN_1
#define S_PORT      GPIOA

SCOPE int coil_polarity = 0;

SCOPE volatile char track1[FIFO_SIZE];
SCOPE volatile char track2[FIFO_SIZE];

SCOPE volatile int track1_len;
SCOPE volatile int track2_len;

//%B1234567890123456^DOE/JOHN           ^25051234567890000000?  <-- track 1 example
//;1234567890123456=25051234567890000000? <-- track 2 example

// only ASCII 0x20–0x7F are used on Track 1; adjust size if you need more
SCOPE const uint8_t track1_lut[128] = {
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

// Track-2 reversed‐bit LUT: ASCII → bit-reversed 5-bit code
SCOPE const uint8_t track2_lut_rev[128] = {
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

// Track-2 original LUT: ASCII → 5-bit code (not bit-reversed)
static const uint8_t track2_lut[128] = {
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

SCOPE void reverse_string(char *input);
SCOPE void toggle_coil_polarity();
SCOPE void set_polarity_low();
SCOPE void send_one(int index);
SCOPE void send_zero(int index);
SCOPE void send_track(int index);
SCOPE void send_zeros(int track, int count);
SCOPE void load_track_one(char* track, int length);
SCOPE void load_track_two(char* track, int length);
SCOPE int convert_track_one(char* track, int length);
SCOPE int convert_track_two(char* track, int length);
SCOPE size_t appendTrack1LRC(uint8_t *codes, size_t length);
SCOPE size_t appendTrack2LRC(uint8_t *codes, size_t length);
SCOPE char validate_track(char* track, int length, int track_num);
SCOPE int clean_track(char* track, int length, int track_num);
SCOPE void send_both_tracks();
SCOPE void init_tracks(char* track1_str, char* track2_str);
SCOPE void set_track(int track_num, char* track_str);
SCOPE void transmit_both_tracks();

#undef SCOPE
#endif