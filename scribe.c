#include <errno.h>
#include <ctype.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/termios.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

// Macros
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"

/** Data Structures */
typedef struct rows {
  char *line;
  int len;
} erow;

struct editorconf {
  int cx;
  int cy;
  struct termios orig_term;
  unsigned short scrnrows;
  unsigned short scrncols;
  erow row;
  int numrows;
};

struct editorconf E;

/** Append Buffer */
struct abuf {
  char *b;
  int len;
};

enum CRS_DIR {
  ARROW_UP = 1000,
  ARROW_DOWN,
  ARROW_RIGHT,
  ARROW_LEFT,
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  DEL_KEY,
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *newAB = realloc(ab->b, ab->len + len);

  if (newAB == NULL)
    return;
  memcpy(&newAB[ab->len], s, len);
  ab->b = newAB;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/** Terminal Manipulation */
void drawrows(struct abuf *ab) {
  int y = 0;
  for (y = 0; y < E.scrnrows; y++) {
    if (y >= E.numrows) {
      if (y == E.scrncols / 3) {
          char welcome[80];
          int welcomelen = snprintf(welcome, sizeof(welcome),
            "Scribe editor -- version %s", KILO_VERSION);
          if (welcomelen > E.scrncols)
            welcomelen = E.scrncols;
          int padding = (E.scrncols + welcomelen) / 3;
          if (padding) {
            abAppend(ab, "~", 1);
            padding--;
          }

          while (padding--) {
            abAppend(ab, " ", 1);
          };

          abAppend(ab, welcome, welcomelen);
        } else {
          abAppend(ab, "~", 1);
        };
    } else {
      int max_length = E.row.len;
      if(max_length > E.scrncols) max_length = E.scrncols;
        abAppend(ab, E.row.line, max_length);
    }
    // This commands the terminal to clear one line at a time
    abAppend(ab, "\x1b[K", 3);
    if (y < E.scrnrows - 1) {
        abAppend(ab, "\r\n", 2);
    }
  }
}

void repcrsr(struct abuf *ab) {
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(ab, buf, strlen(buf));
}

void clearscrn() {
  struct abuf ab = ABUF_INIT;

  drawrows(&ab);
  repcrsr(&ab);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
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

int editor_read_key() {
  int bytesread;
  char c;

  while ((bytesread = read(STDIN_FILENO, &c, 1) != 1)) {
      if(bytesread == -1 && errno != EAGAIN) die("read");
  }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';

        if (seq[2] == '~') {
          switch (seq[1]) {
              case '1': return HOME_KEY;
              case '7': return HOME_KEY;
              case '4': return END_KEY;
              case '8': return END_KEY;
              case '5': return PAGE_UP;
              case '6':
                return PAGE_DOWN;
                case '3': return DEL_KEY;

          }
        }
      } else {
        switch (seq[1]) {
            case 'A':  return ARROW_UP;
            case 'B':  return ARROW_DOWN;
            case 'C':  return ARROW_RIGHT;
            case 'D':
              return ARROW_LEFT;
            case 'H': return HOME_KEY;
                       case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
          switch (seq[1]) {
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
          }
    }

    return '\x1b';
  } else {
  return c;
  }
}

void editor_mv_crsr(int key) {
  switch (key) {
  case ARROW_LEFT:
    if (E.cx - 1 >= 0) {
    E.cx--;
  }
    break;
  case ARROW_RIGHT:
  if (E.cx + 1 < E.scrncols) {
  E.cx++;
}
    break;
    case ARROW_UP:
    if (E.cy - 1 >= 0) {
    E.cy--;
  }
      break;
      case ARROW_DOWN:
      if (E.cy + 1 < E.scrnrows) {
      E.cy++;
    }
        break;
  }
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

  if (sscanf(&buf[2], "%hu;%hu", rows, cols) != 2) return -1;
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
  int c = editor_read_key();

  switch (c) {
  case CTRL_KEY('q'):
    clearscrn();
    exit(0);
    break;
    case HOME_KEY:
        E.cx = 0;
        break;
      case END_KEY:
        E.cx = E.scrncols - 1;
        break;

  case PAGE_UP:
    E.cy = 0;
    break;
  case PAGE_DOWN:
    E.cy = E.scrnrows - 1;
    break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_RIGHT:
    case ARROW_LEFT:
    editor_mv_crsr(c);
    break;
    default:
      printf("%c",c);
    }
}

/** File I/O */
void open_editor(char *f_path) {
  FILE *fp = fopen(f_path, "r");
  size_t linecap = 0;
  char *line = NULL;
  ssize_t size = getline(&line, &linecap, fp);
  if (size != -1) {
    while (size > 0 && (line[size - 1] == '\n' || line[size - 1] == '\r'))
      size--;
    E.row.len = size;
    E.row.line = malloc(size+1);
    memcpy(E.row.line, line, size);
    E.row.line[size] = '\0';
    E.numrows += 1;

  }

  free(line);
  fclose(fp);
}

/** Init Editor */

void setup_editor() {
  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;
  if (getWindowSize(&E.scrnrows, &E.scrncols) == -1) die("getWindowSize");
}

int main(int argc, char *argv[]) {
    setup_editor();
    enable_raw_mode();

  if (argc >= 1) {
      open_editor(argv[1]);
  }
    while (1) {
        clearscrn();
      process_key_press();
    }
    return 0;
}
