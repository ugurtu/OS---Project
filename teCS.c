#include <ctype.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include<errno.h>
#include <sys/ioctl.h>
#include <string.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "Alpha"
#define YEL "\x1B[33m"
#define RESET "\x1B[0m"

struct termios orig_termios;

void processKeypress();

struct editorConfiguration {
    int cursorX;
    int cursorY;
    int screenRow;
    int screenCol;

    struct termios orig_termios;
};

struct editorConfiguration E;

void quit(const char *s) {
    write(STDIN_FILENO, "\x1b[2J", 4);
    write(STDIN_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void rawModeQuit() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        quit("tcesetattr");
    }
}

void rawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        quit("tcesetattr");
    }
    atexit(rawModeQuit);

    struct termios rawInput = E.orig_termios;
    rawInput.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    rawInput.c_oflag &= ~(OPOST);
    rawInput.c_cflag &= (CS8);
    rawInput.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    rawInput.c_cc[VMIN] = 0;
    rawInput.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawInput) == -1) {
        quit("tcesetattr");
    }

}

char keyRead() {
    int retNumber = 0;
    char c;
    while ((retNumber == read(STDIN_FILENO, &c, 1)) != 1) {
        if (retNumber == -1 && errno != EAGAIN) {
            quit("read");
        }
    }
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) { return -1; }
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}


int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return getCursorPosition(rows, cols);
        }
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

struct appendBuffer {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void aBufferAppend(struct appendBuffer *aBuffer, const char *s, int len) {
    char *new = realloc(aBuffer->b, aBuffer->len + len);

    if (new == NULL) {
        return;
    } else {
        memcpy(&new[aBuffer->len], s, len);
        aBuffer->b = new;
        aBuffer->len += len;
    }
}

void freeBuffer(struct appendBuffer *aBuffer) {
    free(aBuffer->b);
}

void processKeypress() {
    char c = keyRead();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDIN_FILENO, "\x1b[2J", 4);
            write(STDIN_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

void editorPrintRows(struct appendBuffer *aBuffer) {

    for (int i = 0; i < E.screenRow; ++i) {
        char int_to_char = i+'0';
        if (i == E.screenRow / 3) {
            char welcome[68];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                                      "\033[1m" YEL "teCS editor -- version %s" RESET"\033[0m", KILO_VERSION);
            if (welcomelen > E.screenCol) welcomelen = E.screenCol;
            int padding = (E.screenCol - welcomelen) / 2;
            if (padding) {
                aBufferAppend(aBuffer, &int_to_char, 1);
                padding--;
            }
            while (padding--) aBufferAppend(aBuffer, " ", 1);
            aBufferAppend(aBuffer, welcome, welcomelen);
        } else {
            aBufferAppend(aBuffer, &int_to_char, 1);
        }
        aBufferAppend(aBuffer, "\x1b[K", 3);
        if (i < E.screenRow - 1) {
            aBufferAppend(aBuffer, "\r\n", 2);
        }
    }


}

void refreshScreen() {
    struct appendBuffer aBuffer = ABUF_INIT;

    aBufferAppend(&aBuffer, "\x1b[?25l", 6);
    aBufferAppend(&aBuffer, "\x1b[H", 3);

    editorPrintRows(&aBuffer);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH",E.cursorY+1,E.cursorX+1);
    aBufferAppend(&aBuffer,buf, strlen(buf));

    aBufferAppend(&aBuffer, "\x1b[?25h", 6);

    write(STDOUT_FILENO, aBuffer.b, aBuffer.len);
    freeBuffer(&aBuffer);
}

void initializeEditor() {
    E.cursorX = 0;
    E.cursorY = 0;

    if (getWindowSize(&E.screenRow, &E.screenCol) == -1) {
        quit("getWindowSize");
    }
}

int main() {
    rawMode();
    initializeEditor();

    while (1) {
        refreshScreen();
        processKeypress();
    }

    return 0;
}
