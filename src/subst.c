#include "ed.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>

static char rhsarr[LBSIZE / 2];

static struct buffer_t rhsbuf = BUFFER_INITIAL(rhsarr, sizeof(rhsarr));

void
dosub(void)
{
        char *rp;
        int c;

        rp = buffer_ptr(&rhsbuf);
        /* do not reset genbuf here */
        buffer_memapp(&genbuf, linebuf.base, loc1);
        while ((c = *rp++ & 0377) != '\0') {
                struct bralist_t *b;
                if (c == '&') {
                        buffer_memapp(&genbuf, loc1, loc2);
                        continue;
                } else if ((b = get_backref(c)) != NULL) {
                        buffer_memapp(&genbuf, b->start, b->end);
                        continue;
                } else {
                        buffer_putc(&genbuf, toascii(c));
                }
        }

        buffer_strapp(&genbuf, loc2);
        buffer_strcpy(&linebuf, &genbuf);
        loc2 = buffer_ptr(&linebuf);
}

int
compsub(void)
{
        int seof, c;
        int ret;

        if ((seof = getchr()) == '\n' || seof == ' ')
                qerror();
        compile(seof);
        buffer_reset(&rhsbuf);
        for (;;) {
                c = getchr();
                if (c == '\\')
                        c = getchr() | HIGHBIT;
                if (c == '\n') {
                        if (!istt())
                                c |= HIGHBIT;
                        else
                                qerror();
                }
                if (c == seof)
                        break;
                buffer_putc(&rhsbuf, c);
        }
        buffer_putc(&rhsbuf, '\0');

        c = getchr();
        ret = (c == 'g');
        ungetchr(ret ? '\0' : c);
        return ret;
}
