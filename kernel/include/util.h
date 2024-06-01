#ifndef _UTIL_H_
#define _UTIL_H_

/**
 * Round up a address val to the next alignment
 * Take for example align = 8
 * The last line pushes the address up for the next alignment, then the AND operation clears off the lower bits
 * Confirming the final address into the next alignment window/address
*/
#define ALIGN_UP(val, a) (((val) + ((a) - 1)) & ~((a) - 1))

#define ALIGN_DOWN(val, a) ((val) & ~((a) - 1))

/**
 * Calculates the first non-zero bit position starting from the left,
 * but counting from the right, minus 1.
 * Returns zero if val is zero.
 * Ex: _fnzb(0b0001110) = 3
 * 
 * @param value The value analyzed
*/
static inline unsigned int _fnzb(unsigned long value) {
    if(value == 0)
        return 0;
    return (sizeof(unsigned long) * 8) - __builtin_clzl(value) - 1;
}

#endif