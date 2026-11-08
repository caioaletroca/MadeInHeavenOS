#ifndef _LINE_LINE_H_
#define _LINE_LINE_H_

#include <ring/ring.h>
#include <stddef.h>
#include <termios.h>

/*
 * Line editing at a terminal, for any program that reads lines from someone
 * typing: the arrows, Home, End, Delete, Backspace, Ctrl+U, and the history
 * with Up and Down. The terminal is raw only while a line is being read.
 */

#define LINE_HISTORY_MAX 32

typedef ring(char *, LINE_HISTORY_MAX) line_ring_t;

/**
 * The newest lines, numbered from 1 in the order they were added.
 */
typedef struct line_history
{
    line_ring_t lines; // copies, oldest first
    int total;         // lines ever added: the newest one's number
} line_history_t;

typedef struct line_editor
{
    int input;               // the terminal's fd for keys
    int output;              // where the prompt and the line are drawn
    struct termios settings; // as found by line_editor_init: put back after each line
    line_history_t history;
} line_editor_t;

typedef enum
{
    LINE_READ,         // Enter: the line is in the buffer
    LINE_INTERRUPTED,  // Ctrl+C: the line was dropped
    LINE_END_OF_INPUT, // Ctrl+D on an empty line, or input closed
} line_result_t;

/**
 * Prepares an editor with an empty history and saves the terminal's settings.
 *
 * @param editor The editor.
 * @param input The terminal's fd for keys.
 * @param output Where to draw.
 * @return 0, or -1 with errno set when input is no terminal (ENOTTY).
 */
int line_editor_init(line_editor_t *editor, int input, int output);

/**
 * Shows the prompt and edits a line until Enter, Ctrl+C or Ctrl+D.
 *
 * @param editor The editor.
 * @param prompt Printed first.
 * @param buffer Receives the line, '\0'-terminated, without the newline.
 * @param size The buffer's size: longer lines take no more keys.
 * @return What ended the line.
 */
line_result_t line_read(line_editor_t *editor, const char *prompt, char *buffer, size_t size);

/**
 * Adds a copy of a line. A blank line is not added and gets no number.
 *
 * @return 0, or -1 if out of memory.
 */
int line_history_add(line_history_t *history, const char *line);

/**
 * A line by its number, as history_add counted it.
 *
 * @return The line, or NULL if it never was or is no longer kept.
 */
const char *line_history_get(const line_history_t *history, int number);

#endif /* _LINE_LINE_H_ */