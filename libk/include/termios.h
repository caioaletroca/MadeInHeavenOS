#ifndef _TERMIOS_H_
#define _TERMIOS_H_

#include <sys/cdefs.h>
#include <mihos/termios.h>

__BEGIN_DECLS

/**
 * @brief Reads a terminal's settings.
 *
 * @param fd The terminal.
 * @param settings Filled with its settings.
 * @return 0, or -1 with errno set (EBADF; ENOTTY when fd is not a terminal).
 */
int tcgetattr(int fd, struct termios *settings);

/**
 * @brief Changes a terminal's settings.
 *
 * @param fd The terminal.
 * @param when TCSANOW, TCSADRAIN, or TCSAFLUSH to also drop the input waiting.
 * @param settings The new settings.
 * @return 0, or -1 with errno set (EBADF; ENOTTY; EINVAL for an unknown `when`).
 */
int tcsetattr(int fd, int when, const struct termios *settings);

/**
 * @brief Turns settings into raw mode: bytes as typed, no echo, no keys with a meaning.
 *        This only edits the struct: pass it to tcsetattr.
 *
 * @param settings The settings to change.
 */
void cfmakeraw(struct termios *settings);

__END_DECLS

#endif /* _TERMIOS_H_ */