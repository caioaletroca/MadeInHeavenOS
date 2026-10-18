#ifndef _DRIVER_MEM_H_
#define _DRIVER_MEM_H_

/**
 * @brief Registers /dev/null (1,3) and /dev/zero (1,5). After devfs_init.
 */
void mem_devices_init(void);

#endif // _DRIVER_MEM_H_