#ifndef INC_UART_MODIFIERS_H_
#define INC_UART_MODIFIERS_H_

// ──────────────────────────────────────────────────
// 1. ESC and Reset
// ──────────────────────────────────────────────────
#define ESC              "\x1B"
#define RESET            "\x1B[0m"

// ──────────────────────────────────────────────────
// 2. Text Attributes
// ──────────────────────────────────────────────────
#define BOLD             "\x1B[1m"
#define DIM              "\x1B[2m"
#define UNDERLINE        "\x1B[4m"
#define BLINK            "\x1B[5m"
#define INVERSE          "\x1B[7m"
#define HIDDEN           "\x1B[8m"

// ──────────────────────────────────────────────────
// 3. Standard Foreground (Text) Colors
// ──────────────────────────────────────────────────
#define BLACK         "\x1B[30m"
#define RED           "\x1B[31m"
#define GREEN         "\x1B[32m"
#define YELLOW        "\x1B[33m"
#define BLUE          "\x1B[34m"
#define MAGENTA       "\x1B[35m"
#define CYAN          "\x1B[36m"
#define WHITE         "\x1B[37m"

// ──────────────────────────────────────────────────
// 4. Bright (High‐Intensity) Foreground Colors
// ──────────────────────────────────────────────────
#define BRIGHT_BLACK   "\x1B[90m"
#define BRIGHT_RED     "\x1B[91m"
#define BRIGHT_GREEN   "\x1B[92m"
#define BRIGHT_YELLOW  "\x1B[93m"
#define BRIGHT_BLUE    "\x1B[94m"
#define BRIGHT_MAGENTA "\x1B[95m"
#define BRIGHT_CYAN    "\x1B[96m"
#define BRIGHT_WHITE   "\x1B[97m"

// ──────────────────────────────────────────────────
// 5. Standard Background Colors
// ──────────────────────────────────────────────────
#define BG_BLACK         "\x1B[40m"
#define BG_RED           "\x1B[41m"
#define BG_GREEN         "\x1B[42m"
#define BG_YELLOW        "\x1B[43m"
#define BG_BLUE          "\x1B[44m"
#define BG_MAGENTA       "\x1B[45m"
#define BG_CYAN          "\x1B[46m"
#define BG_WHITE         "\x1B[47m"

// ──────────────────────────────────────────────────
// 6. Bright (High‐Intensity) Background Colors
// ──────────────────────────────────────────────────
#define BG_BRIGHT_BLACK   "\x1B[100m"
#define BG_BRIGHT_RED     "\x1B[101m"
#define BG_BRIGHT_GREEN   "\x1B[102m"
#define BG_BRIGHT_YELLOW  "\x1B[103m"
#define BG_BRIGHT_BLUE    "\x1B[104m"
#define BG_BRIGHT_MAGENTA "\x1B[105m"
#define BG_BRIGHT_CYAN    "\x1B[106m"
#define BG_BRIGHT_WHITE   "\x1B[107m"

#endif /* INC_UART_MODIFIERS_H_ */
