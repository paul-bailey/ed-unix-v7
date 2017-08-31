#include "ed.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

static struct term_t {
        int peekc;
        int lastc;
        const char *globp;
        int col;
        int listf;
} tt;


/**
 * set_inp_buf - Replace stdin with a nul-terminated string.
 * @s: pointer to a buffer or NULL to use stdin again.
 *
 * @s will be used for getchr() until the end of the string.
 * When the nul-char termination is encountered, EOF will be
 * returned and stdin will be used for future calls to getchr().
 */
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

/**
 * tty_get_line - fill lb with line from terminal.
 * @lb: Line buffer to fill.  If tty_get_line() returns 0, this will
 *      contain a nul-char-terminated string from its base.  It will
 *      not include the final newline character.  If tty_get_line()
 *      returns EOF, @lb will not be guaranteed to have a newline.
 *
 * Escapes:
 *
 * If a backslash is typed, then:
 * - if it is followed by a newline, the newline is inserted
 *   into lb and tty_get_line() will continue to the next un-escaped
 *   newline.
 * - if it is followed by anything else, it will be inserted into
 *   lb along with the following character.
 *
 * Return:
 *
 * If EOF is encountered before newline, EOF is returned.
 * Otherwise, 0 is returned.
 */
int
tty_get_line(struct buffer_t *lb)
{
        int gf;

        buffer_reset(lb);
        gf = !istt();
        for (;;) {
                int c = getchr();
                if (c == EOF) {
                        eof:
                        if (gf)
                                ungetchr(c);
                        return c;
                } else if (c == '\\') {
                        c = getchr();
                        if (c == EOF)
                                goto eof;
                        if (c != '\n')
                                buffer_putc(lb, '\\');
                } else if (c == '\n') {
                        break;
                }
                c = toascii(c);
                if (c == '\0')
                        continue;
                buffer_putc(lb, c);
        }
        buffer_putc(lb, '\n');
        buffer_putc(lb, '\0');
        return '\0';
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
