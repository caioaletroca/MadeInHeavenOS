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
#define F_TTYCHECK (1u << 6) // line-buffered only if the fd is a terminal: decided at the first write
#define F_STATIC (1u << 7)   // stdin, stdout, stderr: not from malloc, never freed

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

// The first open stream; each links the next. fopen pushes on the front, fclose unlinks.
extern FILE *__stdio_head;

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
 * Makes the fd's offset the stream's position and empties the buffer: pending output is
 * written; input read ahead (and an ungetc byte) is given back with an lseek. On a pipe or
 * terminal that lseek fails and those bytes are dropped (they cannot go back).
 *
 * @param f Pointer to the file stream.
 * @return 0, or EOF if pending output could not be written.
 */
int __stream_sync(FILE *f);

/**
 * Turns an fopen mode ("r", "w", "a", each with an optional '+'; 'b' and 'x' allowed after)
 * into the stream's F_READ/F_WRITE flags and open()'s flags.
 *
 * @param mode The mode string.
 * @param open_flags Receives the flags for open().
 * @return F_READ, F_WRITE or both, or 0 for a mode that does not start with r, w or a.
 */
unsigned int __mode_flags(const char *mode, int *open_flags);

/**
 * A new stream on fd, with its own BUFSIZ buffer, pushed on the front of the list.
 * Fully buffered, unless its first write finds a terminal (then line-buffered, as C asks).
 *
 * @param fd An open file descriptor.
 * @param flags F_READ, F_WRITE or both.
 * @return The stream, or NULL with errno ENOMEM.
 */
FILE *__stream_new(int fd, unsigned int flags);

/**
 * @brief Performs necessary cleanup for the standard I/O library before program exit.
 */
void __stdio_exit(void);

#endif /* _FILE_H_ */