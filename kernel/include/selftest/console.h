#ifndef _SELFTEST_CONSOLE_H
#define _SELFTEST_CONSOLE_H

/**
 * Self-test for the console read path: injects key events through the input
 * layer and reads them back with console_read. Needs the console thread,
 * the timer and IRQs enabled.
 */
void console_selftest(void);

#endif // _SELFTEST_CONSOLE_H
