#include "ed.h"
#include <unistd.h>
#include <assert.h>
#include <string.h>


void
buffer_putc(struct buffer_t *b, int c)
{
        if (b->count + 1 >= b->size)
                qerror();
        b->base[b->count++] = c;
}

/* Copy string to current place in buffer. */
void
buffer_strapp(struct buffer_t *dst, const char *s)
{
        int c;
        do {
                c = *s++;
                buffer_putc(dst, c);
        } while (c != '\0');
}

/*
 * Append buffer src to end of dst, assuming src has not been
 * reset.
 */
void
buffer_append(struct buffer_t *dst, struct buffer_t *src)
{
        int i;
        for (i = 0; i < src->count; i++)
                buffer_putc(dst, src->base[i]);
}

void
buffer_strcpy(struct buffer_t *dst, struct buffer_t *src)
{
        int i;
        /*
         * There are quicker methods than this, but this is ed;
         * it's not a DAW or a video game.
         */
        for (i = 0; i < src->size; i++) {
                int c = src->base[i];
                buffer_putc(dst, c);
                if (c == '\0')
                        return;
        }
        /* Do not call this if we don't know if src is nul-term'd */
        assert(false);
}

void
buffer_memapp(struct buffer_t *dst, char *start, char *end)
{
        char *p = start;
        while (p < end)
                buffer_putc(dst, *p++);
}
