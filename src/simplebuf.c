#include "ed.h"
#include <unistd.h>
#include <assert.h>

static char *
putc_helper(char *sp, int c, char *top)
{
        if (sp >= top)
                qerror();
        *sp++ = c;
        return sp;
}

char *
linebuf_putc(char *sp, int c)
{
        assert(sp >= &linebuf[0] && sp < &linebuf[LBSIZE]);
        return putc_helper(sp, c, &linebuf[LBSIZE]);
}

char *
genbuf_putc(char *sp, int c)
{
        assert(sp >= &genbuf[0] && sp < &genbuf[LBSIZE]);
        return putc_helper(sp, c, &genbuf[LBSIZE]);
}

/* Add string to genbuf, return pointer to copied terminating nulchar */
char *
genbuf_puts(char *sp, char *src)
{
        /* TODO: Why -2? */
        int c;
        do {
                sp = genbuf_putc(sp, c = *src++);
        } while (c != '\0');
        return sp;
}

char *
genbuf_putm(char *sp, char *start, char *end)
{
        char *p = start;
        while (p < end) {
                sp = genbuf_putc(sp, *p++);
        }
        return sp;
}
