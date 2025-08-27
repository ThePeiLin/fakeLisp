#include "readline_config.h"

#include <fakeLisp/readline.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32

#define UTF8_CODE_PAGE (65001)

#include <conio.h>
#include <io.h>
#include <windows.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO _fileno(stdin)
#define STDOUT_FILENO _fileno(stdout)
#endif
#define fileno _fileno
#define isatty _isatty
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define fstat _fstat
#define stat _stat
#define read _read
#define strdup _strdup

static UINT console_input_cp = 0;
static UINT console_output_cp = 0;

static DWORD console_origin_output_mode = 0;
static DWORD console_origin_input_mode = 0;

static HANDLE console_output_handle = INVALID_HANDLE_VALUE;
static HANDLE console_input_handle = INVALID_HANDLE_VALUE;

static int const is_readline_win = 1;

static inline int _my_read(int fd) {
    (void)fd;
    fflush(stdout);
    return _getch();
}

#else

#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
static int const is_readline_win = 0;
static struct termios orig_termios;

static inline int _my_read(int fd) {
    char ch = 0;
    int r = read(STDIN_FILENO, &ch, 1);
    (void)r;
    assert(r);
    return ch;
}

#endif

#define MIN(X, Y) ((Y) > (X) ? (X) : (Y))
#define MAX(X, Y) ((Y) < (X) ? (X) : (Y))

// Make control-characters readable
#define CTRL_KEY(key) (key - 0x40)
// Build special key code for escape sequences
#define ALT_KEY(key) (key + ((KEY_ESC + 1) << 8))
#define ESC_KEY3(ch) ((KEY_ESC << 8) + ch)
#define ESC_KEY4(ch1, ch2) ((KEY_ESC << 8) + ((ch1) << 16) + ch2)
#define ESC_KEY6(ch1, ch2, ch3)                                                \
    ((KEY_ESC << 8) + ((ch1) << 16) + ((ch2) << 24) + ch3)
#define ESC_OKEY(ch) ((KEY_ESC << 8) + ('O' << 16) + ch)

enum {
    KEY_TAB = 9,       // Autocomplete.
    KEY_BACKSPACE = 8, // Delete character before cursor.
    KEY_ENTER = 13,    // Accept line. (Linux)
    KEY_ENTER2 = 10,   // Accept line. (Windows)
    KEY_ESC = 27,      // Escapce
    KEY_DEL2 = 127,    // It's treaded as Backspace is Linux
    KEY_DEBUG = 30,    // Ctrl-^ Enter keyboard debug mode

#ifdef _WIN32 // Windows

    KEY_INSERT = (KEY_ESC << 8) + 'R', // Paste last cut text.
    KEY_DEL = (KEY_ESC << 8) + 'S',    // Delete character under cursor.
    KEY_HOME = (KEY_ESC << 8) + 'G',   // Move cursor to start of line.
    KEY_END = (KEY_ESC << 8) + 'O',    // Move cursor to end of line.
    KEY_PGUP = (KEY_ESC << 8) + 'I',   // Move to first line in history.
    KEY_PGDN = (KEY_ESC << 8) + 'Q',   // Move to end of input history.
    KEY_UP = (KEY_ESC << 8) + 'H',     // Fetch previous line in history.
    KEY_DOWN = (KEY_ESC << 8) + 'P',   // Fetch next line in history.
    KEY_LEFT = (KEY_ESC << 8) + 'K',   // Move back a character.
    KEY_RIGHT = (KEY_ESC << 8) + 'M',  // Move forward a character.

    KEY_CTRL_DEL = (KEY_ESC << 8) + 147,  // Cut word following cursor.
    KEY_CTRL_HOME = (KEY_ESC << 8) + 'w', // Cut from start of line to cursor.
    KEY_CTRL_END = (KEY_ESC << 8) + 'u',  // Cut from cursor to end of line.
    KEY_CTRL_UP = (KEY_ESC << 8) + 141, // Uppercase current or following word.
    KEY_CTRL_DOWN =
            (KEY_ESC << 8) + 145, // Lowercase current or following word.
    KEY_CTRL_LEFT = (KEY_ESC << 8) + 's',  // Move back a word.
    KEY_CTRL_RIGHT = (KEY_ESC << 8) + 't', // Move forward a word.
    KEY_CTRL_BACKSPACE =
            (KEY_ESC << 8) + 127, // Cut from start of line to cursor.

    KEY_ALT_DEL = ALT_KEY(163),   // Cut word following cursor.
    KEY_ALT_HOME = ALT_KEY(151),  // Cut from start of line to cursor.
    KEY_ALT_END = ALT_KEY(159),   // Cut from cursor to end of line.
    KEY_ALT_UP = ALT_KEY(152),    // Uppercase current or following word.
    KEY_ALT_DOWN = ALT_KEY(160),  // Lowercase current or following word.
    KEY_ALT_LEFT = ALT_KEY(155),  // Move back a word.
    KEY_ALT_RIGHT = ALT_KEY(157), // Move forward a word.
    KEY_ALT_BACKSPACE =
            ALT_KEY(KEY_BACKSPACE), // Cut from start of line to cursor.

    KEY_F1 = (KEY_ESC << 8) + ';', // Show help.
    KEY_F2 = (KEY_ESC << 8) + '<', // Show history.
    KEY_F3 = (KEY_ESC << 8) + '=', // Clear history (need confirm).
    KEY_F4 = (KEY_ESC << 8) + '>', // Search history with current input.

#else // Linux

    KEY_INSERT = ESC_KEY4('2', '~'), // vt100 Esc[2~: Paste last cut text.
    KEY_DEL =
            ESC_KEY4('3', '~'), // vt100 Esc[3~: Delete character under cursor.
    KEY_HOME =
            ESC_KEY4('1', '~'),   // vt100 Esc[1~: Move cursor to start of line.
    KEY_END = ESC_KEY4('4', '~'), // vt100 Esc[4~: Move cursor to end of line.
    KEY_PGUP =
            ESC_KEY4('5', '~'), // vt100 Esc[5~: Move to first line in history.
    KEY_PGDN =
            ESC_KEY4('6', '~'), // vt100 Esc[6~: Move to end of input history.
    KEY_UP = ESC_KEY3('A'),     //       Esc[A: Fetch previous line in history.
    KEY_DOWN = ESC_KEY3('B'),   //       Esc[B: Fetch next line in history.
    KEY_LEFT = ESC_KEY3('D'),   //       Esc[D: Move back a character.
    KEY_RIGHT = ESC_KEY3('C'),  //       Esc[C: Move forward a character.
    KEY_HOME2 = ESC_KEY3('H'),  // xterm Esc[H: Move cursor to start of line.
    KEY_END2 = ESC_KEY3('F'),   // xterm Esc[F: Move cursor to end of line.

    KEY_CTRL_DEL = ESC_KEY6('3',
            '5',
            '~'), // xterm Esc[3;5~: Cut word following cursor.
    KEY_CTRL_HOME = ESC_KEY6('1',
            '5',
            'H'), // xterm Esc[1;5H: Cut from start of line to cursor.
    KEY_CTRL_END = ESC_KEY6('1',
            '5',
            'F'), // xterm Esc[1;5F: Cut from cursor to end of line.
    KEY_CTRL_UP = ESC_KEY6('1',
            '5',
            'A'), // xterm Esc[1;5A: Uppercase current or following word.
    KEY_CTRL_DOWN = ESC_KEY6('1',
            '5',
            'B'), // xterm Esc[1;5B: Lowercase current or following word.
    KEY_CTRL_LEFT =
            ESC_KEY6('1', '5', 'D'), // xterm Esc[1;5D: Move back a word.
    KEY_CTRL_RIGHT =
            ESC_KEY6('1', '5', 'C'), // xterm Esc[1;5C: Move forward a word.
    KEY_CTRL_BACKSPACE = 31,         // xterm Cut from start of line to cursor.
    KEY_CTRL_UP2 =
            ESC_OKEY('A'), // vt100 EscOA: Uppercase current or following word.
    KEY_CTRL_DOWN2 =
            ESC_OKEY('B'), // vt100 EscOB: Lowercase current or following word.
    KEY_CTRL_LEFT2 = ESC_OKEY('D'),  // vt100 EscOD: Move back a word.
    KEY_CTRL_RIGHT2 = ESC_OKEY('C'), // vt100 EscOC: Move forward a word.

    KEY_ALT_DEL = ESC_KEY6('3',
            '3',
            '~'), // xterm Esc[3;3~: Cut word following cursor.
    KEY_ALT_HOME = ESC_KEY6('1',
            '3',
            'H'), // xterm Esc[1;3H: Cut from start of line to cursor.
    KEY_ALT_END = ESC_KEY6('1',
            '3',
            'F'), // xterm Esc[1;3F: Cut from cursor to end of line.
    KEY_ALT_UP = ESC_KEY6('1',
            '3',
            'A'), // xterm Esc[1;3A: Uppercase current or following word.
    KEY_ALT_DOWN = ESC_KEY6('1',
            '3',
            'B'), // xterm Esc[1;3B: Lowercase current or following word.
    KEY_ALT_LEFT = ESC_KEY6('1', '3', 'D'), // xterm Esc[1;3D: Move back a word.
    KEY_ALT_RIGHT =
            ESC_KEY6('1', '3', 'C'), // xterm Esc[1;3C: Move forward a word.
    KEY_ALT_BACKSPACE = ALT_KEY(KEY_DEL2), // Cut from start of line to cursor.

    KEY_F1 = ESC_OKEY('P'), // 	  EscOP: Show help.
    KEY_F2 = ESC_OKEY('Q'), // 	  EscOQ: Show history.
    KEY_F3 = ESC_OKEY('R'), //       EscOP: Clear history (need confirm).
    KEY_F4 = ESC_OKEY('S'), //       EscOP: Search history with current input.

    KEY_F1_2 = ESC_KEY4('[', 'A'), // linux Esc[[A: Show help.
    KEY_F2_2 = ESC_KEY4('[', 'B'), // linux Esc[[B: Show history.
    KEY_F3_2 =
            ESC_KEY4('[', 'C'), // linux Esc[[C: Clear history (need confirm).
    KEY_F4_2 = ESC_KEY4('[',
            'D'), // linux Esc[[D: Search history with current input.

#endif
};

#define UTF8_ASCII (0x80)

#define UTF8_2_MASK (0xE0)
#define UTF8_2 (0xC0)
#define UTF8_2_FIRST_CODE_MASK (0x1F)

#define UTF8_3_MASK (0xF0)
#define UTF8_3 (0xE0)
#define UTF8_3_FIRST_CODE_MASK (0x0F)

#define UTF8_4_MASK (0xF8)
#define UTF8_4 (0xF0)
#define UTF8_4_FIRST_CODE_MASK (0x07)

#define UTF8_5_MASK (0xFC)
#define UTF8_5 (0xF8)
#define UTF8_5_FIRST_CODE_MASK (0x03)

#define UTF8_6_MASK (0xFE)
#define UTF8_6 (0xFC)
#define UTF8_6_FIRST_CODE_MASK (0x01)

#define UTF8_REST_MASK (0xC0)
#define UTF8_REST (0x80)
#define UTF8_REST_BIT_WIDTH (6)
#define UTF8_REST_CODE_MASK (0x3F)

#define UTF8_ASCII_MAX (0x7F)
#define UTF8_2_MAX (0x7FF)
#define UTF8_3_MAX (0xFFFF)
#define UTF8_4_MAX (0x1FFFFF)
#define UTF8_5_MAX (0x3FFFFFF)
#define UTF8_7_MAX (0x7FFFFFFF)

#ifndef FKL_READLINE_MAX_HISTORY
#define FKL_READLINE_MAX_HISTORY (1024)
#endif

#ifndef FKL_READLINE_MALLOC
#define FKL_READLINE_MALLOC malloc
#endif

#ifndef FKL_READLINE_FREE
#define FKL_READLINE_FREE free
#endif

#ifndef FKL_READLINE_REALLOC
#define FKL_READLINE_REALLOC realloc
#endif

#ifndef FKL_READLINE_STRDUP
#define FKL_READLINE_STRDUP strdup
#endif

#define MALLOC FKL_READLINE_MALLOC
#define FREE FKL_READLINE_FREE
#define REALLOC FKL_READLINE_REALLOC
#define STRDUP FKL_READLINE_STRDUP

static unsigned history_len;
static char *history[FKL_READLINE_MAX_HISTORY];

#ifndef _WIN32
static int rawmode = -1;
static inline void readline_get_screen(int *row, int *col, FILE *out) {
    struct winsize ws = { 0 };
    int r = ioctl(fileno(out), TIOCGWINSZ, &ws);
    assert(r == 0);
    (void)r;
    *row = ws.ws_row;
    *col = ws.ws_col;

    if (*row < 1)
        *row = 24;
    if (*col < 1)
        *col = 160;

    // 留出空间放回车
    *col -= 1;
}

static inline int enable_raw_mode(int fd) {
    struct termios raw;
    if (tcgetattr(fd, &orig_termios) != -1) {
        raw = orig_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_lflag &= ~(ICANON | ECHO | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(fd, TCSANOW, &raw) < 0) {
            perror("tcsetattr");
        } else {
            rawmode = fd;
            return 0;
        }
    }
    errno = ENOTTY;
    return -1;
}

static inline void disable_raw_mode(void) {
    if (rawmode != -1) {
        tcsetattr(rawmode, TCSADRAIN, &orig_termios);
        rawmode = -1;
    }
}
#else

static inline void readline_get_screen(int *row, int *col, FILE *out) {
    (void)out;

    CONSOLE_SCREEN_BUFFER_INFO inf;
    GetConsoleScreenBufferInfo(console_output_handle, &inf);

    *col = inf.srWindow.Right - inf.srWindow.Left + 1;
    *row = inf.srWindow.Bottom - inf.srWindow.Top + 1;
    *col = *col > 1 ? *col : 160;
    *row = *row > 1 ? *row : 24;

    assert(*col);

    // 留出空间放回车
    *col -= 1;
}

static inline int enable_raw_mode(int fd) {
    (void)fd;

    if (console_output_handle != INVALID_HANDLE_VALUE)
        return 0;

    console_output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    console_input_handle = GetStdHandle(STD_INPUT_HANDLE);

    console_input_cp = GetConsoleCP();
    console_output_cp = GetConsoleOutputCP();

    SetConsoleCP(UTF8_CODE_PAGE);
    SetConsoleOutputCP(UTF8_CODE_PAGE);

    GetConsoleMode(console_output_handle, &console_origin_output_mode);
    GetConsoleMode(console_input_handle, &console_origin_input_mode);

    SetConsoleMode(console_output_handle,
            console_origin_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    DWORD mode = console_origin_input_mode;

    mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);

    SetConsoleMode(console_input_handle, mode);

    return 0;
}

static inline void disable_raw_mode(void) {
    if (console_output_handle == INVALID_HANDLE_VALUE)
        return;

    SetConsoleCP(console_input_cp);
    SetConsoleOutputCP(console_output_cp);

    SetConsoleMode(console_output_handle, console_origin_output_mode);
    SetConsoleMode(console_input_handle, console_origin_input_mode);

    console_output_handle = INVALID_HANDLE_VALUE;
    console_input_handle = INVALID_HANDLE_VALUE;
}

#endif

typedef struct StringBuffer {
    char *buf;
    int index;
    int capacity;
} StringBuffer;

static inline void init_string_buffer(StringBuffer *s) {
    s->index = 0;
    s->capacity = 8;
    s->buf = (char *)MALLOC(s->capacity * sizeof(char));
    assert(s->buf);
    s->buf[0] = '\0';
}

static inline void uninit_string_buffer(StringBuffer *s) {
    s->index = 0;
    s->capacity = 0;
    FREE(s->buf);
    s->buf = NULL;
}

static inline void string_buffer_clear(StringBuffer *s) {
    s->index = 0;
    s->buf[0] = '\0';
}

static inline void string_buffer_reserve(StringBuffer *s, int target) {
    if (s->capacity < target) {
        s->capacity <<= 1;
        if (s->capacity < target)
            s->capacity = target;
        char *t = (char *)REALLOC(s->buf, s->capacity * sizeof(char));
        assert(t);
        s->buf = t;
    }
}

static inline void string_buffer_putc(StringBuffer *s, int c) {
    string_buffer_reserve(s, s->index + 2);
    s->buf[s->index++] = (char)c;
    s->buf[s->index] = '\0';
}

static inline void string_buffer_puts(StringBuffer *s, const char *str) {
    int len = (int)strlen(str);
    string_buffer_reserve(s, s->index + len + 1);
    memcpy(&s->buf[s->index], str, len);
    s->index += len;
    s->buf[s->index] = '\0';
}

static inline long
string_buffer_vprintf(StringBuffer *b, const char *fmt, va_list ap) {
    long n;
    va_list cp;
    for (;;) {
#ifdef _WIN32
        cp = ap;
#else
        va_copy(cp, ap);
#endif
        n = vsnprintf(&b->buf[b->index], b->capacity - b->index, fmt, cp);
        va_end(cp);
        if ((n > -1) && n < (b->capacity - b->index)) {
            b->index += n;
            return n;
        }
        if (n > -1)
            string_buffer_reserve(b, b->index + n + 1);
        else
            string_buffer_reserve(b, b->index + (b->capacity) * 2);
    }
    return n;
}

static inline long string_buffer_printf(StringBuffer *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = string_buffer_vprintf(s, fmt, ap);
    va_end(ap);
    return r;
}

// steal from bestline: https://github.com/jart/bestline
static inline uint32_t cwidth(int32_t c) {
    if ((0x000 <= c && c <= 0x01F) || (0x07F <= c && c <= 0x09F)
            || (0x300 <= c && c <= 0x36f) || (0x483 <= c && c <= 0x489)
            || (0x591 <= c && c <= 0x5bd) || (0x5bf <= c && c <= 0x5bf)
            || (0x5c1 <= c && c <= 0x5c2) || (0x5c4 <= c && c <= 0x5c5)
            || (0x5c7 <= c && c <= 0x5c7) || (0x610 <= c && c <= 0x61a)
            || (0x61c <= c && c <= 0x61c) || (0x64b <= c && c <= 0x65f)
            || (0x670 <= c && c <= 0x670) || (0x6d6 <= c && c <= 0x6dc)
            || (0x6df <= c && c <= 0x6e4) || (0x6e7 <= c && c <= 0x6e8)
            || (0x6ea <= c && c <= 0x6ed) || (0x711 <= c && c <= 0x711)
            || (0x730 <= c && c <= 0x74a) || (0x7a6 <= c && c <= 0x7b0)
            || (0x7eb <= c && c <= 0x7f3) || (0x7fd <= c && c <= 0x7fd)
            || (0x816 <= c && c <= 0x819) || (0x81b <= c && c <= 0x823)
            || (0x825 <= c && c <= 0x827) || (0x829 <= c && c <= 0x82d)
            || (0x859 <= c && c <= 0x85b) || (0x898 <= c && c <= 0x89f)
            || (0x8ca <= c && c <= 0x8e1) || (0x8e3 <= c && c <= 0x902)
            || (0x93a <= c && c <= 0x93a) || (0x93c <= c && c <= 0x93c)
            || (0x941 <= c && c <= 0x948) || (0x94d <= c && c <= 0x94d)
            || (0x951 <= c && c <= 0x957) || (0x962 <= c && c <= 0x963)
            || (0x981 <= c && c <= 0x981) || (0x9bc <= c && c <= 0x9bc)
            || (0x9c1 <= c && c <= 0x9c4) || (0x9cd <= c && c <= 0x9cd)
            || (0x9e2 <= c && c <= 0x9e3) || (0x9fe <= c && c <= 0x9fe)
            || (0xa01 <= c && c <= 0xa02) || (0xa3c <= c && c <= 0xa3c)
            || (0xa41 <= c && c <= 0xa42) || (0xa47 <= c && c <= 0xa48)
            || (0xa4b <= c && c <= 0xa4d) || (0xa51 <= c && c <= 0xa51)
            || (0xa70 <= c && c <= 0xa71) || (0xa75 <= c && c <= 0xa75)
            || (0xa81 <= c && c <= 0xa82) || (0xabc <= c && c <= 0xabc)
            || (0xac1 <= c && c <= 0xac5) || (0xac7 <= c && c <= 0xac8)
            || (0xacd <= c && c <= 0xacd) || (0xae2 <= c && c <= 0xae3)
            || (0xafa <= c && c <= 0xaff) || (0xb01 <= c && c <= 0xb01)
            || (0xb3c <= c && c <= 0xb3c) || (0xb3f <= c && c <= 0xb3f)
            || (0xb41 <= c && c <= 0xb44) || (0xb4d <= c && c <= 0xb4d)
            || (0xb55 <= c && c <= 0xb56) || (0xb62 <= c && c <= 0xb63)
            || (0xb82 <= c && c <= 0xb82) || (0xbc0 <= c && c <= 0xbc0)
            || (0xbcd <= c && c <= 0xbcd) || (0xc00 <= c && c <= 0xc00)
            || (0xc04 <= c && c <= 0xc04) || (0xc3c <= c && c <= 0xc3c)
            || (0xc3e <= c && c <= 0xc40) || (0xc46 <= c && c <= 0xc48)
            || (0xc4a <= c && c <= 0xc4d) || (0xc55 <= c && c <= 0xc56)
            || (0xc62 <= c && c <= 0xc63) || (0xc81 <= c && c <= 0xc81)
            || (0xcbc <= c && c <= 0xcbc) || (0xcbf <= c && c <= 0xcbf)
            || (0xcc6 <= c && c <= 0xcc6) || (0xccc <= c && c <= 0xccd)
            || (0xce2 <= c && c <= 0xce3) || (0xd00 <= c && c <= 0xd01)
            || (0xd3b <= c && c <= 0xd3c) || (0xd41 <= c && c <= 0xd44)
            || (0xd4d <= c && c <= 0xd4d) || (0xd62 <= c && c <= 0xd63)
            || (0xd81 <= c && c <= 0xd81) || (0xdca <= c && c <= 0xdca)
            || (0xdd2 <= c && c <= 0xdd4) || (0xdd6 <= c && c <= 0xdd6)
            || (0xe31 <= c && c <= 0xe31) || (0xe34 <= c && c <= 0xe3a)
            || (0xe47 <= c && c <= 0xe4e) || (0xeb1 <= c && c <= 0xeb1)
            || (0xeb4 <= c && c <= 0xebc) || (0xec8 <= c && c <= 0xece)
            || (0xf18 <= c && c <= 0xf19) || (0xf35 <= c && c <= 0xf35)
            || (0xf37 <= c && c <= 0xf37) || (0xf39 <= c && c <= 0xf39)
            || (0xf71 <= c && c <= 0xf7e) || (0xf80 <= c && c <= 0xf84)
            || (0xf86 <= c && c <= 0xf87) || (0xf8d <= c && c <= 0xf97)
            || (0xf99 <= c && c <= 0xfbc) || (0xfc6 <= c && c <= 0xfc6)
            || (0x102d <= c && c <= 0x1030) || (0x1032 <= c && c <= 0x1037)
            || (0x1039 <= c && c <= 0x103a) || (0x103d <= c && c <= 0x103e)
            || (0x1058 <= c && c <= 0x1059) || (0x105e <= c && c <= 0x1060)
            || (0x1071 <= c && c <= 0x1074) || (0x1082 <= c && c <= 0x1082)
            || (0x1085 <= c && c <= 0x1086) || (0x108d <= c && c <= 0x108d)
            || (0x109d <= c && c <= 0x109d) || (0x1160 <= c && c <= 0x11ff)
            || (0x135d <= c && c <= 0x135f) || (0x1712 <= c && c <= 0x1714)
            || (0x1732 <= c && c <= 0x1733) || (0x1752 <= c && c <= 0x1753)
            || (0x1772 <= c && c <= 0x1773) || (0x17b4 <= c && c <= 0x17b5)
            || (0x17b7 <= c && c <= 0x17bd) || (0x17c6 <= c && c <= 0x17c6)
            || (0x17c9 <= c && c <= 0x17d3) || (0x17dd <= c && c <= 0x17dd)
            || (0x180b <= c && c <= 0x180f) || (0x1885 <= c && c <= 0x1886)
            || (0x18a9 <= c && c <= 0x18a9) || (0x1920 <= c && c <= 0x1922)
            || (0x1927 <= c && c <= 0x1928) || (0x1932 <= c && c <= 0x1932)
            || (0x1939 <= c && c <= 0x193b) || (0x1a17 <= c && c <= 0x1a18)
            || (0x1a1b <= c && c <= 0x1a1b) || (0x1a56 <= c && c <= 0x1a56)
            || (0x1a58 <= c && c <= 0x1a5e) || (0x1a60 <= c && c <= 0x1a60)
            || (0x1a62 <= c && c <= 0x1a62) || (0x1a65 <= c && c <= 0x1a6c)
            || (0x1a73 <= c && c <= 0x1a7c) || (0x1a7f <= c && c <= 0x1a7f)
            || (0x1ab0 <= c && c <= 0x1ace) || (0x1b00 <= c && c <= 0x1b03)
            || (0x1b34 <= c && c <= 0x1b34) || (0x1b36 <= c && c <= 0x1b3a)
            || (0x1b3c <= c && c <= 0x1b3c) || (0x1b42 <= c && c <= 0x1b42)
            || (0x1b6b <= c && c <= 0x1b73) || (0x1b80 <= c && c <= 0x1b81)
            || (0x1ba2 <= c && c <= 0x1ba5) || (0x1ba8 <= c && c <= 0x1ba9)
            || (0x1bab <= c && c <= 0x1bad) || (0x1be6 <= c && c <= 0x1be6)
            || (0x1be8 <= c && c <= 0x1be9) || (0x1bed <= c && c <= 0x1bed)
            || (0x1bef <= c && c <= 0x1bf1) || (0x1c2c <= c && c <= 0x1c33)
            || (0x1c36 <= c && c <= 0x1c37) || (0x1cd0 <= c && c <= 0x1cd2)
            || (0x1cd4 <= c && c <= 0x1ce0) || (0x1ce2 <= c && c <= 0x1ce8)
            || (0x1ced <= c && c <= 0x1ced) || (0x1cf4 <= c && c <= 0x1cf4)
            || (0x1cf8 <= c && c <= 0x1cf9) || (0x1dc0 <= c && c <= 0x1dff)
            || (0x200b <= c && c <= 0x200f) || (0x202a <= c && c <= 0x202e)
            || (0x2060 <= c && c <= 0x2064) || (0x2066 <= c && c <= 0x206f)
            || (0x20d0 <= c && c <= 0x20f0) || (0x2cef <= c && c <= 0x2cf1)
            || (0x2d7f <= c && c <= 0x2d7f) || (0x2de0 <= c && c <= 0x2dff)
            || (0x302a <= c && c <= 0x302d) || (0x3099 <= c && c <= 0x309a)
            || (0xa66f <= c && c <= 0xa672) || (0xa674 <= c && c <= 0xa67d)
            || (0xa69e <= c && c <= 0xa69f) || (0xa6f0 <= c && c <= 0xa6f1)
            || (0xa802 <= c && c <= 0xa802) || (0xa806 <= c && c <= 0xa806)
            || (0xa80b <= c && c <= 0xa80b) || (0xa825 <= c && c <= 0xa826)
            || (0xa82c <= c && c <= 0xa82c) || (0xa8c4 <= c && c <= 0xa8c5)
            || (0xa8e0 <= c && c <= 0xa8f1) || (0xa8ff <= c && c <= 0xa8ff)
            || (0xa926 <= c && c <= 0xa92d) || (0xa947 <= c && c <= 0xa951)
            || (0xa980 <= c && c <= 0xa982) || (0xa9b3 <= c && c <= 0xa9b3)
            || (0xa9b6 <= c && c <= 0xa9b9) || (0xa9bc <= c && c <= 0xa9bd)
            || (0xa9e5 <= c && c <= 0xa9e5) || (0xaa29 <= c && c <= 0xaa2e)
            || (0xaa31 <= c && c <= 0xaa32) || (0xaa35 <= c && c <= 0xaa36)
            || (0xaa43 <= c && c <= 0xaa43) || (0xaa4c <= c && c <= 0xaa4c)
            || (0xaa7c <= c && c <= 0xaa7c) || (0xaab0 <= c && c <= 0xaab0)
            || (0xaab2 <= c && c <= 0xaab4) || (0xaab7 <= c && c <= 0xaab8)
            || (0xaabe <= c && c <= 0xaabf) || (0xaac1 <= c && c <= 0xaac1)
            || (0xaaec <= c && c <= 0xaaed) || (0xaaf6 <= c && c <= 0xaaf6)
            || (0xabe5 <= c && c <= 0xabe5) || (0xabe8 <= c && c <= 0xabe8)
            || (0xabed <= c && c <= 0xabed) || (0xd7b0 <= c && c <= 0xd7c6)
            || (0xd7cb <= c && c <= 0xd7fb) || (0xfb1e <= c && c <= 0xfb1e)
            || (0xfe00 <= c && c <= 0xfe0f) || (0xfe20 <= c && c <= 0xfe2f)
            || (0xfeff <= c && c <= 0xfeff) || (0xfff9 <= c && c <= 0xfffb)
            || (0x101fd <= c && c <= 0x101fd) || (0x102e0 <= c && c <= 0x102e0)
            || (0x10376 <= c && c <= 0x1037a) || (0x10a01 <= c && c <= 0x10a03)
            || (0x10a05 <= c && c <= 0x10a06) || (0x10a0c <= c && c <= 0x10a0f)
            || (0x10a38 <= c && c <= 0x10a3a) || (0x10a3f <= c && c <= 0x10a3f)
            || (0x10ae5 <= c && c <= 0x10ae6) || (0x10d24 <= c && c <= 0x10d27)
            || (0x10eab <= c && c <= 0x10eac) || (0x10efd <= c && c <= 0x10eff)
            || (0x10f46 <= c && c <= 0x10f50) || (0x10f82 <= c && c <= 0x10f85)
            || (0x11001 <= c && c <= 0x11001) || (0x11038 <= c && c <= 0x11046)
            || (0x11070 <= c && c <= 0x11070) || (0x11073 <= c && c <= 0x11074)
            || (0x1107f <= c && c <= 0x11081) || (0x110b3 <= c && c <= 0x110b6)
            || (0x110b9 <= c && c <= 0x110ba) || (0x110c2 <= c && c <= 0x110c2)
            || (0x11100 <= c && c <= 0x11102) || (0x11127 <= c && c <= 0x1112b)
            || (0x1112d <= c && c <= 0x11134) || (0x11173 <= c && c <= 0x11173)
            || (0x11180 <= c && c <= 0x11181) || (0x111b6 <= c && c <= 0x111be)
            || (0x111c9 <= c && c <= 0x111cc) || (0x111cf <= c && c <= 0x111cf)
            || (0x1122f <= c && c <= 0x11231) || (0x11234 <= c && c <= 0x11234)
            || (0x11236 <= c && c <= 0x11237) || (0x1123e <= c && c <= 0x1123e)
            || (0x11241 <= c && c <= 0x11241) || (0x112df <= c && c <= 0x112df)
            || (0x112e3 <= c && c <= 0x112ea) || (0x11300 <= c && c <= 0x11301)
            || (0x1133b <= c && c <= 0x1133c) || (0x11340 <= c && c <= 0x11340)
            || (0x11366 <= c && c <= 0x1136c) || (0x11370 <= c && c <= 0x11374)
            || (0x11438 <= c && c <= 0x1143f) || (0x11442 <= c && c <= 0x11444)
            || (0x11446 <= c && c <= 0x11446) || (0x1145e <= c && c <= 0x1145e)
            || (0x114b3 <= c && c <= 0x114b8) || (0x114ba <= c && c <= 0x114ba)
            || (0x114bf <= c && c <= 0x114c0) || (0x114c2 <= c && c <= 0x114c3)
            || (0x115b2 <= c && c <= 0x115b5) || (0x115bc <= c && c <= 0x115bd)
            || (0x115bf <= c && c <= 0x115c0) || (0x115dc <= c && c <= 0x115dd)
            || (0x11633 <= c && c <= 0x1163a) || (0x1163d <= c && c <= 0x1163d)
            || (0x1163f <= c && c <= 0x11640) || (0x116ab <= c && c <= 0x116ab)
            || (0x116ad <= c && c <= 0x116ad) || (0x116b0 <= c && c <= 0x116b5)
            || (0x116b7 <= c && c <= 0x116b7) || (0x1171d <= c && c <= 0x1171f)
            || (0x11722 <= c && c <= 0x11725) || (0x11727 <= c && c <= 0x1172b)
            || (0x1182f <= c && c <= 0x11837) || (0x11839 <= c && c <= 0x1183a)
            || (0x1193b <= c && c <= 0x1193c) || (0x1193e <= c && c <= 0x1193e)
            || (0x11943 <= c && c <= 0x11943) || (0x119d4 <= c && c <= 0x119d7)
            || (0x119da <= c && c <= 0x119db) || (0x119e0 <= c && c <= 0x119e0)
            || (0x11a01 <= c && c <= 0x11a0a) || (0x11a33 <= c && c <= 0x11a38)
            || (0x11a3b <= c && c <= 0x11a3e) || (0x11a47 <= c && c <= 0x11a47)
            || (0x11a51 <= c && c <= 0x11a56) || (0x11a59 <= c && c <= 0x11a5b)
            || (0x11a8a <= c && c <= 0x11a96) || (0x11a98 <= c && c <= 0x11a99)
            || (0x11c30 <= c && c <= 0x11c36) || (0x11c38 <= c && c <= 0x11c3d)
            || (0x11c3f <= c && c <= 0x11c3f) || (0x11c92 <= c && c <= 0x11ca7)
            || (0x11caa <= c && c <= 0x11cb0) || (0x11cb2 <= c && c <= 0x11cb3)
            || (0x11cb5 <= c && c <= 0x11cb6) || (0x11d31 <= c && c <= 0x11d36)
            || (0x11d3a <= c && c <= 0x11d3a) || (0x11d3c <= c && c <= 0x11d3d)
            || (0x11d3f <= c && c <= 0x11d45) || (0x11d47 <= c && c <= 0x11d47)
            || (0x11d90 <= c && c <= 0x11d91) || (0x11d95 <= c && c <= 0x11d95)
            || (0x11d97 <= c && c <= 0x11d97) || (0x11ef3 <= c && c <= 0x11ef4)
            || (0x11f00 <= c && c <= 0x11f01) || (0x11f36 <= c && c <= 0x11f3a)
            || (0x11f40 <= c && c <= 0x11f40) || (0x11f42 <= c && c <= 0x11f42)
            || (0x13430 <= c && c <= 0x13440) || (0x13447 <= c && c <= 0x13455)
            || (0x16af0 <= c && c <= 0x16af4) || (0x16b30 <= c && c <= 0x16b36)
            || (0x16f4f <= c && c <= 0x16f4f) || (0x16f8f <= c && c <= 0x16f92)
            || (0x16fe4 <= c && c <= 0x16fe4) || (0x1bc9d <= c && c <= 0x1bc9e)
            || (0x1bca0 <= c && c <= 0x1bca3) || (0x1cf00 <= c && c <= 0x1cf2d)
            || (0x1cf30 <= c && c <= 0x1cf46) || (0x1d167 <= c && c <= 0x1d169)
            || (0x1d173 <= c && c <= 0x1d182) || (0x1d185 <= c && c <= 0x1d18b)
            || (0x1d1aa <= c && c <= 0x1d1ad) || (0x1d242 <= c && c <= 0x1d244)
            || (0x1da00 <= c && c <= 0x1da36) || (0x1da3b <= c && c <= 0x1da6c)
            || (0x1da75 <= c && c <= 0x1da75) || (0x1da84 <= c && c <= 0x1da84)
            || (0x1da9b <= c && c <= 0x1da9f) || (0x1daa1 <= c && c <= 0x1daaf)
            || (0x1e000 <= c && c <= 0x1e006) || (0x1e008 <= c && c <= 0x1e018)
            || (0x1e01b <= c && c <= 0x1e021) || (0x1e023 <= c && c <= 0x1e024)
            || (0x1e026 <= c && c <= 0x1e02a) || (0x1e08f <= c && c <= 0x1e08f)
            || (0x1e130 <= c && c <= 0x1e136) || (0x1e2ae <= c && c <= 0x1e2ae)
            || (0x1e2ec <= c && c <= 0x1e2ef) || (0x1e4ec <= c && c <= 0x1e4ef)
            || (0x1e8d0 <= c && c <= 0x1e8d6) || (0x1e944 <= c && c <= 0x1e94a)
            || (0xe0001 <= c && c <= 0xe0001) || (0xe0020 <= c && c <= 0xe007f)
            || (0xe0100 <= c && c <= 0xe01ef))
        return 0;
    if ((0x1100 <= c && c <= 0x115f) || (0x231a <= c && c <= 0x231b)
            || (0x2329 <= c && c <= 0x232a) || (0x23e9 <= c && c <= 0x23ec)
            || (0x23f0 <= c && c <= 0x23f0) || (0x23f3 <= c && c <= 0x23f3)
            || (0x25fd <= c && c <= 0x25fe) || (0x2614 <= c && c <= 0x2615)
            || (0x2648 <= c && c <= 0x2653) || (0x267f <= c && c <= 0x267f)
            || (0x2693 <= c && c <= 0x2693) || (0x26a1 <= c && c <= 0x26a1)
            || (0x26aa <= c && c <= 0x26ab) || (0x26bd <= c && c <= 0x26be)
            || (0x26c4 <= c && c <= 0x26c5) || (0x26ce <= c && c <= 0x26ce)
            || (0x26d4 <= c && c <= 0x26d4) || (0x26ea <= c && c <= 0x26ea)
            || (0x26f2 <= c && c <= 0x26f3) || (0x26f5 <= c && c <= 0x26f5)
            || (0x26fa <= c && c <= 0x26fa) || (0x26fd <= c && c <= 0x26fd)
            || (0x2705 <= c && c <= 0x2705) || (0x270a <= c && c <= 0x270b)
            || (0x2728 <= c && c <= 0x2728) || (0x274c <= c && c <= 0x274c)
            || (0x274e <= c && c <= 0x274e) || (0x2753 <= c && c <= 0x2755)
            || (0x2757 <= c && c <= 0x2757) || (0x2795 <= c && c <= 0x2797)
            || (0x27b0 <= c && c <= 0x27b0) || (0x27bf <= c && c <= 0x27bf)
            || (0x2b1b <= c && c <= 0x2b1c) || (0x2b50 <= c && c <= 0x2b50)
            || (0x2b55 <= c && c <= 0x2b55) || (0x2e80 <= c && c <= 0x2e99)
            || (0x2e9b <= c && c <= 0x2ef3) || (0x2f00 <= c && c <= 0x2fd5)
            || (0x2ff0 <= c && c <= 0x3029) || (0x302e <= c && c <= 0x303e)
            || (0x3041 <= c && c <= 0x3096) || (0x309b <= c && c <= 0x30ff)
            || (0x3105 <= c && c <= 0x312f) || (0x3131 <= c && c <= 0x318e)
            || (0x3190 <= c && c <= 0x31e3) || (0x31ef <= c && c <= 0x321e)
            || (0x3220 <= c && c <= 0xa48c) || (0xa490 <= c && c <= 0xa4c6)
            || (0xa960 <= c && c <= 0xa97c) || (0xac00 <= c && c <= 0xd7a3)
            || (0xf900 <= c && c <= 0xfa6d) || (0xfa70 <= c && c <= 0xfad9)
            || (0xfe10 <= c && c <= 0xfe19) || (0xfe30 <= c && c <= 0xfe52)
            || (0xfe54 <= c && c <= 0xfe66) || (0xfe68 <= c && c <= 0xfe6b)
            || (0xff01 <= c && c <= 0xff60) || (0xffe0 <= c && c <= 0xffe6)
            || (0x16fe0 <= c && c <= 0x16fe3) || (0x16ff0 <= c && c <= 0x16ff1)
            || (0x17000 <= c && c <= 0x187f7) || (0x18800 <= c && c <= 0x18cd5)
            || (0x18d00 <= c && c <= 0x18d08) || (0x1aff0 <= c && c <= 0x1aff3)
            || (0x1aff5 <= c && c <= 0x1affb) || (0x1affd <= c && c <= 0x1affe)
            || (0x1b000 <= c && c <= 0x1b122) || (0x1b132 <= c && c <= 0x1b132)
            || (0x1b150 <= c && c <= 0x1b152) || (0x1b155 <= c && c <= 0x1b155)
            || (0x1b164 <= c && c <= 0x1b167) || (0x1b170 <= c && c <= 0x1b2fb)
            || (0x1f004 <= c && c <= 0x1f004) || (0x1f0cf <= c && c <= 0x1f0cf)
            || (0x1f18e <= c && c <= 0x1f18e) || (0x1f191 <= c && c <= 0x1f19a)
            || (0x1f200 <= c && c <= 0x1f202) || (0x1f210 <= c && c <= 0x1f23b)
            || (0x1f240 <= c && c <= 0x1f248) || (0x1f250 <= c && c <= 0x1f251)
            || (0x1f260 <= c && c <= 0x1f265) || (0x1f300 <= c && c <= 0x1f320)
            || (0x1f32d <= c && c <= 0x1f335) || (0x1f337 <= c && c <= 0x1f37c)
            || (0x1f37e <= c && c <= 0x1f393) || (0x1f3a0 <= c && c <= 0x1f3ca)
            || (0x1f3cf <= c && c <= 0x1f3d3) || (0x1f3e0 <= c && c <= 0x1f3f0)
            || (0x1f3f4 <= c && c <= 0x1f3f4) || (0x1f3f8 <= c && c <= 0x1f43e)
            || (0x1f440 <= c && c <= 0x1f440) || (0x1f442 <= c && c <= 0x1f4fc)
            || (0x1f4ff <= c && c <= 0x1f53d) || (0x1f54b <= c && c <= 0x1f54e)
            || (0x1f550 <= c && c <= 0x1f567) || (0x1f57a <= c && c <= 0x1f57a)
            || (0x1f595 <= c && c <= 0x1f596) || (0x1f5a4 <= c && c <= 0x1f5a4)
            || (0x1f5fb <= c && c <= 0x1f64f) || (0x1f680 <= c && c <= 0x1f6c5)
            || (0x1f6cc <= c && c <= 0x1f6cc) || (0x1f6d0 <= c && c <= 0x1f6d2)
            || (0x1f6d5 <= c && c <= 0x1f6d7) || (0x1f6dc <= c && c <= 0x1f6df)
            || (0x1f6eb <= c && c <= 0x1f6ec) || (0x1f6f4 <= c && c <= 0x1f6fc)
            || (0x1f7e0 <= c && c <= 0x1f7eb) || (0x1f7f0 <= c && c <= 0x1f7f0)
            || (0x1f90c <= c && c <= 0x1f93a) || (0x1f93c <= c && c <= 0x1f945)
            || (0x1f947 <= c && c <= 0x1f9ff) || (0x1fa70 <= c && c <= 0x1fa7c)
            || (0x1fa80 <= c && c <= 0x1fa88) || (0x1fa90 <= c && c <= 0x1fabd)
            || (0x1fabf <= c && c <= 0x1fac5) || (0x1face <= c && c <= 0x1fadb)
            || (0x1fae0 <= c && c <= 0x1fae8) || (0x1faf0 <= c && c <= 0x1faf8)
            || (0x20000 <= c && c <= 0x2a6df) || (0x2a700 <= c && c <= 0x2b739)
            || (0x2b740 <= c && c <= 0x2b81d) || (0x2b820 <= c && c <= 0x2cea1)
            || (0x2ceb0 <= c && c <= 0x2ebe0) || (0x2ebf0 <= c && c <= 0x2ee5d)
            || (0x2f800 <= c && c <= 0x2fa1d) || (0x30000 <= c && c <= 0x3134a)
            || (0x31350 <= c && c <= 0x323af))
        return 2;
    return 1;
}

typedef struct U32Buffer {
    uint32_t *base;
    int size;
    int capacity;
} U32Buffer;

static inline void init_u32_buffer(U32Buffer *b) {
    b->size = 0;
    b->capacity = 8;
    b->base = (uint32_t *)MALLOC(b->capacity * sizeof(uint32_t));
    assert(b->base);
}

static inline void uninit_u32_buffer(U32Buffer *b) {
    b->size = 0;
    b->capacity = 0;
    FREE(b->base);
    b->base = NULL;
}

static inline void u32_buffer_reserve(U32Buffer *b, int target) {
    if (b->capacity < target) {
        b->capacity <<= 1;
        if (b->capacity < target)
            b->capacity = target;
        uint32_t *t =
                (uint32_t *)REALLOC(b->base, b->capacity * sizeof(uint32_t));
        assert(t);
        b->base = t;
    }
}

static inline void u32_buffer_push(U32Buffer *b, uint32_t c) {
    u32_buffer_reserve(b, b->size + 1);
    b->base[b->size++] = c;
}

static inline void u32_buffer_clear(U32Buffer *b) { b->size = 0; }

typedef struct {
    uint32_t len;
    uint32_t ch;
} Utf8ToU32Ret;

static inline uint32_t get_utf8_len(uint8_t c) {
    if ((c & UTF8_ASCII) == 0x00) {
        return 1;
    } else if ((c & UTF8_2_MASK) == UTF8_2) {
        return 2;
    } else if ((c & UTF8_3_MASK) == UTF8_3) {
        return 3;
    } else if ((c & UTF8_4_MASK) == UTF8_4) {
        return 4;
    } else if ((c & UTF8_5_MASK) == UTF8_5) {
        return 5;
    } else if ((c & UTF8_6_MASK) == UTF8_6) {
        return 6;
    }

    return 0;
}

static inline Utf8ToU32Ret utf8_to_u32(const uint8_t *str) {
    Utf8ToU32Ret ret = {
        .len = 0,
        .ch = 0,
    };

    if ((*str & UTF8_ASCII) == 0x00) {
        ret.len = 1;
        ret.ch = *str;
        return ret;
    } else if ((*str & UTF8_2_MASK) == UTF8_2) {
        ret.len = 2;
        ret.ch = UTF8_2_FIRST_CODE_MASK & *str;
    } else if ((*str & UTF8_3_MASK) == UTF8_3) {
        ret.len = 3;
        ret.ch = UTF8_3_FIRST_CODE_MASK & *str;
    } else if ((*str & UTF8_4_MASK) == UTF8_4) {
        ret.len = 4;
        ret.ch = UTF8_4_FIRST_CODE_MASK & *str;
    } else if ((*str & UTF8_5_MASK) == UTF8_5) {
        ret.len = 5;
        ret.ch = UTF8_5_FIRST_CODE_MASK & *str;
    } else if ((*str & UTF8_6_MASK) == UTF8_6) {
        ret.len = 6;
        ret.ch = UTF8_6_FIRST_CODE_MASK & *str;
    }
    assert(str[ret.len - 1]);
    for (uint32_t i = 1; i < ret.len; ++i) {
        uint8_t b = str[i];
        assert((b & UTF8_REST_MASK) == UTF8_REST);
        ret.ch = (ret.ch << UTF8_REST_BIT_WIDTH) | (b & UTF8_REST_CODE_MASK);
    }

    assert(ret.ch <= INT32_MAX);

    return ret;
}

static inline void write_utf8_to_u32_buffer(U32Buffer *b, const uint8_t *str) {
    while (*str) {
        Utf8ToU32Ret ch = utf8_to_u32(str);
        assert(ch.len);
        u32_buffer_push(b, ch.ch);
        str += ch.len;
    }
}

#define UTF8R UTF8_REST
#define UTF8R_M UTF8_REST_CODE_MASK
#define UTF8R_BW0 (0)
#define UTF8R_BW1 UTF8_REST_BIT_WIDTH
#define UTF8R_BW2 (UTF8R_BW1 * 2)
#define UTF8R_BW3 (UTF8R_BW1 * 3)
#define UTF8R_BW4 (UTF8R_BW1 * 4)
#define UTF8R_BW5 (UTF8R_BW1 * 5)
static inline void u32_to_fix_utf8_buffer(uint32_t cur, char *buf, size_t len) {
    if (cur <= UTF8_ASCII_MAX) {
        assert(len >= 2);
        buf[0] = (char)cur;
        buf[1] = '\0';
    } else if (cur <= UTF8_2_MAX) {
        assert(len >= 3);
        buf[0] = (char)(UTF8_2 | (cur >> UTF8R_BW1));
        buf[1] = (char)(UTF8R | ((cur >> UTF8R_BW0) & UTF8R_M));
        buf[2] = '\0';
    } else if (cur <= UTF8_3_MAX) {
        assert(len >= 4);
        buf[0] = (char)(UTF8_3 | (cur >> UTF8R_BW2));
        buf[1] = (char)(UTF8R | ((cur >> UTF8R_BW1) & UTF8R_M));
        buf[2] = (char)(UTF8R | ((cur >> UTF8R_BW0) & UTF8R_M));
        buf[3] = '\0';
    } else if (cur <= UTF8_4_MAX) {
        assert(len >= 5);
        buf[0] = (char)(UTF8_4 | (cur >> UTF8R_BW3));
        buf[1] = (char)(UTF8R | ((cur >> UTF8R_BW2) & UTF8R_M));
        buf[2] = (char)(UTF8R | ((cur >> UTF8R_BW1) & UTF8R_M));
        buf[3] = (char)(UTF8R | ((cur >> UTF8R_BW0) & UTF8R_M));
        buf[4] = '\0';
    } else if (cur <= UTF8_5_MAX) {
        assert(len >= 6);
        buf[0] = (char)(UTF8_5 | (cur >> UTF8R_BW4));
        buf[1] = (char)(UTF8R | ((cur >> UTF8R_BW3) & UTF8R_M));
        buf[2] = (char)(UTF8R | ((cur >> UTF8R_BW2) & UTF8R_M));
        buf[3] = (char)(UTF8R | ((cur >> UTF8R_BW1) & UTF8R_M));
        buf[4] = (char)(UTF8R | ((cur >> UTF8R_BW0) & UTF8R_M));
        buf[5] = '\0';
    } else {
        assert(len >= 7);
        buf[0] = (char)(UTF8_6 | (cur >> UTF8R_BW5));
        buf[1] = (char)(UTF8R | ((cur >> UTF8R_BW4) & UTF8R_M));
        buf[2] = (char)(UTF8R | ((cur >> UTF8R_BW3) & UTF8R_M));
        buf[3] = (char)(UTF8R | ((cur >> UTF8R_BW2) & UTF8R_M));
        buf[4] = (char)(UTF8R | ((cur >> UTF8R_BW1) & UTF8R_M));
        buf[5] = (char)(UTF8R | ((cur >> UTF8R_BW0) & UTF8R_M));
        buf[6] = '\0';
    }
}

static inline void
u32_to_utf8_buffer(StringBuffer *buf, const uint32_t *cur, size_t len) {
    string_buffer_clear(buf);
    const uint32_t *const end = cur + len;
    for (; cur < end; ++cur) {
        assert(*cur <= INT32_MAX);
        if (*cur <= UTF8_ASCII_MAX) {
            string_buffer_putc(buf, (char)*cur);
        } else if (*cur <= UTF8_2_MAX) {
            string_buffer_putc(buf, (UTF8_2 | (*cur >> UTF8R_BW1)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW0) & UTF8R_M)));
        } else if (*cur <= UTF8_3_MAX) {
            string_buffer_putc(buf, (UTF8_3 | (*cur >> UTF8R_BW2)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW1) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW0) & UTF8R_M)));
        } else if (*cur <= UTF8_4_MAX) {
            string_buffer_putc(buf, (UTF8_4 | (*cur >> UTF8R_BW3)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW2) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW1) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW0) & UTF8R_M)));
        } else if (*cur <= UTF8_5_MAX) {
            string_buffer_putc(buf, (UTF8_5 | (*cur >> UTF8R_BW4)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW3) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW2) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW1) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW0) & UTF8R_M)));
        } else {
            string_buffer_putc(buf, (UTF8_6 | (*cur >> UTF8R_BW5)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW4) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW3) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW2) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW1) & UTF8R_M)));
            string_buffer_putc(buf, (UTF8R | ((*cur >> UTF8R_BW0) & UTF8R_M)));
        }
    }
}
#undef UTF8R
#undef UTF8R_M
#undef UTF8R_BW0
#undef UTF8R_BW1
#undef UTF8R_BW2
#undef UTF8R_BW3
#undef UTF8R_BW4
#undef UTF8R_BW5

static inline int is_char_dev(int fd) {
    struct stat st = { 0 };
    st.st_mode = 0;
    fstat(fd, &st);
    return (st.st_mode & S_IFMT) == S_IFCHR;
}

static const char *unsupported_terms[] = {
    "dumb",
    "cons25",
    "emacs",
    NULL,
};

static inline int is_unsupported_term(void) {
    static int once = 0;
    static int res = 0;
    if (!once) {
        const char *term = getenv("TERM");
        if (term) {
            for (const char **cur_term = &unsupported_terms[0]; *cur_term;
                    ++cur_term) {
                if (!strcmp(term, *cur_term)) {
                    res = 1;
                    break;
                }
            }
        }
        once = 1;
    }
    return res;
}

static inline char *get_line(FILE *in) {
    int c;
    StringBuffer buf;
    init_string_buffer(&buf);
    while ((c = fgetc(in)) != EOF) {
        string_buffer_putc(&buf, c);
        if (c == '\n')
            break;
    }

    if (c == EOF && buf.index == 0) {
        uninit_string_buffer(&buf);
        return NULL;
    }
    return buf.buf;
}

// Convert ESC+Key to Alt-Key
static inline int readline_key_esc2alt(int ch) {
    switch (ch) {
    case KEY_DEL:
        ch = KEY_ALT_DEL;
        break;
    case KEY_HOME:
        ch = KEY_ALT_HOME;
        break;
    case KEY_END:
        ch = KEY_ALT_END;
        break;
    case KEY_UP:
        ch = KEY_ALT_UP;
        break;
    case KEY_DOWN:
        ch = KEY_ALT_DOWN;
        break;
    case KEY_LEFT:
        ch = KEY_ALT_LEFT;
        break;
    case KEY_RIGHT:
        ch = KEY_ALT_RIGHT;
        break;
    case KEY_BACKSPACE:
        ch = KEY_ALT_BACKSPACE;
        break;
    }
    return ch;
}

// Map other function keys to main key
static int readline_key_mapping(int ch) {
    switch (ch) {
#ifndef _WIN32
    case KEY_HOME2:
        ch = KEY_HOME;
        break;
    case KEY_END2:
        ch = KEY_END;
        break;
    case KEY_CTRL_UP2:
        ch = KEY_CTRL_UP;
        break;
    case KEY_CTRL_DOWN2:
        ch = KEY_CTRL_DOWN;
        break;
    case KEY_CTRL_LEFT2:
        ch = KEY_CTRL_LEFT;
        break;
    case KEY_CTRL_RIGHT2:
        ch = KEY_CTRL_RIGHT;
        break;
    case KEY_F1_2:
        ch = KEY_F1;
        break;
    case KEY_F2_2:
        ch = KEY_F2;
        break;
    case KEY_F3_2:
        ch = KEY_F3;
        break;
    case KEY_F4_2:
        ch = KEY_F4;
        break;
#endif
    case KEY_DEL2:
        ch = KEY_BACKSPACE;
        break;
    }
    return ch;
}

#ifndef _WIN32
static inline int readline_get_esckey(int ch, int fd) {
    if (ch == 0) {
        ch = _my_read(fd);
    }
    if (ch == '[') {
        ch = _my_read(fd);
        if (ch >= '0' && ch <= '6') {
            int ch2 = _my_read(fd);
            if (ch2 == '~') {
                ch = ESC_KEY4(ch, ch2);
            } else if (ch2 == ';') {
                ch2 = _my_read(fd);
                if (ch2 != '5' && ch2 != '3') {
                    return 0;
                }
                int ch3 = _my_read(fd);
                ch = ESC_KEY6(ch, ch2, ch3);
            }
        } else if (ch == '[') {
            int ch2 = _my_read(fd);
            ch = ESC_KEY4('[', ch2);
        } else {
            ch = ESC_KEY3(ch);
        }
    } else if (ch == 'O') {
        int ch2 = _my_read(fd);
        ch = ESC_OKEY(ch2);
    } else {
        ch = ALT_KEY(ch);
    }
    return ch;
}

static inline int readline_getkey(int *is_esc, int fd) {
    int ch = _my_read(fd);
    if (ch == KEY_ESC) {
        *is_esc = 1;
        ch = _my_read(fd);
        if (ch == KEY_ESC) {
            ch = readline_get_esckey(0, fd);
            ch = readline_key_mapping(ch);
            ch = readline_key_esc2alt(ch);
        } else {
            ch = readline_get_esckey(ch, fd);
        }
    }

    return ch;
}

#else

static inline int readline_getkey(int *is_esc, int fd) {
    int ch = _my_read(fd);
    int esc = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) && (ch == KEY_DEL2)) {
        ch = KEY_CTRL_BACKSPACE;
    } else if (ch == 224 || ch == 0) {
        *is_esc = 1;
        ch = _my_read(fd);
        ch = (GetKeyState(VK_MENU) & 0x8000) ? ALT_KEY(ch)
                                             : ch + (KEY_ESC << 8);
    } else if (ch == KEY_ESC) {
        *is_esc = 1;
        ch = readline_getkey(&esc, fd);
        ch = readline_key_esc2alt(ch);
    } else if ((GetKeyState(VK_MENU) & 0x8000)
               && !(GetKeyState(VK_CONTROL) & 0x8000)) {
        *is_esc = 1;
        ch = ALT_KEY(ch);
    }

    return ch;
}

#endif

static inline void readline_clear_screen_after_cursor(StringBuffer *s) {
    string_buffer_puts(s, "\033[J");
}

static inline void readline_clear_line_after_cursor(StringBuffer *s) {
    string_buffer_puts(s, "\033[K");
}

static inline void readline_cursor_move(StringBuffer *s, int row, int col) {
    if (col > 0) {
        string_buffer_printf(s, "\033[%dC", col);
    } else if (col < 0) {
        string_buffer_printf(s, "\033[%dD", -col);
    }

    if (row > 0) {
        string_buffer_printf(s, "\033[%dB", row);
    } else if (row < 0) {
        string_buffer_printf(s, "\033[%dA", -row);
    }
}

struct EditState {
    FILE *out;
    const char *prompt;
    int *ppos;

    int start_row;
    int rows;
    int cy;

    int hindex;
    int in_his;

    StringBuffer *str_buf;
    U32Buffer *buf;
};

// steal from bestline: https://github.com/jart/bestline
// XXX: 需要优化，参数refresh_pos的用途有待优化
static inline void readline_refresh(struct EditState *l,
        int new_pos,
        int new_size,
        int refresh_pos) {
    if (new_size != l->buf->size) {
        l->in_his = 0;
        l->hindex = 0;
    }

    FILE *out = l->out;
    const char *prompt = l->prompt;
    StringBuffer *str_buf = l->str_buf;
    U32Buffer *buf = l->buf;
    int *ppos = l->ppos;

    static int yn;
    static int xn;

    int pwidth = (int)strlen(prompt);

    if (refresh_pos || !yn || is_readline_win) {
        readline_get_screen(&yn, &xn, out);
    }

    int cx;
    int cy;
    int rows;
    int x;

    // x of the previous line
    int prev_x = 0;

    int end_row;

print_start:
    string_buffer_clear(str_buf);

    if (l->rows - l->cy - 1 > 0) {
        readline_cursor_move(str_buf, -(l->rows - l->cy - 1), 0);
    }

    string_buffer_puts(str_buf, "\r");
    cx = -1;
    cy = -1;
    rows = 1;

    x = pwidth;

    if (l->start_row < 1) {
        string_buffer_puts(str_buf, prompt);
    }

    if (l->start_row == -1) {
        end_row = INT32_MAX;
    } else {
        end_row = l->start_row + yn;
    }

    for (int i = 0; i < new_size; ++i) {
        uint32_t ch = buf->base[i];
        int width = cwidth(ch);

        if (ch == '\n' || (x && x + width > xn)) {

            if (rows >= end_row && i >= new_pos)
                break;

            if (cy >= 0)
                ++cy;
            if (x < xn)
                readline_clear_line_after_cursor(str_buf);

            prev_x = x;
            x = 0;

            if (rows > l->start_row && end_row > rows) {
                string_buffer_puts(str_buf, "\r\n");
            }

            ++rows;
        }

        if (i == new_pos) {
            int is_newline = ch == '\n';
            cy = is_newline;
            cx = is_newline ? prev_x : x;

            if (l->start_row > -1) {
                if (l->start_row + is_newline >= rows) {
                    l->start_row = rows - 1 - is_newline;
                    goto print_start;
                } else if (rows > end_row) {
                    end_row = rows;
                    l->start_row = end_row - yn;
                    goto print_start;
                }
            }
        }

        if (ch != '\n' && rows > l->start_row && end_row >= rows) {
            char utf8_buf[8];
            u32_to_fix_utf8_buffer(buf->base[i], utf8_buf, sizeof(utf8_buf));
            string_buffer_puts(str_buf, utf8_buf);
        }

        x += width;
    }

    if (x < xn)
        readline_clear_screen_after_cursor(str_buf);

    if (new_pos && new_pos == new_size) {
        if (rows > end_row) {
            end_row = rows;
            l->start_row = end_row - yn;
            l->cy = 0;
            goto print_start;
        } else if (x >= xn) {
            ++rows;
            if (rows > end_row) {
                end_row = rows;
                l->start_row = end_row - yn;
                l->cy = 0;
                goto print_start;
            }
            string_buffer_puts(str_buf, "\n\r");
            readline_clear_screen_after_cursor(str_buf);
        }
    }

    if (cy > 0) {
        readline_cursor_move(str_buf, -cy, 0);
    }

    if (cx > 0) {
        string_buffer_puts(str_buf, "\r");
        readline_cursor_move(str_buf, 0, cx);
    } else if (!cx) {
        string_buffer_puts(str_buf, "\r");
    }

    l->rows = rows;
    l->cy = MAX(0, cy);

    fputs(str_buf->buf, stdout);
    fflush(stdout);

    if (l->start_row == -1 && rows > yn) {
        l->start_row = rows - yn;
        goto print_start;
    }

    *ppos = new_pos;
    buf->size = new_size;
}

#define FKL_READLINE_HISTORY_PREV (+1)
#define FKL_READLINE_HISTORY_NEXT (-1)

static void readline_history_goto(struct EditState *s, unsigned idx) {
    if (history_len <= 1)
        return;
    if (idx > history_len - 1)
        return;

    U32Buffer *buf = s->buf;
    idx = MAX(MIN(idx, history_len - 1), 0);

    if (s->hindex == 0) {
        FREE(history[history_len - 1 - s->hindex]);
        u32_to_utf8_buffer(s->str_buf, buf->base, buf->size);
        history[history_len - 1 - s->hindex] = STRDUP(s->str_buf->buf);
    }

    s->hindex = idx;
    char *line = history[history_len - 1 - s->hindex];

    u32_buffer_clear(buf);
    write_utf8_to_u32_buffer(buf, (const uint8_t *)line);

    *(s->ppos) = buf->size;
    readline_refresh(s, buf->size, buf->size, 1);
    s->in_his = 1;
    s->hindex = idx;
}

static void readline_history_move(struct EditState *s, int dx) {
    readline_history_goto(s, s->hindex + dx);
}

static inline void readline_print(struct EditState *l, int pos) {
    *(l->ppos) = 0;
    readline_refresh(l, pos, l->buf->size, 1);
}

static inline int readline_move_up(const U32Buffer *buf, int pos) {
    if (pos == 0)
        return 0;
    int newline_pos = pos - 1;
    for (; newline_pos > -1 && buf->base[newline_pos] != '\n'; --newline_pos)
        ;

    if (newline_pos == -1)
        return pos;

    int cur_line_x = pos - newline_pos;

    int prev_newline_pos = newline_pos - 1;
    for (; prev_newline_pos > -1 && buf->base[prev_newline_pos] != '\n';
            --prev_newline_pos)
        ;

    int prev_line_width = newline_pos - prev_newline_pos;

    return (prev_line_width > cur_line_x) ? prev_newline_pos + cur_line_x
                                          : prev_newline_pos + prev_line_width;
}

static inline int readline_move_down(const U32Buffer *buf, int pos) {
    if (pos == buf->size - 1)
        return buf->size;

    int prev_newline_pos = pos - 1;
    for (; prev_newline_pos > -1 && buf->base[prev_newline_pos] != '\n';
            --prev_newline_pos)
        ;

    int newline_pos = pos;
    for (; newline_pos < buf->size && buf->base[newline_pos] != '\n';
            ++newline_pos)
        ;

    if (newline_pos == buf->size)
        return pos;

    int cur_line_x = pos - prev_newline_pos;

    int next_newline_pos = newline_pos + 1;
    for (; next_newline_pos < buf->size && buf->base[next_newline_pos] != '\n';
            ++next_newline_pos)
        ;

    int next_line_width = next_newline_pos - newline_pos;

    return (next_line_width > cur_line_x) ? newline_pos + cur_line_x
                                          : newline_pos + next_line_width;
}

static inline void free_history(void) {
    for (size_t i = 0; i < FKL_READLINE_MAX_HISTORY; ++i) {
        if (history[i]) {
            FREE(history[i]);
            history[i] = NULL;
        }
    }
}

static void readline_atexit(void) {
    disable_raw_mode();
    free_history();
}

static int readline_end(int r) {
    if (history_len) {
        FREE(history[--history_len]);
        history[history_len] = 0;
    }

    return r;
}

// TODO: 添加更多的快捷键
static inline char *readline_internal(const char *prompt,
        const char *init,
        FklReadlineEndPredicateCb cb,
        void *args,
        FILE *in,
        FILE *out) {
    static char once;
    enable_raw_mode(fileno(in));

    struct EditState ps = {
        .prompt = prompt,
        .start_row = -1,
        .rows = 0,
        .cy = 0,
        .hindex = 0,
        .out = out,
    };

    if (!once) {
        atexit(readline_atexit);
        once = 1;
    }

    U32Buffer buf;
    init_u32_buffer(&buf);
    if (init != NULL) {
        write_utf8_to_u32_buffer(&buf, (const uint8_t *)init);
    }

    int pos = buf.size;
    int read_end = 0;
    uint32_t utf32_ch = 0;

    ps.buf = &buf;
    ps.ppos = &pos;

    StringBuffer str_buf;
    init_string_buffer(&str_buf);
    ps.str_buf = &str_buf;

    readline_print(&ps, pos);

    fklReadlineHistoryAdd("");
    do {
        int is_esc = 0;
        int ch = readline_getkey(&is_esc, fileno(in));
        ch = readline_key_mapping(ch);

        switch (ch) {
        case KEY_TAB:
            break;

        case CTRL_KEY('P'):
        case KEY_UP:
            if (pos > 0 && !ps.in_his) {

            case KEY_CTRL_UP:
            case KEY_ALT_UP:

                int new_pos = readline_move_up(&buf, pos);
                readline_refresh(&ps, new_pos, buf.size, 0);
            } else {
                readline_history_move(&ps, FKL_READLINE_HISTORY_PREV);
            }
            break;

        case CTRL_KEY('N'):
        case KEY_DOWN:
            if (pos < buf.size && !ps.in_his) {

            case KEY_CTRL_DOWN:
            case KEY_ALT_DOWN:

                int new_pos = readline_move_down(&buf, pos);
                readline_refresh(&ps, new_pos, buf.size, 0);
            } else {
                readline_history_move(&ps, FKL_READLINE_HISTORY_NEXT);
            }
            break;

        case KEY_LEFT:
        case CTRL_KEY('B'):
            if (pos > 0) {
                readline_refresh(&ps, pos - 1, buf.size, 0);
            }
            break;
        case KEY_RIGHT:
        case CTRL_KEY('F'):
            if (pos < buf.size) {
                readline_refresh(&ps, pos + 1, buf.size, 0);
            }
            break;

        case CTRL_KEY('A'): // Move cursor to start of line.
        case KEY_HOME:
            readline_refresh(&ps, 0, buf.size, 0);
            break;

        case CTRL_KEY('E'): // Move cursor to end of line
        case KEY_END:
            readline_refresh(&ps, buf.size, buf.size, 0);
            break;

        case KEY_BACKSPACE:
            if (pos > 0) {
                memmove(&buf.base[pos - 1],
                        &buf.base[pos],
                        (buf.size - pos) * sizeof(*buf.base));
                readline_refresh(&ps, pos - 1, buf.size - 1, 1);
            }
            break;

        case KEY_DEL:
        case CTRL_KEY('D'):
            if (pos < buf.size) {
                memmove(&buf.base[pos],
                        &buf.base[pos + 1],
                        (buf.size - pos - 1) * sizeof(*buf.base));
                readline_refresh(&ps, pos, buf.size - 1, 1);
            } else if (buf.size == 0 && ch == CTRL_KEY('D')) {
                printf(" \b\n");

                read_end = readline_end(-1);
            }
            break;

        case KEY_ENTER:
        case KEY_ENTER2:
            if (cb) {
                u32_to_utf8_buffer(&str_buf, buf.base, buf.size);
                read_end = cb(str_buf.buf,
                        str_buf.index,
                        buf.base,
                        buf.size,
                        pos,
                        args);
                if (!read_end) {
                    utf32_ch = '\n';
                    goto insert_u32_ch;
                    break;
                }
            }

            readline_refresh(&ps, buf.size, buf.size, 0);
            printf(" \b\n");
            read_end = readline_end(1);
            break;

        case CTRL_KEY('G'):
        case CTRL_KEY('C'):
            readline_refresh(&ps, buf.size, buf.size, 0);
            if (ch == CTRL_KEY('C')) {
                printf(" \b^C\n");
            } else {
                printf(" \b\n");
            }
            buf.size = 0;
            pos = 0;
            errno = EAGAIN;

            read_end = readline_end(-1);
            break;
        case CTRL_KEY('Z'):
#ifndef _WIN32
            raise(SIGSTOP); // Suspend current process
            readline_print(&ps, pos);
#endif
            break;
        default:
            if (!is_esc) {
                uint8_t chs[7] = { 0 };
                chs[0] = (uint8_t)(ch & UINT8_MAX);
                uint32_t utf8_len = get_utf8_len(chs[0]);
                assert(utf8_len);
                for (uint32_t i = 1; i < utf8_len; ++i) {
                    int ch = readline_getkey(&is_esc, fileno(in));
                    ch = readline_key_mapping(ch);
                    assert(!is_esc);

                    chs[i] = (uint8_t)(ch & UINT8_MAX);
                }

                chs[utf8_len] = '\0';

                utf32_ch = utf8_to_u32(chs).ch;
                assert(utf32_ch);

            insert_u32_ch:

                u32_buffer_reserve(&buf, buf.size + 1);
                memmove(&buf.base[pos + 1],
                        &buf.base[pos],
                        (buf.size - pos) * sizeof(*buf.base));
                buf.base[pos] = utf32_ch;
                readline_refresh(&ps, pos + 1, buf.size + 1, pos + 1);
            }
            break;
        }
    } while (!read_end);

#ifndef _WIN32
    disable_raw_mode();
#endif

    if (read_end == -1) {
        uninit_u32_buffer(&buf);
        uninit_string_buffer(&str_buf);
        return NULL;
    } else {
        u32_to_utf8_buffer(&str_buf, buf.base, buf.size);
        uninit_u32_buffer(&buf);
        char *ret = STRDUP(str_buf.buf);
        uninit_string_buffer(&str_buf);
        return ret;
    }
}

char *fklReadline3(const char *prompt,
        const char *init,
        FklReadlineEndPredicateCb cb,
        void *args) {
    assert(prompt);
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        if (is_char_dev(STDIN_FILENO) && is_char_dev(STDOUT_FILENO)) {
            fputs(prompt, stdout);
            fflush(stdout);
        }
        return get_line(stdin);
    } else if (is_unsupported_term()) {
        fputs(prompt, stdout);
        fflush(stdout);
        return get_line(stdin);
    } else {
        fflush(stdout);
        return readline_internal(prompt, init, cb, args, stdin, stdout);
    }
}

char *fklReadline2(const char *prompt, const char *init) {
    return fklReadline3(prompt, init, NULL, NULL);
}

char *fklReadline(const char *prompt) { return fklReadline2(prompt, ""); }

int fklReadlineHistoryAdd(const char *line) {
    if (!FKL_READLINE_MAX_HISTORY)
        return 0;
    if (history_len && !strcmp(history[history_len - 1], line))
        return 0;
    char *line_copy = STRDUP(line);
    if (!line_copy)
        return 0;
    if (history_len == FKL_READLINE_MAX_HISTORY) {
        FREE(history[0]);
        memmove(&history[0],
                &history[1],
                (FKL_READLINE_MAX_HISTORY - 1) * sizeof(char *));
        --history_len;
    }
    history[history_len++] = line_copy;
    return 1;
}
