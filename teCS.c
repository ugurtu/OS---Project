#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

/**
 * safdoasdfosajdfndsafdapsüf
 * @param s
 */
void die(const char *s) {
    perror(s);
    exit(1);
}

/**
 * This method returns the terminal back to its normal state. That means restoring the terminals original
 * attributes when we exit the program. We store the original terminal attributes in orig_termios.
 */
void deactivateRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    die("tcsetattr");
}

/**
 * This method activates the raw mode. That means that it turn's the ECHO feature off.
 * Acts the same way as if you are typing a password in the terminal.
 * Terminal attributes will be red into the struct by the tcgetaatr() method. After modifying them
 * they will by applied to the terminal by the tcsetattr() method. The TCSAFLUSH argument waits for
 * all pending output to be written to the terminal, and also discards any input that hasn’t been read.
 */
void activateRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(deactivateRawMode);

    struct termios raw_input = orig_termios;
    raw_input.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_input.c_oflag &= ~(OPOST);
    raw_input.c_cflag |= (CS8);
    raw_input.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw_input.c_cc[VMIN] = 0;
    raw_input.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_input) == -1) die("tcsetattr");

}

/**
 * The keyboard input gets red into the variable c.
 * The while loop reads 1 byte from the standard input into c. It keeps doing it
 * until there are no more bytes to read().
 * @return read() if there are bytes that it red. else 0 if it reaches the end of the file.
 */
int main() {
    activateRawMode();

    while (1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') break;
    }
}

