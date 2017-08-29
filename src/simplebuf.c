#include "ed.h"
#include <unistd.h>

void
simplebuf_init(struct simplebuf_t *b, int istty)
{
        b->count = 0;
        b->istty = istty;
}

void
simplebuf_putc(struct simplebuf_t *b, int c)
{
        if (b->count == LBSIZE - 1) {
                if (!b->istty)
                        qerror();
                goto flsh;
        }

        b->buf[b->count++] = c;
        if (b->istty && c == '\n')
                goto flsh;
        return;

flsh:
        b->count = 0;
        write(STDOUT_FILENO, b->buf, b->count);
}
