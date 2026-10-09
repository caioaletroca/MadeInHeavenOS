#ifndef _SH_H_
#define _SH_H_

#include "history.h"

#define COMMAND_MAX 1024
#define MAX_ARGS 64

/**
 * Structure representing the shell state.
 */
typedef struct shell
{
    int status;  // exit status of the last command
    int running; // flag indicating if the shell is running

    history_t history; // command history
} shell_t;

/**
 * Enumeration representing the result of reading a command.
 */
typedef enum
{
    READ_COMMAND, // cmd holds a command to run
    READ_NOTHING, // empty line or an error already reported: prompt again
    READ_EOF,     // end of input: leave the shell
} read_result_t;

/**
 * Structure representing a command.
 */
typedef struct command
{
    char command[COMMAND_MAX];
    char *argv[MAX_ARGS + 1];
    int argc;
} command_t;

/**
 * Reads a command from the standard input.
 *
 * @param cmd The command structure to populate.
 * @return READ_COMMAND if a command was successfully read, READ_EOF on end of input.
 */
read_result_t command_read(command_t *cmd);

/**
 * Parses a command.
 *
 * @param cmd The command structure containing the command to parse.
 * @return READ_COMMAND if a command was successfully parsed, READ_NOTHING if no command was read.
 */
read_result_t command_parse(command_t *cmd);

/**
 * Runs a builtin command.
 *
 * @param sh The shell instance.
 * @param cmd The command structure containing the command to execute.
 * @return 1 if a builtin command was executed, 0 if not a builtin, -1 on error.
 */
int builtin_run(shell_t *sh, command_t *cmd);

/**
 * Runs a command.
 *
 * @param sh The shell instance.
 * @param cmd The command structure containing the command to execute.
 * @return 0 on success, -1 on error.
 */
int command_run(shell_t *sh, command_t *cmd);

#endif // _SH_H_