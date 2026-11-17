#ifndef _SPAWN_H_
#define _SPAWN_H_

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

typedef struct __posix_spawn_file_actions posix_spawn_file_actions_t;
typedef struct __posix_spawnattr posix_spawnattr_t;

/**
 * Creates a new process with the specified executable, arguments, and environment.
 *
 * @param pid Pointer to store the PID of the newly created process.
 * @param path Path to the executable.
 * @param file_actions File actions to be performed in the new process.
 * @param attr Spawn attributes for the new process.
 * @param argv Argument vector.
 * @param envp Environment vector.
 * @return 0 on success, or an error code on failure.
 */
int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *file_actions, const posix_spawnattr_t *attr, char *const argv[], char *const envp[]);

__END_DECLS

#endif /* _SPAWN_H_ */