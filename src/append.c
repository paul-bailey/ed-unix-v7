#include "ed.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

static int
getcopy(struct buffer_t *lb)
{
        if (addrs.addr1 > addrs.addr2)
                return EOF;
        tempf_getline(*addrs.addr1++, lb);
        return 0;
}

static int
getfile(struct buffer_t *lb)
{
        return file_next_line(lb);
}

static int
getsub(struct buffer_t *lb)
{
        char *lp = lb->base;
        char *top = &lb->base[lb->size];
        char *linebp = NULL;
        int c;

        /*
         * XXX: This is a lot to do
         * to remove the linebp
         * interdependency with tempf_getline().
         */
        while (lp < top && (c = *lp++) != '\0') {
                if (c == '\n') {
                        linebp = lp;
                        break;
                }
        }

        if (linebp == NULL)
                return EOF;

        /* TODO: Reset and buffer_strapp instead? */
        buffer_reset(lb);
        buffer_strapp(lb, linebp);
        return 0;
}

static int
tty_to_line(struct buffer_t *lb)
{
        int c;
        int gf;

        buffer_reset(lb);
        gf = !istt();
        while ((c = getchr()) != '\n') {
                if (c == EOF) {
                        if (gf)
                                ungetchr(c);
                        return c;
                }
                c = toascii(c);
                if (c == '\0')
                        continue;
                buffer_putc(lb, c);
        }
        buffer_putc(lb, '\0');
        if (lb->base[0] == '.' && lb->base[1] == '\0')
                return EOF;
        return '\0';
}

int
append(int action, int *a)
{
        int (*func)(struct buffer_t *);
        int nline, tl;
        struct buffer_t lb = BUFFER_INITIAL();

        switch (action) {
        case A_GETSUB:
                func = getsub;
                break;
        case A_GETLINE:
                func = tty_to_line;
                break;
        case A_GETFILE:
                func = getfile;
                break;
        case A_GETCOPY:
                func = getcopy;
                break;
        default:
                assert(false);
                abort();
        };

        nline = 0;
        addrs.dot = a;
        while (func(&lb) == 0) {
                int *a1, *a2, *rdot;
                if ((addrs.dol - addrs.zero) + 1 >= addrs.nlall) {
                        int *ozero = addrs.zero;
                        addrs.nlall += 512;
                        addrs.zero = realloc(addrs.zero,
                                        addrs.nlall * sizeof(*addrs.zero));
                        if (addrs.zero == NULL) {
                                addrs.zero = ozero;
                                error("MEM?", true);
                        }
                        addrs.dot += addrs.zero - ozero;
                        addrs.dol += addrs.zero - ozero;
                }
                tl = tempf_putline(&lb);
                nline++;
                a1 = ++addrs.dol;
                a2 = a1 + 1;
                rdot = ++addrs.dot;
                while (a1 > rdot)
                        *--a2 = *--a1;
                *rdot = tl;
        }
        buffer_free(&lb);
        return nline;
}
