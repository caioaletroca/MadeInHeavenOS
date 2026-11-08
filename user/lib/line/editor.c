#include <line/line.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define ESCAPE 0x1B
#define BACKSPACE 0x7F
#define CTRL(c) ((c) & 0x1F)

// What key_read returns past the bytes: a key with no character
enum
{
    KEY_UP = 256,
    KEY_DOWN,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_HOME,
    KEY_END,
    KEY_DELETE,
    KEY_OTHER, // a sequence nobody here knows
};

/**
 * The line being edited.
 */
typedef struct line
{
    line_editor_t *editor;
    char *text;    // the caller's buffer, always '\0'-terminated
    size_t size;   // the buffer's size
    size_t length; // bytes in text
    size_t cursor; // byte the cursor is on, always the start of a character
    int back;      // history lines back from the newest, 0 for the line being typed
    char *draft;   // the line being typed, while browsing history; NULL: no browsing
} line_t;

/**
 * @brief Writes bytes to the editor's output.
 *
 * @param line The line being edited.
 * @param bytes The bytes to write.
 * @param n The number of bytes to write.
 */
static void output(line_t *line, const char *bytes, size_t n)
{
    write(line->editor->output, bytes, n);
}

/**
 * @brief Reads a single byte from the editor's input.
 *
 * @param line The line being edited.
 * @return The byte read, or -1 on error.
 */
static int byte_read(line_t *line)
{
    unsigned char bytes;
    long got;

    do
        got = read(line->editor->input, &bytes, 1);
    while (got == -1 && errno == EINTR);

    return got == 1 ? bytes : -1;
}

/**
 * @brief Reads a key from the editor's input.
 *
 * @param line The line being edited.
 * @return The key read, or -1 on error.
 */
static int key_read(line_t *line)
{
    int byte = byte_read(line);
    if (byte != ESCAPE)
        return byte;

    byte = byte_read(line);
    if (byte != '[')
        return byte;

    switch (byte_read(line))
    {
    case -1:
        return -1;
    case 'A':
        return KEY_UP;
    case 'B':
        return KEY_DOWN;
    case 'C':
        return KEY_RIGHT;
    case 'D':
        return KEY_LEFT;
    case 'H':
        return KEY_HOME;
    case 'F':
        return KEY_END;
    case '3':
        return byte_read(line) == '~' ? KEY_DELETE : KEY_OTHER;
    default:
        return KEY_OTHER;
    }
}

/**
 * @brief Checks if a byte is a UTF-8 continuation byte.
 *
 * @param byte The byte to check.
 * @return Non-zero if the byte is a continuation byte, 0 otherwise.
 */
static int utf8_continuation(char byte)
{
    return ((unsigned char)byte & 0xC0) == 0x80;
}

/**
 * @brief Determines the length of a UTF-8 character from its first byte.
 *
 * @param first The first byte of the character.
 * @return The length, or 0 if no character starts with this byte.
 */
static size_t utf8_length(unsigned char first)
{
    if (first < 0x80)
        return 1;
    if ((first & 0xE0) == 0xC0)
        return 2;
    if ((first & 0xF0) == 0xE0)
        return 3;
    if ((first & 0xF8) == 0xF0)
        return 4;
    return 0;
}

/**
 * @brief Counts the number of character cells in a UTF-8 string.
 *
 * @param text The UTF-8 string.
 * @param n The number of bytes in the string.
 * @return The number of character cells.
 */
static size_t cells(const char *text, size_t n)
{
    size_t count = 0;

    for (size_t i = 0; i < n; i++)
        if (!utf8_continuation(text[i]))
            count++;

    return count;
}

/**
 * @brief Moves the cursor back by a given number of character cells.
 *
 * @param line The line being edited.
 * @param count The number of character cells to move back.
 */
static void cursor_back(line_t *line, size_t count)
{
    for (size_t i = 0; i < count; i++)
        output(line, "\b", 1);
}

/**
 * @brief Finds the position of the character before the cursor.
 *
 * @param line The line being edited.
 * @return The position of the character before the cursor.
 */
static size_t char_before(line_t *line)
{
    size_t i = line->cursor;

    do
        i--;
    while (i > 0 && utf8_continuation(line->text[i]));

    return i;
}

/**
 * @brief Finds the position of the character after the cursor.
 *
 * @param line The line being edited.
 * @return The position of the character after the cursor.
 */
static size_t char_after(line_t *line)
{
    size_t i = line->cursor;

    do
        i++;
    while (i < line->length && utf8_continuation(line->text[i]));

    return i;
}

/**
 * @brief Shows the rest of the line after the cursor, with optional blanks.
 *
 * @param line The line being edited.
 * @param blanks The number of blank spaces to add after the rest of the line.
 */
static void line_show_rest(line_t *line, size_t blanks)
{
    size_t rest = line->length - line->cursor;

    output(line, line->text + line->cursor, rest);
    for (size_t i = 0; i < blanks; i++)
        output(line, " ", 1);

    cursor_back(line, cells(line->text + line->cursor, rest) + blanks);
}

/**
 * @brief Inserts bytes into the line at the cursor position.
 *
 * @param line The line being edited.
 * @param bytes The bytes to insert.
 * @param n The number of bytes to insert.
 */
static void line_insert(line_t *line, const char *bytes, size_t n)
{
    if (line->length + n >= line->size)
        return;

    char *at = line->text + line->cursor;
    memmove(at + n, at, line->length - line->cursor + 1);
    memcpy(at, bytes, n);
    line->length += n;

    output(line, bytes, n);
    line->cursor += n;
    line_show_rest(line, 0);
}

/**
 * @brief Removes characters from the cursor to a specified position.
 *
 * @param line The line being edited.
 * @param end The position to remove characters up to.
 */
static void line_remove_to(line_t *line, size_t end)
{
    size_t removed = cells(line->text + line->cursor, end - line->cursor);

    memmove(line->text + line->cursor, line->text + end, line->length - end + 1);
    line->length -= end - line->cursor;

    line_show_rest(line, removed);
}

/**
 * @brief Moves the cursor one character to the left.
 *
 * @param line The line being edited.
 */
static void line_left(line_t *line)
{
    if (line->cursor == 0)
        return;

    line->cursor = char_before(line);
    cursor_back(line, 1);
}

/**
 * @brief Moves the cursor one character to the right.
 *
 * @param line The line being edited.
 */
static void line_right(line_t *line)
{
    if (line->cursor == line->length)
        return;

    size_t next = char_after(line);
    output(line, line->text + line->cursor, next - line->cursor);
    line->cursor = next;
}

/**
 * @brief Moves the cursor to the beginning of the line.
 *
 * @param line The line being edited.
 */
static void line_home(line_t *line)
{
    cursor_back(line, cells(line->text, line->cursor));
    line->cursor = 0;
}

/**
 * @brief Moves the cursor to the end of the line.
 *
 * @param line The line being edited.
 */
static void line_end(line_t *line)
{
    output(line, line->text + line->cursor, line->length - line->cursor);
    line->cursor = line->length;
}

/**
 * @brief Deletes the character at the cursor position.
 *
 * @param line The line being edited.
 */
static void line_delete(line_t *line)
{
    if (line->cursor < line->length)
        line_remove_to(line, char_after(line));
}

/**
 * @brief Deletes the character before the cursor position.
 *
 * @param line The line being edited.
 */
static void line_backspace(line_t *line)
{
    if (line->cursor == 0)
        return;

    line_left(line);
    line_delete(line);
}

/**
 * @brief Clears all characters to the left of the cursor.
 *
 * @param line The line being edited.
 */
static void line_clear_left(line_t *line)
{
    size_t end = line->cursor;

    line_home(line);
    line_remove_to(line, end);
}

/**
 * @brief Replaces the current line with the given text.
 *
 * @param line The line being edited.
 * @param text The new text to replace the current line with.
 */
static void line_replace(line_t *line, const char *text)
{
    size_t old_cells = cells(line->text, line->length);

    line_home(line);
    line->length = strlcpy(line->text, text, line->size);
    if (line->length >= line->size)
        line->length = line->size - 1;

    line->cursor = line->length;
    output(line, line->text, line->length);

    size_t new_cells = cells(line->text, line->length);
    // A shorter line: the old one's last cells are still on screen past the new end
    if (old_cells > new_cells)
    {
        // The cursor is at the new end already: blank those cells
        for (size_t i = 0; i < old_cells - new_cells; i++)
            output(line, " ", 1);

        // And come back to the new end
        cursor_back(line, old_cells - new_cells);
    }
}

/**
 * @brief Browses through the line history. Edits to a recalled line last until the next move.
 *
 * @param line The line being edited.
 * @param step 1 for an older line (Up), -1 for a newer one (Down).
 */
static void line_browse(line_t *line, int step)
{
    line_history_t *history = &line->editor->history;
    int back = line->back + step;

    if (line->draft == NULL || back < 0 || back > history->lines.count)
        return;

    // Leaving the line being typed: keep it, so Down can bring it back
    if (line->back == 0)
        strlcpy(line->draft, line->text, line->size);

    // Back at 0 is that kept line, anything else a history line
    line->back = back;
    line_replace(line, back == 0 ? line->draft : line_history_get(history, history->total + 1 - back));
}

/**
 * @brief Edits the current line, handling user input.
 *
 * @param line The line being edited.
 * @return The result of the line editing operation.
 */
static line_result_t line_edit(line_t *line)
{
    for (;;)
    {
        int key = key_read(line);

        switch (key)
        {
        case -1:
            return LINE_END_OF_INPUT;
        case '\r':
        case '\n':
            line_end(line);
            output(line, "\n", 1);
            return LINE_READ;
        case CTRL('C'):
            line_end(line);
            output(line, "^C\n", 3);
            return LINE_INTERRUPTED;
        case CTRL('D'):
            if (line->length == 0)
                return LINE_END_OF_INPUT;
            line_delete(line);
            break;
        case CTRL('U'):
            line_clear_left(line);
            break;
        case BACKSPACE:
        case '\b':
            line_backspace(line);
            break;
        case KEY_DELETE:
            line_delete(line);
            break;
        case KEY_LEFT:
            line_left(line);
            break;
        case KEY_RIGHT:
            line_right(line);
            break;
        case KEY_HOME:
            line_home(line);
            break;
        case KEY_END:
            line_end(line);
            break;
        case KEY_UP:
            line_browse(line, 1);
            break;
        case KEY_DOWN:
            line_browse(line, -1);
            break;
        default:
            if (key >= 0x20 && key < 0x100)
            {
                char bytes[4];
                size_t n = utf8_length(key);

                if (n == 0)
                    break;

                bytes[0] = key;
                for (size_t i = 1; i < n; i++)
                {
                    int next = byte_read(line);
                    if (next < 0)
                        return LINE_END_OF_INPUT;
                    bytes[i] = next;
                }

                line_insert(line, bytes, n);
            }
            break;
        }
    }
}

int line_editor_init(line_editor_t *editor, int input, int output)
{
    memset(editor, 0, sizeof(*editor));
    editor->input = input;
    editor->output = output;
    return tcgetattr(input, &editor->settings);
}

line_result_t line_read(line_editor_t *editor, const char *prompt, char *buffer, size_t size)
{
    if (size == 0)
        return LINE_END_OF_INPUT;

    line_t line = {
        .editor = editor,
        .text = buffer,
        .size = size,
        .draft = malloc(size)};
    buffer[0] = '\0';
    output(&line, prompt, strlen(prompt));

    struct termios raw = editor->settings;
    cfmakeraw(&raw);
    raw.c_oflag = editor->settings.c_oflag;

    tcsetattr(editor->input, TCSANOW, &raw);
    line_result_t result = line_edit(&line);
    tcsetattr(editor->input, TCSANOW, &editor->settings);

    free(line.draft);
    return result;
}