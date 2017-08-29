#include "ed.h"
#include <unistd.h>
#include <stdio.h>

static struct term_t {
        int peekc;
        int lastc;
        const char *globp;
        int col;
        int listf;
} tt;


/* s is a pointer to a buffer or NULL to get chars from terminal */
void
set_inp_buf(const char *s)
{
        tt.globp = s;
}

int
regetchr(void)
{
        return tt.lastc;
}

int
istt(void)
{
        return tt.globp == NULL;
}

void
ungetchr(int c)
{
        if (c == EOF)
                c = tt.lastc;
        tt.peekc = c;
}

/*
 * Terminal interface, not the file being edited.
 */
int
getchr(void)
{
        if (tt.peekc != '\0') {
                tt.lastc = tt.peekc;
                tt.peekc = '\0';
        } else if (tt.globp) {
                tt.lastc = *tt.globp++;
                if (tt.lastc == '\0') {
                        tt.globp = NULL;
                        return EOF;
                }
        } else {
                char c;
                int count = read(STDIN_FILENO, &c, 1);
                tt.lastc = count <= 0 ? EOF : c & 0177;
        }
        return tt.lastc;
}

void
ttlwrap(int en)
{
        tt.listf = !!en;
}

void
putchr(int ac)
{
        enum { NCOL = 72 };
        static char line[70];
        static char *linp = line;
        char *lp;
        int c;

        lp = linp;
        c = ac;
        if (tt.listf) {
                tt.col++;
                if (tt.col >= NCOL) {
                        tt.col = 0;
                        *lp++ = '\\';
                        *lp++ = '\n';
                }

                if (c == '\t') {
                        c = '>';
                        goto esc;
                }

                if (c == '\b') {
                        c = '<';
                esc:
                        *lp++ = '-';
                        *lp++ = '\b';
                        *lp++ = c;
                        goto out;
                }

                if (c < ' ' && c != '\n') {
                        *lp++ = '\\';
                        *lp++ = (c >> 3) + '0';
                        *lp++ = (c & 07) + '0';
                        tt.col += 2;
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
putstr(const char *sp)
{
        tt.col = 0;
        while (*sp)
                putchr(*sp++);
        putchr('\n');
}

/* IE  printf("%d", count);  */
void
putd(long v)
{
        int r;

        r = v % 10;
        v /= 10;
        if (v)
                putd(v);
        putchr(r + '0');
}
