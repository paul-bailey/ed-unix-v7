#include "ed.h"
#include <unistd.h>
#include <assert.h>

void
simplebuf_init(struct simplebuf_t *b, int istty)
{
        b->count = 0;
        b->istty = istty;
}

void
simplebuf_flush(struct simplebuf_t *b)
{
        if (b->istty && b->count > 0)
                write(STDOUT_FILENO, b->buf, b->count);

        b->count = 0;
}

void
simplebuf_putc(struct simplebuf_t *b, int c)
{
        if (b->count == LBSIZE - 1) {
                if (!b->istty)
                        qerror();
                simplebuf_flush(b);
        }

        b->buf[b->count++] = c;
        if (b->istty && c == '\n')
                simplebuf_flush(b);
}

/* Add string to genbuf, return pointer to copied terminating nulchar */
char *
genbuf_puts(char *sp, char *src)
{
        /* TODO: Why -2? */
        while ((*sp = *src++) != '\0') {
                if (sp++ >= &genbuf[LBSIZE - 2])
                        qerror();
        }
        return sp;
}

char *
linebuf_putc(char *sp, int c)
{
        assert(sp >= &linebuf[0] && sp < &linebuf[LBSIZE]);
        *sp++ = c;
        if (sp >= &linebuf[LBSIZE - 2])
                qerror();
        return sp;
}

char *
genbuf_putc(char *sp, int c)
{
        assert(sp >= &genbuf[0] && sp < &genbuf[LBSIZE]);
        *sp++ = c;
        if (sp >= &genbuf[LBSIZE])
                qerror();
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
