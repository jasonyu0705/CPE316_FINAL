#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "fifo.h"
#include "timer.h"
#include "main.h"

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

SCOPE int coil_polarity;

SCOPE char track1[FIFO_SIZE];
SCOPE char track2[FIFO_SIZE];

SCOPE int track1_len;
SCOPE int track2_len;

//%B1234567890123456^DOE/JOHN           ^25051234567890000000?  <-- track 1 example
//;1234567890123456=25051234567890000000? <-- track 2 example

// only ASCII 0x20–0x7F are used on Track 1; adjust size if you need more
SCOPE const uint8_t track1_lut[128];

// Track-2 reversed‐bit LUT: ASCII → bit-reversed 5-bit code
SCOPE const uint8_t track2_lut_rev[128];

// Track-2 original LUT: ASCII → 5-bit code (not bit-reversed)
SCOPE const uint8_t track2_lut[128];

SCOPE void reverse_string(char *input);
SCOPE void toggle_coil_polarity();
SCOPE void set_polarity_low();
SCOPE void send_one(int index);
SCOPE void send_zero(int index);
SCOPE void send_track(int index);
SCOPE void send_zeros(int track, int count);
SCOPE void load_track_one(char* track, int length);
SCOPE void load_track_two(char* track, int length);
SCOPE void load_track_two_rev(char* track, int length);
SCOPE int convert_track_one(char* track, int length);
SCOPE int convert_track_two(char* track, int length);
SCOPE int appendTrack1LRC(char *codes, int length);
SCOPE int appendTrack2LRC(char *codes, int length);
SCOPE char validate_track(char* track, int length, int track_num);
SCOPE int clean_track(char* track, int length, int track_num);
SCOPE void send_both_tracks();
SCOPE void delay_between_tracks(int time);
SCOPE void init_tracks(char* track1_str, char* track2_str);
SCOPE void set_track(int track_num, char* track_str);
SCOPE void transmit_both_tracks(int delay);
SCOPE void transmit_track_one();
SCOPE void transmit_track_two();
SCOPE void init_track(int index, char* track_str);

#undef SCOPE
#endif
