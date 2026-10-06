#ifndef _SELFTEST_ELF_H_
#define _SELFTEST_ELF_H_

#include <boot_info.h>

/**
 * Self-test for the ELF loader: loads the `hello` boot module, then checks
 * that broken copies of it are rejected with -ENOEXEC.
 */
void elf_selftest(const boot_info_t *info);

#endif // _SELFTEST_ELF_H_
