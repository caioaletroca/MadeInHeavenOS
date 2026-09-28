#ifndef _STDINT_H_
#define _STDINT_H_

/**
 * @name Exact-width integer types.
 * The typedef name int N _t designates a signed integer type with width N, no
 * padding bits, and a two's-complement representation.
 * The typedef name uint N _t designates an unsigned integer type with width N.
 * @{
 */

typedef __INT8_TYPE__           int8_t;
typedef __INT16_TYPE__          int16_t;
typedef __INT32_TYPE__          int32_t;
typedef __INT64_TYPE__          int64_t;

typedef __UINT8_TYPE__          uint8_t;
typedef __UINT16_TYPE__         uint16_t;
typedef __UINT32_TYPE__         uint32_t;
typedef __UINT64_TYPE__         uint64_t;

#define INT8_MAX                __INT8_MAX__
#define INT8_MIN                (-INT8_MAX - 1)
#define INT16_MAX               __INT16_MAX__
#define INT16_MIN               (-INT16_MAX - 1)
#define INT32_MAX               __INT32_MAX__
#define INT32_MIN               (-INT32_MAX - 1)
#define INT64_MAX               __INT64_MAX__
#define INT64_MIN               (-INT64_MAX - 1)

#define UINT8_MAX               __UINT8_MAX__
#define UINT16_MAX              __UINT16_MAX__
#define UINT32_MAX              __UINT32_MAX__
#define UINT64_MAX              __UINT64_MAX__

/** @} */

/**
 * @name Minimum-width integer types.
 * The typedef name int_least N _t designates a signed integer type with a
 * width of at least N, such that no signed integer type with lesser size has
 * at least the specified width. Thus, int_least32_t denotes a signed integer
 * type with a width of at least 32 bits.
 * @{
 */

typedef __INT_LEAST8_TYPE__     int_least8_t;
typedef __INT_LEAST16_TYPE__    int_least16_t;
typedef __INT_LEAST32_TYPE__    int_least32_t;
typedef __INT_LEAST64_TYPE__    int_least64_t;

typedef __UINT_LEAST8_TYPE__    uint_least8_t;
typedef __UINT_LEAST16_TYPE__   uint_least16_t;
typedef __UINT_LEAST32_TYPE__   uint_least32_t;
typedef __UINT_LEAST64_TYPE__   uint_least64_t;

#define INT_LEAST8_MAX          __INT_LEAST8_MAX__
#define INT_LEAST8_MIN          (-INT_LEAST8_MAX - 1)
#define INT_LEAST16_MAX         __INT_LEAST16_MAX__
#define INT_LEAST16_MIN         (-INT_LEAST16_MAX - 1)
#define INT_LEAST32_MAX         __INT_LEAST32_MAX__
#define INT_LEAST32_MIN         (-INT_LEAST32_MAX - 1)
#define INT_LEAST64_MAX         __INT_LEAST64_MAX__
#define INT_LEAST64_MIN         (-INT_LEAST64_MAX - 1)

#define UINT_LEAST8_MAX         __UINT_LEAST8_MAX__
#define UINT_LEAST16_MAX        __UINT_LEAST16_MAX__
#define UINT_LEAST32_MAX        __UINT_LEAST32_MAX__
#define UINT_LEAST64_MAX        __UINT_LEAST64_MAX__

/** @} */

/**
 * @name Fastest minimum-width integer types.
 * Each of the following types designates an integer type that is usually
 * fastest to operate with among all integer types that have at least the
 * specified width.
 * The designated type is not guaranteed to be fastest for all purposes; if the
 * implementation has no clear grounds for choosing one type over another, it
 * will simply pick some integer type satisfying the signedness and width
 * requirements.
 * The typedef name int_fast N _t designates the fastest signed integer type
 * with a width of at least N. The typedef name uint_fast N _t designates the
 * fastest unsigned integer type with a width of at least N.
 * @{
 */

typedef __INT_FAST8_TYPE__      int_fast8_t;
typedef __INT_FAST16_TYPE__     int_fast16_t;
typedef __INT_FAST32_TYPE__     int_fast32_t;
typedef __INT_FAST64_TYPE__     int_fast64_t;

typedef __UINT_FAST8_TYPE__     uint_fast8_t;
typedef __UINT_FAST16_TYPE__    uint_fast16_t;
typedef __UINT_FAST32_TYPE__    uint_fast32_t;
typedef __UINT_FAST64_TYPE__    uint_fast64_t;

#define INT_FAST8_MAX           __INT_FAST8_MAX__
#define INT_FAST8_MIN           (-INT_FAST8_MAX - 1)
#define INT_FAST16_MAX          __INT_FAST16_MAX__
#define INT_FAST16_MIN          (-INT_FAST16_MAX - 1)
#define INT_FAST32_MAX          __INT_FAST32_MAX__
#define INT_FAST32_MIN          (-INT_FAST32_MAX - 1)
#define INT_FAST64_MAX          __INT_FAST64_MAX__
#define INT_FAST64_MIN          (-INT_FAST64_MAX - 1)

#define UINT_FAST8_MAX          __UINT_FAST8_MAX__
#define UINT_FAST16_MAX         __UINT_FAST16_MAX__
#define UINT_FAST32_MAX         __UINT_FAST32_MAX__
#define UINT_FAST64_MAX         __UINT_FAST64_MAX__

/** @} */

/**
 * @name Integer types capable of holding object pointers.
 * The following type designates a signed integer type with the property that
 * any valid pointer to void can be converted to this type, then converted back
 * to a pointer to void, and the result will compare equal to the original
 * pointer: intptr_t.
 * The following type designates an unsigned integer type with the property that
 * any valid pointer to void can be converted to this type, then converted back
 * to a pointer to void, and the result will compare equal to the original
 * pointer: uintptr_t.
 * @{
 */

typedef __INTPTR_TYPE__         intptr_t;
typedef __UINTPTR_TYPE__        uintptr_t;

#define INTPTR_MAX              __INTPTR_MAX__
#define INTPTR_MIN              (-INTPTR_MAX - 1)
#define UINTPTR_MAX             __UINTPTR_MAX__

/** @} */

/**
 * @name Greatest-width integer types
 * The following type designates a signed integer type capable of representing
 * any value of any signed integer type: intmax_t.
 * The following type designates an unsigned integer type capable of
 * representing any value of any unsigned integer type: uintmax_t.
 * @{
 */

typedef __INTMAX_TYPE__         intmax_t;
typedef __UINTMAX_TYPE__        uintmax_t;

#define INTMAX_MAX              __INTMAX_MAX__
#define INTMAX_MIN              (-INTMAX_MAX - 1)
#define UINTMAX_MAX             __UINTMAX_MAX__

/** @} */

/**
 * @name Limits of other integer types
 * @{
 */

#define PTRDIFF_MAX             __PTRDIFF_MAX__
#define PTRDIFF_MIN             (-PTRDIFF_MAX - 1)

#define SIZE_MAX                __SIZE_MAX__

/** @} */

/**
 * @name Macros for integer constants
 * @{
 */

#define INT8_C(c)               c
#define INT16_C(c)              c
#define INT32_C(c)              c
#define INT64_C(c)              __INT64_C(c)

#define UINT8_C(c)              c
#define UINT16_C(c)             c
#define UINT32_C(c)             __UINT32_C(c)
#define UINT64_C(c)             __UINT64_C(c)

#define INTMAX_C(c)             __INTMAX_C(c)
#define UINTMAX_C(c)            __UINTMAX_C(c)

/** @} */

#endif
