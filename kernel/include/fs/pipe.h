#ifndef _FS_PIPE_H_
#define _FS_PIPE_H_

struct file;

/**
 * @brief Pipe interface for the kernel.
 */
void pipe_init(void);

/**
 * @brief Creates a pipe with read and write ends.
 *
 * @param read_end Pointer to store the read end of the pipe.
 * @param write_end Pointer to store the write end of the pipe.
 * @return 0 on success, -ENOMEM on allocation failure.
 */
int pipe_create(struct file **read_end, struct file **write_end);

#endif /* _FS_PIPE_H_ */