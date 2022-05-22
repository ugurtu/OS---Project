/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f) //allows to quit the program with a ctrl-keymakro

/*** data ***/
/**
 *
 */
struct editorConfig {
    int screenrows;
    int screencols;
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
char readKey() {
    int n_read;
    char c;
    while ((n_read = read(STDIN_FILENO, &c, 1)) != 1) {
        if (n_read == -1 && errno != EAGAIN) die("read");
    }
    return c;
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
    if (buf[0] != '\x1b' || buf[1] != '[') return -1; //This ensures that we dont print the escape character
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1; // 3rd char, two int's will be parsed to get rows and columns

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

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}
void abFree(struct abuf *ab) {
    free(ab->b);
}
/*** output ***/
void refreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

/**
 * This functions draws a tilde at every row like vim.
 */
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                                      "Kilo editor -- version %s", KILO_VERSION);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
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
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/
/**
 * This function waits for a keypress, and then handles it.
 */
void processKeyPress() {
    char c = readKey();
    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4); //escape sequence
            write(STDOUT_FILENO, "\x1b[H", 3); //escape sequence
            exit(0);
            break;
    }
}

/*** init ***/
/**
 * Initializes all the fields in the E struct
 */
void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

/**
 * The keyboard input gets red into the variable c.
 * The while loop reads 1 byte from the standard input into c. It keeps doing it
 * until there are no more bytes to read().
 * @return read() if there are bytes that it red. else 0 if it reaches the end of the file.
 */
int main() {
    activateRawMode();
    initEditor();
    while (1) {
        refreshScreen();
        processKeyPress();
    }
}

