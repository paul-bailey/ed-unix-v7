#include "ed.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static struct buffer_t rhsbuf = BUFFER_INITIAL();

void
dosub(struct code_t *cd)
{
        char *rp;
        int c;

        assert(cd->loc1 >= cd->lb.base);
        assert(cd->loc1 < &cd->lb.base[cd->lb.size]);

        rp = buffer_ptr(&rhsbuf);
        /* do not reset genbuf here */
        buffer_memapp(&genbuf, cd->lb.base, cd->loc1);
        while ((c = (*rp++ & 0377)) != '\0') {
                struct bralist_t *b;
                if (c == '&') {
                        assert(cd->loc2 >= cd->lb.base);
                        assert(cd->loc2 < &cd->lb.base[cd->lb.size]);
                        buffer_memapp(&genbuf, cd->loc1, cd->loc2);
                } else if ((b = get_backref(c)) != NULL) {
                        buffer_memapp(&genbuf, b->start, b->end);
                } else {
                        buffer_putc(&genbuf, toascii(c));
                }
        }

        buffer_strapp(&genbuf, cd->loc2);
        buffer_strcpy(&cd->lb, &genbuf);
        cd->loc2 = buffer_ptr(&cd->lb);
}

int
compsub(void)
{
        int seof, c;
        int ret;
        char *s;
        int tt = istt();

        if ((seof = getchr()) == '\n' || seof == ' ')
                qerror();

        compile(seof);
        buffer_reset(&rhsbuf);
        s = ttgetdelim(seof);
        if (s == NULL)
                goto err;

        for (;;) {
                c = *s++;
                if (c == '\0')
                        goto err;
                if (c == '\\')
                        c = *s++ | HIGHBIT;
                if (c == '\n') {
                        if (tt)
                                c |= HIGHBIT;
                        else
                                goto err;
                }
                if (c == seof)
                        break;
                buffer_putc(&rhsbuf, c);
        }
        free(s);
        buffer_putc(&rhsbuf, '\0');

        c = getchr();
        ret = (c == 'g');
        ungetchr(ret ? '\0' : c);
        return ret;

err:
        if (s != NULL)
                free(s);
        qerror();
        /* keep compiler happy... */
        return 0;
}
