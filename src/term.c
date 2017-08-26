#include "ed.h"
#include <unistd.h>
#include <stdio.h>

static int col;

/*
 * Terminal interface, not the file being edited.
 */
int
getchr(void)
{
        char c;
        if (!!(lastc = peekc)) {
                peekc = 0;
                return lastc;
        }
        if (globp) {
                if ((lastc = *globp++) != 0)
                        return lastc;
                globp = 0;
                return EOF;
        }
        if (read(0, &c, 1) <= 0)
                return lastc = EOF;
        lastc = c & 0177;
        return lastc;
}

void
putchr(int ac)
{
        static char line[70];
        static char *linp = line;
        char *lp;
        int c;

        lp = linp;
        c = ac;
        if (listf) {
                col++;
                if (col >= 72) {
                        col = 0;
                        *lp++ = '\\';
                        *lp++ = '\n';
                }
                if (c=='\t') {
                        c = '>';
                        goto esc;
                }
                if (c=='\b') {
                        c = '<';
                esc:
                        *lp++ = '-';
                        *lp++ = '\b';
                        *lp++ = c;
                        goto out;
                }
                if (c<' ' && c!= '\n') {
                        *lp++ = '\\';
                        *lp++ = (c >> 3) + '0';
                        *lp++ = (c & 07) + '0';
                        col += 2;
                        goto out;
                }
        }
        *lp++ = c;
out:
        if (c == '\n' || lp >= &line[64]) {
                linp = line;
                write(STDOUT_FILENO, line, lp - line);
                return;
        }
        linp = lp;
}

void
putstr(char *sp)
{
        col = 0;
        while (*sp)
                putchr(*sp++);
        putchr('\n');
}
