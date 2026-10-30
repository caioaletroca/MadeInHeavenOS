#ifndef _INIT_H_
#define _INIT_H_

/**
 * @brief Start /sbin/init as pid 1, with the command line's words after the
 * kernel's name=value ones as its arguments. Init never exits: if it does,
 * the kernel panics.
 *
 * @param arguments Init's part of the command line (kernel_args.init_args).
 */
void init_start(const char *arguments);

#endif