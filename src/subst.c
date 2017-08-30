#include "ed.h"
#include <assert.h>
#include <string.h>

static char rhsbuf[LBSIZE / 2];

void
dosub(void)
{
        char *lp, *sp, *rp;
        int c;

        rp = rhsbuf;
        sp = genbuf_putm(genbuf, linebuf, loc1);
        while ((c = *rp++ & 0377) != '\0') {
                struct bralist_t *b;
                if (c == '&') {
                        sp = genbuf_putm(sp, loc1, loc2);
                        continue;
                } else if ((b = get_backref(c)) != NULL) {
                        sp = genbuf_putm(sp, b->start, b->end);
                        continue;
                } else {
                        sp = genbuf_putc(sp, c & 0177);
                }
        }
        lp = loc2;
        loc2 = sp - genbuf + linebuf;
        do {
                sp = genbuf_putc(sp, *lp);
        } while (*lp++ != '\0');

        strcpy(linebuf, genbuf);
}

int
compsub(void)
{
        int seof, c;
        char *p;
        int ret;

        if ((seof = getchr()) == '\n' || seof == ' ')
                qerror();
        compile(seof);
        p = rhsbuf;
        for (;;) {
                c = getchr();
                if (c == '\\')
                        c = getchr() | 0200;
                if (c == '\n') {
                        if (!istt())
                                c |= 0200;
                        else
                                qerror();
                }
                if (c == seof)
                        break;
                p = buffer_putc(p, c, &rhsbuf[LBSIZE / 2]);
        }
        buffer_putc(p, '\0', &rhsbuf[LBSIZE / 2]);

        c = getchr();
        ret = (c == 'g');
        ungetchr(ret ? '\0' : c);
        return ret;
}
