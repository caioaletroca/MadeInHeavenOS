#ifndef _SELFTEST_ELF_H_
#define _SELFTEST_ELF_H_

/**
 * Self-test for the ELF loader: loads /usr/tests/hello from the initrd, then
 * checks that broken copies of it are rejected with -ENOEXEC.
 */
void elf_selftest(void);

#endif // _SELFTEST_ELF_H_
