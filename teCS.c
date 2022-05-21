#include <unistd.h>
#include <termios.h>

void rawMode() {

    struct termios raw_input;
    tcgetattr(STDIN_FILENO, &raw_input);
    raw_input.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_input);

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

