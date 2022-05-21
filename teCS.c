#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

struct termios orig_termios;

/**
 * This method returns the terminal back to its normal state. That means restoring the terminals original
 * attributes when we exit the program. We store the original terminal attributes in orig_termios.
 */
void deactivateRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); //leftover input is guaranteed to be not red by TCSAFLUSH.
}

/**
 * This method activates the raw mode. That means that it turn's the ECHO feature off.
 * Acts the same way as if you are typing a password in the terminal.
 * Terminal attributes will be red into the struct by the tcgetaatr() method. After modifying them
 * they will by applied to the terminal by the tcsetattr() method. The TCSAFLUSH argument waits for
 * all pending output to be written to the terminal, and also discards any input that hasn't been read.
 */
void activateRawMode() {

    tcgetattr(STDIN_FILENO, &orig_termios);  //Get the attributes for a terminal
    atexit(deactivateRawMode); //calls the deactivateRawMode function automatically when the program exits.

    struct termios raw_input = orig_termios;
    raw_input.c_lflag &= ~(ECHO); //local flags. Echo is a bitflag
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_input); //Set the attributes for a terminal

}

/**
 * The keyboard input gets red into the variable c.
 * The while loop reads 1 byte from the standard input into c. It keeps doing it
 * until there are no more bytes to read().
 * @return read() if there are bytes that it red. else 0 if it reaches the end of the file.
 */
int main() {
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}

