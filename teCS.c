#include <unistd.h>
#include <termios.h>

void rawMode() {

    struct termios raw_input;
    tcgetattr(STDIN_FILENO, &raw_input);
    raw_input.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_input);

}

int main() {
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}

