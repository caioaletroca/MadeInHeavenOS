#ifndef _STRING_H_
#define _STRING_H_ 1

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * @brief Copy memory area.
 *
 * @param dest Destination memory area.
 * @param src Source memory area.
 * @param n Number of bytes to copy.
 * @return Pointer to the destination memory area.
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * @brief Compare memory areas.
 *
 * @param str1 First memory area.
 * @param str2 Second memory area.
 * @param n Number of bytes to compare.
 * @return An integer less than, equal to, or greater than zero if the first n bytes of str1 is found, respectively, to be less than, to match, or be greater than the first n bytes of str2.
 */
int memcmp(const void *str1, const void *str2, size_t n);

/**
 * @brief Move memory area.
 *
 * @param str1 Destination memory area.
 * @param str2 Source memory area.
 * @param n Number of bytes to move.
 * @return Pointer to the destination memory area.
 */
void *memmove(void *str1, const void *str2, size_t n);

/**
 * @brief Fill memory area with a constant byte.
 *
 * @param str Memory area to fill.
 * @param c Constant byte to fill with.
 * @param n Number of bytes to fill.
 * @return Pointer to the memory area.
 */
void *memset(void *str, int c, size_t n);

/**
 * @brief Compare two strings.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @return An integer less than, equal to, or greater than zero if s1 is found, respectively, to be less than, to match, or be greater than s2.
 */
int strcmp(const char *s1, const char *s2);

/**
 * @brief Compare two strings up to a specified number of characters.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @param n Maximum number of characters to compare.
 * @return An integer less than, equal to, or greater than zero if the first n characters of s1 is found, respectively, to be less than, to match, or be greater than the first n characters of s2.
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * @brief Copy string.
 *
 * @param dest Destination string.
 * @param src Source string.
 * @return Pointer to the destination string.
 */
char *strcpy(char *dest, const char *src);

/**
 * @brief Copy string with size limit.
 *
 * @param dest Destination string.
 * @param src Source string.
 * @param size Maximum number of bytes to copy, including the null terminator.
 * @return Total length of the source string.
 */
size_t strlcpy(char *dest, const char *src, size_t size);

/**
 * @brief Get the length of a string.
 *
 * @param str Input string.
 * @return Number of characters in the string, excluding the null terminator.
 */
size_t strlen(const char *);

/**
 * @brief Get the length of the initial segment of a string consisting entirely of characters in another string.
 *
 * @param s Input string.
 * @param accept String containing the characters to match.
 * @return Number of characters in the initial segment of s consisting entirely of characters in accept.
 */
size_t strspn(const char *s, const char *accept);

/**
 * @brief Get the length of the initial segment of a string consisting entirely of characters not in another string.
 *
 * @param s Input string.
 * @param reject String containing the characters to avoid.
 * @return Number of characters in the initial segment of s consisting entirely of characters not in reject.
 */
size_t strcspn(const char *s, const char *reject);

/**
 * @brief Search a string for any of a set of characters.
 *
 * @param s Input string.
 * @param accept String containing the characters to search for.
 * @return Pointer to the first occurrence of any character from accept in s, or NULL if none are found.
 */
char *strpbrk(const char *s, const char *accept);

/**
 * @brief Search a string for the first occurrence of a character.
 *
 * @param s Input string.
 * @param c Character to search for.
 * @return Pointer to the first occurrence of c in s, or NULL if not found.
 */
char *strchr(const char *s, int c);

/**
 * @brief Tokenize a string using a set of delimiters, reentrant version.
 *
 * @param str Input string to tokenize. If NULL, continue tokenizing the previous string.
 * @param delim String containing delimiter characters.
 * @param saveptr Pointer to a char* variable that stores the context between successive calls.
 * @return Pointer to the next token, or NULL if no more tokens are found.
 */
char *strtok_r(char *str, const char *delim, char **saveptr);

/**
 * @brief Tokenize a string using a set of delimiters.
 *
 * @param str Input string to tokenize. If NULL, continue tokenizing the previous string.
 * @param delim String containing delimiter characters.
 * @return Pointer to the next token, or NULL if no more tokens are found.
 */
char *strtok(char *str, const char *delim);

/**
 * @brief Duplicate a string.
 *
 * @param s Input string to duplicate.
 * @return Pointer to the newly allocated duplicate string, or NULL if allocation fails.
 */
char *strdup(const char *s);

__END_DECLS

#endif