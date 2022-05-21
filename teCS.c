#include <unistd.h>
#include <termios.h>

/**
 * This method activates the raw mode. That means that it turn's the ECHO feature off.
 * Acts the same way as if you are typing a password in the terminal.
 * Terminal attributes will be red into the struct by the tcgetaatr() method. After modifying them
 * they will by applied to the terminal by the tcsetattr() method. The TCSAFLUSH argument waits for
 * all pending output to be written to the terminal, and also discards any input that hasn’t been read.
 */
void rawMode() {

    struct termios raw_input;
    tcgetattr(STDIN_FILENO, &raw_input); //Get the attributes for a terminal
    raw_input.c_lflag &= ~(ECHO); //local flags. dumping ground for other state
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_input); //Set the attributes for a terminal

}
/**
 * The keyboard input gets red into the variable c.
 * The while loop reads 1 byte from the standart input into c. It keeps doing it
 * until there are no more bytes to read().
 * @return read() if there are bytes that it red. else 0 if it reaches the end of the file.
 */
int main() {
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}

