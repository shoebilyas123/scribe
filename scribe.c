
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/termios.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

// Macros
#define CTRL_KEY(k)((k) & 0x1f)

/** Data Structures */
struct editorconf {
  struct termios orig_term;
  unsigned short scrnrows;
  unsigned short scrncols;
};

struct editorconf E;


/** Terminal Manipulation */
void drawrows() {
  int y = 0;
  for (y = 0; y < E.scrncols; y++) {
      write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void repcrsr() {
  write(STDOUT_FILENO, "\x1b[H",3);
}

void clearscrn() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  drawrows();
  repcrsr();
}
void die(const char *s) {
  clearscrn();
  perror(s);
  exit(1);
}

void disable_raw_mode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_term)==-1) {
      die("tcsetattr");
    }
}

void enable_raw_mode() {
  // We are fetching the terminal attributes at start
  if (tcgetattr(STDIN_FILENO, &E.orig_term) == -1) {
    die("tcgetattr");
 }
  // This function is called when our program exits
  atexit(disable_raw_mode);

  // The copying the struct value to tc_attr;
  struct termios tc_attr = E.orig_term;
  // Setting the ECHO flag to zero using bitwise operation

  tc_attr.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  tc_attr.c_oflag &= ~(OPOST);
  tc_attr.c_cflag |= (CS8);
  tc_attr.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  tc_attr.c_cc[VMIN] = 0;
  tc_attr.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tc_attr) == -1) {
    die("tcsetattr");
  }
}

char editor_read_key() {
  int bytesread;
  char c;

  while ((bytesread = read(STDIN_FILENO, &c, 1) != 1)) {
      if(bytesread == -1 && errno != EAGAIN) die("read");
  }

  return c;
}

int getCursorPos(unsigned short *rows, unsigned short *cols) {
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }

  char buf[32];
  unsigned int i = 0;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      return -1;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;

  if (sscanf(&buf[2], "%d;%d", rows, cols) != 1) return -1;
  return 0;
}

int getWindowSize(unsigned short *rows, unsigned short *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPos(rows, cols);
  }

  *cols = ws.ws_col;
  *rows = ws.ws_row;
  return 0;
}

/** Input Handling */

void process_key_press() {
  char c = editor_read_key();

  switch (c) {
  case CTRL_KEY('q'):
    clearscrn();
      exit(0);
      break;
    default:
      printf("%c",c);
    }
}

/** Init Editor */

void setup_editor() {
  if (getWindowSize(&E.scrnrows, &E.scrncols) == -1) die("getWindowSize");
}

int main() {
    setup_editor();
    enable_raw_mode();
    while (1) {
      clearscrn();
      process_key_press();
    }
    return 0;
}
