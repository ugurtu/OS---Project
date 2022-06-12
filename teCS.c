/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>

/*** defines ***/

#define TECS_TAB_STOP 8 //length of a tab. 8 bytes
#define TECS_QUIT_TIMES 1
#define RED "\033[0;31m"
#define reset "\033[0m"
#define TECS_VERSION RED"  Final"reset
#define CTRL_KEY(k) ((k) & 0x1f)//allows to quit the program with a ctrl-key macro
#define reverse "\x1b[7m";
enum keys {
    BACKSPACE = 127, //ascii value for delete
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*** data ***/

/**
 * erow stores line of text as a pointer to the dynamically-allocated char data and a length.
 */
typedef struct erow {
    int size;
    int rsize; //size of the contents of render
    char *chars;
    char *render; //contains the characters to draw on the screen for that row of text
} erow;

/**
 *
 */
struct editorConfig {
    int cx, cy; //The cursors x and y position
    int rx; //horizontal coordinate variable for render field
    int rowoff; //keeps track of what row the user is currently scrolled to
    int coloff; //keeps track of what column the user is currently scrolled to
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;
};
struct editorConfig E;//stores the terminal attributes

void editorSetStatusMessage(const char *fmt, ...);

void refreshScreen();

char *inputFileName(char *prompt, void (*callback)(char *, int));

char *concat(const char *s1, const char *s2);

/*** terminal ***/
/**
 * A exit method for the program.
 * @param
 */
void quit(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); //the following two escape sequences clear the screen when we exit the program
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s); //prints a descriptive error message
    exit(1); // 1 because this indicates failure
}

/**
 * This method returns the terminal back to its normal state. That means restoring the terminals original
 * attributes when we exit the program. We store the original terminal attributes in orig_termios.
 */
void deactivateUnprocessedMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        quit("tcsetattr");
}

/**
 * This method activates the raw mode. That means that it turn's the ECHO feature off.
 * Acts the same way as if you are typing a password in the terminal.
 * Terminal attributes will be red into the struct by the tcgetaatr() method. After modifying them
 * they will by applied to the terminal by the tcsetattr() method. The TCSAFLUSH argument waits for
 * all pending output to be written to the terminal, and also discards any input that hasn't been red.
 */
void activateUnprocessedMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) quit("tcgetattr");

    atexit(deactivateUnprocessedMode);

    struct termios raw_input = E.orig_termios;
    raw_input.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); //turning off more flags like underneath
    raw_input.c_oflag &= ~(OPOST); //turns off all output processing features
    raw_input.c_cflag |= (CS8); //Bitwise OR assignment
    raw_input.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); //disables Ctrl-(C,Z,S,Q,V)
    raw_input.c_cc[VMIN] = 0; //read() returns as soon as there is any input to read
    raw_input.c_cc[VTIME] = 1; //100 milliseconds wait before read() returns

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_input) == -1) quit("tcsetattr");

}

/**
 * This function waits for one keypress and returns it.
 * @return the keypress
 */
int readKeypress() {
    int nread;
    char c;
    /** reads one byte at a time
     * It can take two status one is 0 which represents the end of file
     * or it can be 1 if an errors occurs.
     *
     * */
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            quit("read");
        };
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DEL_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

/**
 *  This function gets the cursor position. It's essentially a fallback function in case
 *  ioctl() doesnt work.
 * @param rows
 * @param cols
 * @return 0 if error
 */
int returnPos(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1; //ask for the cursor position
    }
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break; //we read the response into a buffer
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1; //This ensures that we don't print the escape character
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1; // 3rd char, two int will be parsed to get rows and columns

    return 0;
}

/**
 * This function returns the size of the window(terminal).
 * @param rows value of number rows
 * @param cols value of number columns
 * @return Position of cursor
 */
int windowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; //moves the cursor to the right bottom
        return returnPos(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** row operations ***/
/**
 * This function converts chars index into a render index
 * @param row
 * @param cx
 * @return
 */
int cXToRx(erow *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) { //loop through all the characters left of cx
        if (row->chars[j] == '\t')
            rx += (TECS_TAB_STOP - 1) - (rx %
                                         TECS_TAB_STOP); //subtract the amount of columns we are right of last tab stop from amount of columns to the left of next tab stop.
        rx++; // this gets us to the next tab stop
    }
    return rx;
}

/**
 * This function converts render index into a char index
 * @param row
 * @param rx
 * @return
 */
int rXToCx(erow *row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++) { //loop through the chars
        if (row->chars[cx] == '\t')
            cur_rx += (TECS_TAB_STOP - 1) - (cur_rx % TECS_TAB_STOP); //calculate current cx value
        cur_rx++;
        if (cur_rx > rx) return cx; //if current rx hits given rx value return cx
    }
    return cx; //if rx is out of range
}

/**
 * This function uses the chars string of an erow to fill the contents of the rendered string.
 * @param row
 */
void updateRow(erow *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++; //we loop through the chars of the row
    free(row->render);
    row->render = malloc(row->size + tabs * (TECS_TAB_STOP - 1) + 1); //allocate memory for render(count of tabs)
    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') { //checks whether the current character is a tab
            row->render[idx++] = ' '; // if it is, we append one space
            while (idx % TECS_TAB_STOP != 0)
                row->render[idx++] = ' '; //append spaces until we get a tab stop, which is a column divisible by 8
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx; //contains the number of characters of row -> render
}

/**
 * This function allocates space for a new erow, and then copies the given string
 * to a new erow at the end of the E.row array. It inserts a row at the index specified by the new argument.
 * @param at
 * @param s
 * @param len
 */
void insertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return; //validate at
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1)); //allocate memory for one more erow
    memmove(&E.row[at + 1], &E.row[at],
            sizeof(erow) * (E.numrows - at)); //make room at the specified index for the new row

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    updateRow(&E.row[at]);

    E.numrows++;
    E.dirty++;
}

/**
 * This function frees the memory owned by erow.
 * @param row
 */
void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
}

/**
 * This function deletes the erow.
 * @param at
 */
void deleteRow(int at) {
    if (at < 0 || at >= E.numrows) return; //validate the at index
    editorFreeRow(&E.row[at]); //free memory owned by the row
    memmove(&E.row[at], &E.row[at + 1],
            sizeof(erow) * (E.numrows - at - 1)); //overwrite the deleted row struct with rest of rowes which come after
    E.numrows--;
    E.dirty++;
}

/**
 *
 * @param row the erow we insert the character into
 * @param at the index we want to insert characters into
 * @param c position in the array
 */
void insertCharInRow(erow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size +
                                     2); //allocate one more byte for the chars of erow. + 2 because making room for null byte.
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1); //memmove makes room for the new character
    row->size++;
    row->chars[at] = c; //assign the character to its position in the chars array
    updateRow(row); //Update fields with the new row content
    E.dirty++;
}

/**
 * This function appends a string to the end of the row.
 * @param row
 * @param s
 * @param len
 */
void appendString(erow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1); //allocate specified memory for row
    memcpy(&row->chars[row->size], s, len); //copy the given string to the end of the contents
    row->size += len; //update length
    row->chars[row->size] = '\0';
    updateRow(row);
    E.dirty++;
}

/**
 * This function deletes a character in a row.
 * @param row
 * @param at
 */
void rowDeleteChar(erow *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1],
            row->size - at); //overwrites the deleted character with the characters that come after it
    row->size--; //decrement the row size
    updateRow(row); //updates the rows
    E.dirty++;
}

/**
 * This function deletes the character left to the cursor.
 */
void deleteChar() {
    if (E.cy == E.numrows) return; //if the cursor is past the end of file, nothing to delete.
    if (E.cx == 0 && E.cy == 0) return; //if cursor is at the beginning of the first line, nothing to do.
    erow *row = &E.row[E.cy]; //gets the erow the cursor is on
    if (E.cx > 0) { //if there is a character to the left of the cursor
        rowDeleteChar(row, E.cx - 1); //delete the character and move the cursor one to the left
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size; //set E.cx to the end of the contents of the previous row before appending
        appendString(&E.row[E.cy - 1], row->chars, row->size); //append to the previous row
        deleteRow(E.cy); //delete the row
        E.cy--;
    }
}

/*** editor operations ***/
/**
 * This function takes a character and uses editorRow() to insert that character
 * into the position that the cursor is at.
 * @param c position in the array
 */
void insertChar(int c) {
    if (E.cy == E.numrows) { //if condition is true, then we append a new row to the file before inserting character
        insertRow(E.numrows, "", 0);
    }
    insertCharInRow(&E.row[E.cy], E.cx, c);
    E.cx++; //moving the cursor forward, so that the next character we insert comes after the just inserted character
}

/**
 * This function allows us to add a new line or break an existing line. This is done using the Enter key.
 */
void insertNewline() {
    if (E.cx == 0) { //If we are at the beginning of a line
        insertRow(E.cy, "", 0); //insert a new blank row
    } else {
        erow *row = &E.row[E.cy];
        insertRow(E.cy + 1, &row->chars[E.cx],
                  row->size - E.cx); //pass the characters on current row which are right of the cursor
        row = &E.row[E.cy]; //reassign the row pointer
        row->size = E.cx; //cut off current rows content by setting size to the position of the cursor
        row->chars[row->size] = '\0'; //signifies end of line
        updateRow(row);
    }
    E.cy++;
    E.cx = 0; //move cursor to beginning of row
}

/*** append buffer ***/

struct aBuffer {
    char *b; //pointer to our buffer in memory
    int len;
};

#define ABUF_INIT {NULL, 0} //acts as a constructor

/**
 * This function appends the string to an abuffer buffer
 * @param ab append buffer
 * @param s string
 * @param len of string
 */
void abAppend(struct aBuffer *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len +
                               len); //alloc enough memory for the string. Current size of string + size of appending string
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len); //copies the string after the end of current data in buffer
    ab->b = new;
    ab->len += len;
}

/**
 * This function deallocates the dynamic memory.
 * @param ab append buffer
 */
void aBufferFree(struct aBuffer *ab) {
    free(ab->b);
}

/*** output ***/
/**
 * This function checks if the users cursor has moved outside of the visible window
 * and adjusts E.rowoff so that the cursor is just inside the visible window.
 */

void scroll() {
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = cXToRx(&E.row[E.cy], E.cx);
    }
    if (E.cy < E.rowoff) { // checks if the cursor is above the visible window.
        E.rowoff = E.cy;  // scrolls up to where the cursor is.
    }
    if (E.cy >= E.rowoff + E.screenrows) { //checks if the cursor is past the bottom of the visible window.
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff) { // checks if the cursor is outside the visible window.
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols) { //checks if the cursor is outside the visible window.
        E.coloff = E.rx - E.screencols + 1;
    }
}

/**
 * This function gives us useful information about the file
 * @param ab
 */
void setStatusBar(struct aBuffer *ab) {
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                       E.filename ? E.filename : "[No Name]", E.numrows,
                       E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
                        E.cy + 1, E.numrows);
    if (len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}


/**
 * This function draws a message bar and then displays that in that bar
 * @param ab
 */
void drawStatusBar(struct aBuffer *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;

    abAppend(ab, E.statusmsg, msglen);

}

/**
 * This functions draws a tilde at every row like vim.
 */
void drawTildes(struct aBuffer *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                                          "\x1b[7m teCS -- version %s", TECS_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, " ", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, " ", 1);
            }
        } else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len);
        }
        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);

    }
}

/**
 * This function renders the interface.
 */
void refreshScreen() {
    scroll();
    struct aBuffer ab = ABUF_INIT; //init buffer

    abAppend(&ab, "\x1b[?25l", 6); //hide cursor
    abAppend(&ab, "\x1b[H", 3);

    drawTildes(&ab);
    setStatusBar(&ab);
    drawStatusBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
             (E.rx - E.coloff) +
             1); //To position the cursor on the screen, we have to subtract E.rowoff from E.cy. Same with E.rx. vor horizontal scrolling.

    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); //shows cursor l hides cursor

    write(STDOUT_FILENO, ab.b, ab.len); //whole screen updates at once
    aBufferFree(&ab);
}

/**
 * This function takes a format string and number of arguments
 * @param fmt
 * @param ... allows to take any number of arguments
 */
void setStatusMessage(const char *fmt, ...) {

    setlocale(LC_ALL, "en_US.utf8");
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap); //status message with number of seconds
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*** input ***/
/**
 * This function prompts the user to input a filename when saving a new file displayed in the status bar.
 * @param prompt
 * @return
 */
char *inputFileName(char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize); //input is stored in buf which is dynamically allocated
    size_t buflen = 0;
    buf[0] = '\0';
    while (1) { //infinite loop
        setStatusMessage(prompt, buf); //sets status message
        refreshScreen(); //refresh screen
        int c = readKeypress(); //waits for keypress
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) buf[--buflen] = '\0';
        } else if (c == '\x1b') { //if escape is pressed
            setStatusMessage("");
            if (callback) callback(buf, c);
            free(buf); //free the buf
            return NULL;
        } else if (c == '\r') { //when enter pressed
            if (buflen != 0) { //not empty
                setStatusMessage(""); //status message cleared
                if (callback) callback(buf, c); //if we dont want to use callpack we can just pass null
                return buf; //input returned
            }
        } else if (!iscntrl(c) && c <
                                  128) { //if printable char is entered and also test if char has value less than 128 to check if special char
            if (buflen == bufsize - 1) { //if buflen reached maximum capacity
                buf = realloc(buf, bufsize); //allocate the amount of memory before appending to buf
            }
            buf[buflen++] = c; //append to buf
            buf[buflen] = '\0'; //signify end
        }
        if (callback) callback(buf, c);
    }
}

/**
 * Handles the the cursor position
 * @param key pressed
 */
void moveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy]; //checks if the cursor is on actual line

    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
            } else if (E.cy > 0) { // allows to press <- at beginning of new line to come to previous line's end.
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx ==
                              row->size) { //opposite of <-. This allows user to press -> at the end of a line and appear then at the beginning of next line
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows) {
                E.cy++;
            }
            break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) { //if cx is to the right of the end of that line
        E.cx = rowlen; //set cx as end of that line
    }
}

/**
 * This function converts our array of erow structs into a single string, so that its ready
 * to be written out to a file.
 * @param buflen saves the lenght of text
 * @return buf
 */
char *rowToString(int *buflen) {
    int totlen = 0;
    int j;
    for (j = 0; j < E.numrows; j++)
        totlen += E.row[j].size +
                  1; //adds up the lengths of each row of text and adding 1 to each one for the newline character we’ll add to the end of each line
    *buflen = totlen;
    char *buf = malloc(totlen); //allocating required memory
    char *p = buf;
    for (j = 0; j < E.numrows; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size); //copy the contents of each row to the end of the buffer
        p += E.row[j].size;
        *p = '\n'; //appending a new line character after each row
        p++;
    }
    return buf;
}

/**
 * This method opens and reads a file from the disk. It takes the filename and opens the file.
 * @param filename file which will be opened and read.
 */
void readFile(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);
    setlocale(LC_ALL, "de-CH.utf8");
    FILE *fp = fopen(filename, "r"); //opens the file
    if (!fp) quit("fopen");
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) { //allows to read an entire file into E.row
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        insertRow(E.numrows, line, linelen);
    }
    free(line); //freeing from allocation
    fclose(fp);
    E.dirty = 0;
}

/**
 * This function writes the string returned by rowToString() to disk.
 */
void saveFile() {
    if (E.filename == NULL) {
        E.filename = inputFileName("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL) {
            setStatusMessage("Save aborted");
            return;
        }
    }

    int len;
    char *buf = rowToString(&len);
    int fd = open(E.filename, O_RDWR | O_CREAT,
                  0644); //create a file, open it for reading and writing. 0644 is standard permission for text files.
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) { //if no error, then ftruncate sets file size to specified length.
            if (write(fd, buf, len) == len) {
                close(fd); //close the file
                free(buf); //free the memory that buf points to
                E.dirty = 0;
                setStatusMessage("%d bytes written to disk", len); //notifies user if save succeeded
                return; //write
            }
        }
        close(fd);
    }
    free(buf);
    setStatusMessage("Can't save! I/O error: %s", strerror(errno)); //notifies user if save didnt succeed.
}

/*** find ***/
/**
 * This function is a callback function for our search function searchWord().
 */
void searchCallback(char *query, int key) {
    static int last_match = -1; //last match of search
    static int direction = 1; // stores direction of search

    if (key == '\r' || key == '\x1b') { //checks whether we pressed Enter or Escape
        last_match = -1;
        direction = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }
    if (last_match == -1) direction = 1;
    int current = last_match; //index of the current row we are searching
    int i;
    for (i = 0; i < E.numrows; i++) {
        current += direction;
        if (current == -1)
            current = E.numrows -
                      1; //causes current to go from the end of the file back to the beginning of the file or vice versa.
        else if (current == E.numrows) current = 0;
        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if (match) {
            last_match = current; //when we find a match, we set last_match to current
            E.cy = current;
            E.cx = rXToCx(row, match - row->render);
            E.rowoff = E.numrows;

            break;
        }
    }
}

/**
 * This function allows us to position the cursor back
 * to its position before the search query if we cancel the search.
 */
void searchWord() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;
    char *word = inputFileName("Search: %s (Use ESC/Arrows/Enter)",
                               searchCallback);
    time_t raw_time;
    struct tm *info;
    time(&raw_time);
    info = localtime(&raw_time);

    char *s = concat("\U00002139: Ctrl-S   \U0001F4BE |Ctrl-Q   \U0001F6AB | Ctrl-F  \U0001F50D |\U000023F1  ",
                     asctime(info));
    setStatusMessage(s);
    if (word) {
        free(word);
    } else {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

/**
 * This function waits for a keypress, and then handles it.
 */
void checkKeyPress() {
    static int quit_times = TECS_QUIT_TIMES; //tracks how many times the user presses ctrl-q
    int c = readKeypress();

    switch (c) {
        case '\r': //Enter key
            insertNewline();
            break;

        case CTRL_KEY('q'):
            if (E.dirty && (quit_times > 0)) {
                setStatusMessage("\U000026A0 File has unsaved changes. "
                                       "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            system("clear");
            exit(0);
            break;

        case CTRL_KEY('s'): //save key which saves the file
            saveFile();
            break;
        case HOME_KEY:
            E.cx = 0; //moves the cursor to the left side of the screen
            break;
        case END_KEY:
            if (E.cy < E.numrows)
                E.cx = E.row[E.cy].size; //allows to move the cursor to the end of current line with end key
            break;

        case CTRL_KEY('f'): //search function key
            searchWord();
            break;

        case BACKSPACE:
        case CTRL_KEY('h'): //ascii for backspace
        case DEL_KEY:
            if (c == DEL_KEY) moveCursor(ARROW_RIGHT);
            deleteChar();
            break;
        case PAGE_UP:
        case PAGE_DOWN: {
            if (c == PAGE_UP) { //allows to scroll up
                E.cy = E.rowoff;
            } else if (c == PAGE_DOWN) { //allows to scroll down
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows) E.cy = E.numrows;
            }
            int times = E.screenrows;
            while (times--)
                moveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            moveCursor(c);
            break;

        case CTRL_KEY('l'): //esc key ascii
        case '\x1b':
            break;

        default:
            insertChar(c); //any keypress which isn't mapped will be inserted directly
            break;
    }

    quit_times = TECS_QUIT_TIMES; //if user presses any other key then ctrl-quit, then it gets reset back to 3
}

/*** init ***/
/**
 * Initializes all the fields in the E struct
 */
void initializeEditor() {
    E.cx = 0; //horizontal coordinate of cursor (column)
    E.cy = 0; //vertical coordinate of cursor (row)
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    if (windowSize(&E.screenrows, &E.screencols) == -1) quit("windowSize");
    E.screenrows -= 2;
}

/**
 * Function to concat two chars needed for Help
 *
 * @param s1 Tools
 * @param s2 time
 *
 * @return concatenated string
 */
char *concat(const char *s1, const char *s2) {
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

/**
 * The keyboard input gets red into the variable c.
 * The while loop reads 1 byte from the standard input into c. It keeps doing it
 * until there are no more bytes to read().
 * @return read() if there are bytes that it red. else 0 if it reaches the end of the file.
 */
int main(int argc, char *argv[]) {
    activateUnprocessedMode();
    initializeEditor();
    if (argc == 2) {
        readFile(argv[1]);
    }
    time_t raw_time;
    struct tm *info;
    time(&raw_time);
    info = localtime(&raw_time);

    char *s = concat("\U00002139: Ctrl-S   \U0001F4BE |Ctrl-Q   \U0001F6AB | Ctrl-F  \U0001F50D | \U000023F1 ",
                     asctime(info));
    setStatusMessage(s);

    while (1) {
        refreshScreen();
        checkKeyPress();
    }
}

