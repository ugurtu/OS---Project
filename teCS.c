#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include<errno.h>


struct termios orig_termios;

void quit(const char *s) {
    perror(s);
    exit(1);
}

void rawModeDis() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        quit("tcsetattr");
    }
}

void rawModeEna() {
    if(tcgetattr(STDIN_FILENO, &orig_termios)==-1){
        quit("tcsetattr");
    }
    atexit(rawModeDis);

    struct termios rawInput = orig_termios;
    rawInput.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    rawInput.c_oflag &= ~(OPOST);
    rawInput.c_cflag &= (CS8);
    rawInput.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    rawInput.c_cc[VMIN] = 0;
    rawInput.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawInput) == -1){
        quit("tcsetattr");
    }

}

int main() {
    rawModeEna();
    while (1) {
        char c = '\0';
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN){
            quit("read");
        }
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') break;
    }
    return 0;
}