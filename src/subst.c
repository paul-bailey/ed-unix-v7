#include "ed.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

struct subst_t subst;

static struct buffer_t rhsbuf = BUFFER_INITIAL();

static void
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

static int
compsub(void)
{
        int seof, c;
        int ret;
        char *s, *line;
        int tt = istt();

        if ((seof = getchr()) == '\n' || seof == ' ')
                qerror();

        compile(seof);
        buffer_reset(&rhsbuf);
        s = line = ttgetdelim(seof);
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
        free(line);
        buffer_putc(&rhsbuf, '\0');

        c = getchr();
        ret = (c == 'g');
        ungetchr(ret ? '\0' : c);
        return ret;

err:
        if (line != NULL)
                free(line);
        qerror();
        /* keep compiler happy... */
        return 0;
}

/* isbuff = true if not getting from tty */
void
substitute(int isbuff)
{
        int *a, nl;
        int gsubf;
        struct code_t cd = CODE_INITIAL();

        gsubf = compsub();
        newline();
        assert(addrs.addr1 <= addrs.addr2 && addrs.addr1 != NULL);
        assert(addrs.zero != NULL);
        for (a = addrs.addr1; a <= addrs.addr2; a++) {
                int *ozero;
                if (execute(a, addrs.zero, &cd) == 0)
                        continue;

                isbuff |= 01;
                dosub(&cd);
                if (gsubf) {
                        while (*cd.loc2 != '\0') {
                                if (execute(NULL, addrs.zero, &cd) == 0)
                                        break;
                                dosub(&cd);
                        }
                }
                subst.newaddr = tempf_putline(&cd.lb);
                *a = toeven(*a);
                if (marks.any) {
                        int i;
                        for (i = 0; i < NMARKS; i++) {
                                if (marks.names[i] == *a)
                                        marks.names[i] = subst.newaddr;
                        }
                }
                subst.oldaddr = *a;
                *a = subst.newaddr;
                ozero = addrs.zero;
                nl = append(A_GETSUB, a);
                nl += addrs.zero - ozero;
                a += nl;
                addrs.addr2 += nl;
        }
        code_free(&cd);

        if (!isbuff)
                qerror();
}
