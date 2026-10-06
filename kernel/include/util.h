#ifndef _UTIL_H_
#define _UTIL_H_

/**
 * Round up a address val to the next alignment
 * Take for example align = 8
 * The last line pushes the address up for the next alignment, then the AND operation clears off the lower bits
 * Confirming the final address into the next alignment window/address
 */
#define ALIGN_UP(val, a) (((val) + ((a) - 1)) & ~((a) - 1))

/**
 * Round down a address val to the previous alignment
 * Take for example align = 8
 * The AND operation clears off the lower bits, effectively rounding down to the nearest alignment.
 */
#define ALIGN_DOWN(val, a) ((val) & ~((a) - 1))

/**
 * Get the minimum of two values.
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * Get the maximum of two values.
 */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * Calculates the first non-zero bit position starting from the left,
 * but counting from the right, minus 1.
 * Returns zero if val is zero.
 * Ex: _fnzb(0b0001110) = 3
 *
 * @param value The value analyzed
 */
static inline unsigned int _fnzb(unsigned long value)
{
    if (value == 0)
        return 0;
    return (sizeof(unsigned long) * 8) - __builtin_clzl(value) - 1;
}

/**
 * Get a pointer to the struct start given a pointer to a member.
 *
 * @param member_ptr    Struct member pointer.
 * @param struct_type   Type of the structure the element is embedded in.
 * @param member_name   Name of the member within the struct.
 */
#define struct_ptr(member_ptr, struct_type, member_name) \
    ((struct_type *)((char *)(member_ptr) - offsetof(struct_type, member_name)))

#endif