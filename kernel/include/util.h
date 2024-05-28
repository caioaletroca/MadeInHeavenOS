#ifndef _UTIL_H_
#define _UTIL_H_

/**
 * Round up a address val to the next alignment
 * Take for example align = 8
 * The last line pushes the address up for the next alignment, then the AND operation clears off the lower bits
 * Confirming the final address into the next alignment window/address
*/
#define ALIGN_UP(val, align) ({ \
	const typeof((val)) _val = (val); \
	const typeof((align)) _align = (align); \
	(_val + (_align - 1)) & -_align; })

#endif