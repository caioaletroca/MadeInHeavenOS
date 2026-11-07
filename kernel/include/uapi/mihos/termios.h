#ifndef _UAPI_MIHOS_TERMIOS_H_
#define _UAPI_MIHOS_TERMIOS_H_

#include <stdint.h>

typedef uint32_t tcflag_t;
typedef uint8_t cc_t;

#define NCCS 19

struct termios
{
    tcflag_t c_iflag; // input: what happens to a byte coming in
    tcflag_t c_oflag; // output
    tcflag_t c_cflag; // hardware: kept for programs that set it
    tcflag_t c_lflag; // local: lines, echo, signals
    cc_t c_line;
    cc_t c_cc[NCCS]; // special characters and read's limits, by V* index
};

// c_iflag
#define IGNBRK 0x0001
#define BRKINT 0x0002
#define PARMRK 0x0008
#define ISTRIP 0x0020
#define INLCR 0x0040
#define IGNCR 0x0080
#define ICRNL 0x0100 // Enter's '\r' arrives as '\n'
#define IXON 0x0400

// c_oflag
#define OPOST 0x0001
#define ONLCR 0x0004

// c_cflag
#define CSIZE 0x0030
#define CS8 0x0030
#define CREAD 0x0080
#define PARENB 0x0100

// c_lflag
#define ISIG 0x0001   // Ctrl+C, Ctrl+\ and Ctrl+Z send signals
#define ICANON 0x0002 // input is edited and read by the line
#define ECHO 0x0008   // what is typed is shown
#define ECHOE 0x0010
#define ECHOK 0x0020
#define ECHONL 0x0040
#define IEXTEN 0x8000

// c_cc
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6 // without ICANON: the bytes a read sleeps for, 0 to never sleep
#define VSTART 8
#define VSTOP 9
#define VSUSP 10

// tcsetattr's `when`
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#endif /* _UAPI_MIHOS_TERMIOS_H_ */