
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/termios.h>
#include <termios.h>
#include <unistd.h>

// Macros
#define CTRL_KEY(k)((k) & 0x1f)

/** Data Structures */
struct termios orig_term;


/** Terminal Manipulation */
void drawrows() {
  int y = 0;
  for (y = 0; y < 24; y++) {
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
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term)==-1) {
      die("tcsetattr");
    }
}

void enable_raw_mode() {
  // We are fetching the terminal attributes at start
  if (tcgetattr(STDIN_FILENO, &orig_term) == -1) {
    die("tcgetattr");
 }
  // This function is called when our program exits
  atexit(disable_raw_mode);

  // The copying the struct value to tc_attr;
  struct termios tc_attr = orig_term;
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

/** Main */
int main() {
    enable_raw_mode();
    while (1) {
      clearscrn();
        process_key_press();
    }
    return 0;
}
