#ifndef _ARCH_X86_64_IO_H_
#define _ARCH_X86_64_IO_H_

#include <stdint.h>

#define out_asm(port, value) __asm__ __volatile__ ("out %%al, %%dx" : : "a" (value), "Nd" (port))

#define in_asm(port, value) __asm__ __volatile__ ("in %%dx, %%al" : "=a" (value) : "Nd" (port))

/**
 * @brief Write on byte to an output port
 * 
 * @param port Port address
 * @param value Value
 */
static inline void outb(uint16_t port, uint8_t value) {
    out_asm(port, value);
}

/**
 * @brief Read one byte from an input port
 * 
 * @param port Port address
 * @return uint8_t 
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    in_asm(port, value);
    return value;
}

/**
 * @brief Write on word to an output port
 * 
 * @param port Port address
 * @param value Value
 */
static inline void outw(uint16_t port, uint16_t value) {
    out_asm(port, value);
}

/**
 * @brief Read one word from an input port
 * 
 * @param port Port address
 * @return uint8_t 
 */
static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    in_asm(port, value);
    return value;
}

/**
 * @brief Write on long word to an output port
 * 
 * @param port Port address
 * @param value Value
 */
static inline void outl(uint16_t port, uint32_t value) {
    out_asm(port, value);
}

/**
 * @brief Read one long word from an input port
 * 
 * @param port Port address
 * @return uint8_t 
 */
static inline uint32_t inl(uint16_t port) {
    uint16_t value;
    in_asm(port, value);
    return value;
}

#endif