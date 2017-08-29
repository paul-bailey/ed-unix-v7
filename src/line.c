#include "ed.h"
#include <string.h>

char linebuf[LBSIZE];

static char *linebp = NULL;
static int tline;

int
tty_to_line(void)
{
        int c;
        int gf;
        char *p;

        p = linebuf;
        gf = !istt();
        while ((c = getchr()) != '\n') {
                if (c == EOF) {
                        if (gf)
                                ungetchr(c);
                        return c;
                }
                c &= 0177;
                if (c == '\0')
                        continue;
                p = linebuf_putc(p, c);
        }
        linebuf_putc(p, '\0');
        if (linebuf[0] == '.' && linebuf[1] == '\0')
                return EOF;
        return '\0';
}

int
line_to_tempf(void)
{
        char *bp, *lp;
        int nleft;
        int tl;
        int c;

        fchange = 1;
        lp = linebuf;
        tl = tline;
        bp = getblock(tl, WRITE, &nleft);
        tl &= ~0377;
        while ((c = *lp++) != '\0') {
                if (c == '\n') {
                        *bp = '\0';
                        linebp = lp;
                        break;
                }
                *bp++ = c;
                if (--nleft == 0)
                        bp = getblock(tl += 0400, WRITE, &nleft);
        }
        nleft = tline;
        /* XXX: What the hell is this! */
        tline += (((lp - linebuf) + 03) >> 1) & 077776;
        return nleft;
}

char *
tempf_to_line(int tl)
{
        char *bp, *lp;
        int nleft;
        int c;

        lp = linebuf;
        bp = getblock(tl, READ, &nleft);
        tl &= ~0377;
        /* TODO: What if insanely long line! */
        while ((c = *bp++) != '\0') {
                lp = linebuf_putc(lp, c);
                if (--nleft <= 0)
                        bp = getblock(tl += 0400, READ, &nleft);
        }
        return linebuf;
}

int
line_getsub(void)
{
        if (linebp == NULL)
                return EOF;
        strcpy(linebuf, linebp);
        linebp = NULL;
        return 0;
}

void
lineinit(void)
{
        tline = 2;
}
