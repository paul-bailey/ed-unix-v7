#include "ed.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>

static struct buffer_t rhsbuf = BUFFER_INITIAL();

void
dosub(struct buffer_t *lb)
{
        char *rp;
        int c;

        assert(loc1 >= lb->base);
        assert(loc1 < &lb->base[lb->size]);

        rp = buffer_ptr(&rhsbuf);
        /* do not reset genbuf here */
        buffer_memapp(&genbuf, lb->base, loc1);
        while ((c = (*rp++ & 0377)) != '\0') {
                struct bralist_t *b;
                if (c == '&') {
                        assert(loc2 >= lb->base);
                        assert(loc2 < &lb->base[lb->size]);
                        buffer_memapp(&genbuf, loc1, loc2);
                } else if ((b = get_backref(c)) != NULL) {
                        buffer_memapp(&genbuf, b->start, b->end);
                } else {
                        buffer_putc(&genbuf, toascii(c));
                }
        }

        buffer_strapp(&genbuf, loc2);
        buffer_strcpy(lb, &genbuf);
        loc2 = buffer_ptr(lb);
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
