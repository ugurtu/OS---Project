/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8

#define CTRL_KEY(k) ((k) & 0x1f) //allows to quit the program with a ctrl-keymakro

enum editorKey {
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
 * erow stores line of text as a pointer to the dynamicallyallocated char data and a lenght.
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
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;
};
struct editorConfig E;//stores the terminal attributes



/*** terminal ***/
/**
 * A exit method for the program.
 * @param
 */
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); //the following two escape sequences clear the screen when we exit the program
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s); //prints a descriptive error message
    exit(1); // 1 because this indicates failure
}

/**
 * This method returns the terminal back to its normal state. That means restoring the terminals original
 * attributes when we exit the program. We store the original terminal attributes in orig_termios.
 */
void deactivateRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

/**
 * This method activates the raw mode. That means that it turn's the ECHO feature off.
 * Acts the same way as if you are typing a password in the terminal.
 * Terminal attributes will be red into the struct by the tcgetaatr() method. After modifying them
 * they will by applied to the terminal by the tcsetattr() method. The TCSAFLUSH argument waits for
 * all pending output to be written to the terminal, and also discards any input that hasn't been red.
 */
void activateRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");

    atexit(deactivateRawMode);

    struct termios raw_input = E.orig_termios;
    raw_input.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); //turning off more flags like underneath
    raw_input.c_oflag &= ~(OPOST); //turns off all output processing features
    raw_input.c_cflag |= (CS8);
    raw_input.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); //disables Ctrl-(C,Z,S,Q,V)
    raw_input.c_cc[VMIN] = 0; //read() returns as soon as there is any input to read
    raw_input.c_cc[VTIME] = 1; //100 milliseconds wait before read() returns

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_input) == -1) die("tcsetattr");

}

/**
 * This function waits for one keypress and returns it.
 * @return the keypress
 */
int readKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
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
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; //ask for the cursor position
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
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; //moves the cursor to the right bottom
        return getCursorPosition(rows, cols);
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
int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) { //loop through all the characters left of cx
        if (row->chars[j] == '\t')
            rx += (KILO_TAB_STOP - 1) - (rx %
                                         KILO_TAB_STOP); //substract the amount of columns we are right of last tab stop from amount of columns to the left of next tab stop.
        rx++; // this gets us to the next tab stop
    }
    return rx;
}

/**
 * This function uses the chars string of an erow to fill the contents of the rendered string.
 * @param row
 */
void editorUpdateRow(erow *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++; //we loop through the chars of the row
    free(row->render);
    row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1); //allocate memory for render(count of tabs)
    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') { //checks whether the current character is a tab
            row->render[idx++] = ' '; // if it is, we append one space
            while (idx % KILO_TAB_STOP != 0)
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
 * to a new erow at the end of the E.row array.
 * @param s line
 * @param len of line
 */
void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row,
                    sizeof(erow) * (E.numrows + 1)); //multiply the number of bytes of each erow by the number of rows.

    int at = E.numrows; // setting at to the index of the new row we initialize
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.numrows++;
}


/*** append buffer ***/

struct abuf {
    char *b; //pointer to our buffer in memory
    int len;
};

#define ABUF_INIT {NULL, 0} //acts as a constructor

/**
 * This function appends the string to an abuf buffer
 * @param ab append buffer
 * @param s string
 * @param len of string
 */
void abAppend(struct abuf *ab, const char *s, int len) {
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
void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/
/**
 * This function checks if the users cursor has moved outside of the visible window
 * and adjusts E.rowoff so that the cursor is just inside the visible window.
 */
void editorScroll() {
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
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
void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status),
                       "%.20s - %d lines", //Up to 20 characters of the file name is displayed & number of lines
                       E.filename ? E.filename : "[No Name]",
                       E.numrows); //If the file has no name, then we just display "No Name"
    int rlen = snprintf(rstatus, sizeof(rstatus),
                        "%d/%d", //shows the current line number on the right edge of the window
                        E.cy + 1, E.numrows); //have to add +1 because e.cy is indexed with 0
    if (len > E.screencols) len = E.screencols; //cuts the status string short if it doesn't fit in the window
    abAppend(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) { //if length are equal to status string
            abAppend(ab, rstatus, rlen); //print the status and break
            break;
        } else {
            abAppend(ab, " ",
                     1); //else print spaces as long as we get to the point where status is against edge of screen.
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);//Bold
    abAppend(ab, "\r\n", 2);
}

/**
 * This function draws a message bar and then displays that in that bar
 * @param ab
 */
void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5) //set to 5 seconds intervall
        abAppend(ab, E.statusmsg, msglen);
}

/**
 * This functions draws a tilde at every row like vim.
 */
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff; //at each y position we add E.rowoff to the y position
        if (filerow >= E.numrows) {
            if (E.numrows == 0 &&
                y == E.screenrows / 3) { //shows the welcome message only when the editor opens without selecting a file
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                                          "teCS -- version %s", KILO_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E.row[filerow].rsize -
                      E.coloff; //subtract the number of characters that are to the left of the offset from the length of the row.
            if (len < 0) len = 0; //setting len = 0 so that nothing is displayed on that line
            if (len > E.screencols) len = E.screencols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len); //displays each row at the column offset
        }
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

/**
 * This function renders the interface.
 */
void editorRefreshScreen() {
    editorScroll();
    struct abuf ab = ABUF_INIT; //init buffer

    abAppend(&ab, "\x1b[?25l", 6); //hide cursor
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
             (E.rx - E.coloff) +
             1); //To position the cursor on the screen, we have to subtract E.rowoff from E.cy. Same with E.rx. vor horizontal scrolling.

    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); //show cursor

    write(STDOUT_FILENO, ab.b, ab.len); //whole screen updates at once
    abFree(&ab);
}

/**
 * This function takes a format string and number of arguments
 * @param fmt
 * @param ... allows to take any number of arguments
 */
void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap); //status message with number of seconds
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*** input ***/
/**
 * Handles the the cursor position
 * @param key pressed
 */
void editorMoveCursor(int key) {
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
 * This function waits for a keypress, and then handles it.
 */
void processKeyPress() {
    int c = readKey();
    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4); //escape sequence
            write(STDOUT_FILENO, "\x1b[H", 3); //escape sequence
            exit(0);
            break;
        case HOME_KEY:
            E.cx = 0; //moves the cursor to the left side of the screen
            break;
        case END_KEY:
            if (E.cy < E.numrows)
                E.cx = E.row[E.cy].size; //allows to move the cursor to the end of current line with end key
            break;

        case PAGE_UP:
        case PAGE_DOWN: {
            if (c == PAGE_UP) { //alows to scroll up
                E.cy = E.rowoff;
            } else if (c == PAGE_DOWN) { //allows to scroll down
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows) E.cy = E.numrows;
            }
            int times = E.screenrows;
            while (times--)
                editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
            break;

            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

/**
 * This method opens and reads a file from the disk. It takes the filename and opens the file.
 * @param filename file which will be opened and red.
 */
void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r"); //opens the file
    if (!fp) die("fopen");
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) { //allows to read an entire file into E.row
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        editorAppendRow(line, linelen);
    }
    free(line); //freeing from allocation
    fclose(fp);
}

/*** init ***/
/**
 * Initializes all the fields in the E struct
 */
void initEditor() {
    E.cx = 0; //horizontal coordinate of cursor (column)
    E.cy = 0; //vertical coordinate of cursor (row)
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;
}

/**
 * The keyboard input gets red into the variable c.
 * The while loop reads 1 byte from the standard input into c. It keeps doing it
 * until there are no more bytes to read().
 * @return read() if there are bytes that it red. else 0 if it reaches the end of the file.
 */
int main(int argc, char *argv[]) {
    activateRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]); //sali
    }
    editorSetStatusMessage("HELP: Ctrl-Q = quit");
    while (1) {
        editorRefreshScreen();
        processKeyPress();
    }
}

