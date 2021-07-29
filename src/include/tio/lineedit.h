#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#define LINE_SIZE   81  	/* 80 chars + terminating */

#define GOTOXY(x,y)	fprintf(stdout, "\033[%d;%dH", (y), (x))

#define ACTION_PREFIX_1 0x1b
#define ACTION_PREFIX_2 0x5b
#define ARROW_LEFT      0x44
#define ARROW_RIGHT     0x43
#define ARROW_UP        0x41
#define ARROW_DOWN      0x42

#define BACKSPACE       0x7f

void lineedit_configure(const char * prefix, size_t max_len);
void add_to_history(const char * line);
char * get_line(char c);
void free_line(char * line);

#endif
