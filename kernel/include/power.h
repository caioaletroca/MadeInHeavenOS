#ifndef _POWER_H_
#define _POWER_H_

#include <sys/list.h>

/**
 * Something with a last word before the power goes off (Linux's reboot notifiers):
 * embedded in its owner, which `run` finds with list_container on `link`.
 */
typedef struct halt_hook
{
    void (*run)(struct halt_hook *hook); // before the block cache is written: what it writes goes out too
    list_t link;                         // in the hooks' list
} halt_hook_t;

/**
 * @brief Run a hook when the system halts. Hooks run newest first; they are never
 * removed. Boot and mount time only (no lock).
 *
 * @param hook The hook, with run set; it must stay valid until the halt.
 */
void halt_hook_add(halt_hook_t *hook);

/**
 * Halts the system.
 * This function does not return.
 */
__attribute__((noreturn)) void system_halt(void);

#endif /* _POWER_H_ */