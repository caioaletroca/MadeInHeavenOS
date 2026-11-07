#ifndef _SH_H_
#define _SH_H_

#include "history.h"
#include <sys/types.h>

#define CWD_MAX 1024
#define COMMAND_MAX 1024
#define MAX_ARGS 64
#define PIPELINE_MAX 8 // commands in one pipeline: a | b | ... (bash: no limit)

/**
 * Structure representing the shell state.
 */
typedef struct shell
{
    int status;        // exit status of the last command
    int running;       // flag indicating if the shell is running
    int interactive;   // stdin and stderr are terminals (POSIX): banner, prompt, history
    pid_t group;       // the shell's own process group: the terminal comes back here
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
 * One command of a pipeline.
 */
typedef struct stage
{
    char *argv[MAX_ARGS + 1]; // NULL-terminated, pointing into command_t.words
    int argc;
} stage_t;

/**
 * Structure representing a command.
 */
typedef struct command
{
    char command[COMMAND_MAX];
    stage_t stages[PIPELINE_MAX]; // in order: stages[0]'s stdout is stages[1]'s stdin
    int count;                    // stages used; 1 for a plain command
    char words[COMMAND_MAX];      // every stage's words, each '\0'-terminated; stage_t.argv points here
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
 * @param sh The shell instance.
 * @param cmd The command structure containing the command to parse.
 * @return READ_COMMAND if a command was successfully parsed, READ_NOTHING if no command was read.
 */
read_result_t command_parse(shell_t *sh, command_t *cmd);

/**
 * Runs a builtin command.
 *
 * @param sh The shell instance.
 * @param stage The stage of the pipeline to execute.
 * @return 1 if a builtin command was executed, 0 if not a builtin.
 */
int builtin_run(shell_t *sh, stage_t *stage);

/**
 * Runs a command.
 *
 * @param sh The shell instance.
 * @param cmd The command structure containing the command to execute.
 * @return 0 on success, -1 on error.
 */
int command_run(shell_t *sh, command_t *cmd);

#endif // _SH_H_