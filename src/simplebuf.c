#include "ed.h"
#include <unistd.h>

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
