#include "ed.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
                tt.lastc = getchar();
        }
        return tt.lastc;
}

char *
ttgetdelim(int delim)
{
        int c;
        struct buffer_t b = BUFFER_INITIAL();
        do {
                c = getchr();
                if (c == EOF) {
                        if (b.count == 0) {
                                buffer_free(&b);
                                return NULL;
                        }
                        break;
                }
                buffer_putc(&b, c);
        } while (c != delim);

        buffer_putc(&b, '\0');
        return b.base;
}

void
ttlwrap(int en)
{
        tt.listf = !!en;
}


void
putchr(int c)
{
        enum { NCOL = 72 };

        if (tt.listf) {
                tt.col++;
                if (tt.col >= NCOL) {
                        tt.col = 0;
                        putchar('\\');
                        putchar('\n');
                }

                if (c == '\t') {
                        c = '>';
                        goto esc;
                }

                if (c == '\b') {
                        c = '<';
                esc:
                        putchar('-');
                        putchar('\b');
                        putchar(c);
                        return;
                }

                if (c < ' ' && c != '\n') {
                        putchar('\\');
                        putchar((c >> 3) + '0');
                        putchar((c & 07) + '0');
                        tt.col += 2;
                        return;
                }
        }
        putchar(c);
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
