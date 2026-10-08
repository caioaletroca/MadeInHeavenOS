#ifndef _FILE_H_
#define _FILE_H_

#include <stddef.h>
#include <stdio.h>

#define F_READ (1u << 0)
#define F_WRITE (1u << 1)
#define F_EOF (1u << 2)
#define F_ERR (1u << 3)
#define F_LINEBUF (1u << 4)
#define F_NOBUF (1u << 5)

enum
{
    MODE_NONE,
    MODE_READ,
    MODE_WRITE
};

/**
 * Structure representing a file.
 */
struct _FILE
{
    int fd;
    unsigned int flags;
    int mode;

    unsigned char *buffer;
    size_t size;
    size_t pos;
    size_t len;
    int ungot;

    struct _FILE *next;

    // TODO: Add thread-safety mechanisms if needed
};

extern FILE *const __stdio_head;

/**
 * Write all n bytes of data to f's fd, looping over short writes.
 *
 * @param f Pointer to the file stream.
 * @param data Pointer to the data to write.
 * @param n Number of bytes to write.
 * @return n on success; fewer bytes on error, with F_ERR set on f.
 */
size_t __write_all(FILE *f, const unsigned char *data, size_t n);

/**
 * Writes data to the given file stream.
 *
 * @param data Pointer to the data to write.
 * @param n Number of bytes to write.
 * @param f Pointer to the file stream.
 * @return Number of bytes written, or 0 on error.
 */
size_t __fwritex(const unsigned char *data, size_t n, FILE *f);

/**
 * Flushes the buffer of the given file stream.
 *
 * @param f Pointer to the file stream.
 * @return 0 on success, or non-zero on error.
 */
int __fflush_one(FILE *f);

/**
 * Fills the buffer of the given file stream.
 *
 * @param f Pointer to the file stream.
 * @return 0 on success, or EOF on error or end of file.
 */
int __fillbuf(FILE *f);

/**
 * @brief Performs necessary cleanup for the standard I/O library before program exit.
 */
void __stdio_exit(void);

#endif /* _FILE_H_ */