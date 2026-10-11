#ifndef _SELFTEST_INITRD_H_
#define _SELFTEST_INITRD_H_

/**
 * Self-test for the initrd: programs are found by absolute path and start with
 * an ELF header; directories, relative paths, partial names and old .elf names
 * are not found.
 */
void initrd_selftest(void);

#endif // _SELFTEST_INITRD_H_
