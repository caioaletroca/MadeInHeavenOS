#ifndef _SELFTEST_KERNEL_ARGS_H
#define _SELFTEST_KERNEL_ARGS_H

/**
 * Self-test for the command line parser: lines parsed into kernel_args_t and
 * every field compared. Prints the parser's warnings for the bad lines.
 */
void kernel_args_selftest(void);

#endif // _SELFTEST_KERNEL_ARGS_H
