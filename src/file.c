#include "ed.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

static char *perm = NULL;
static int io = -1;

struct buffer_t filebuf = BUFFER_INITIAL();

static char *
file_filbuf(struct buffer_t *gb)
{
        char *fp;
        buffer_guarantee_size(gb, LBSIZE);
        gb->count = read(io, gb->base, LBSIZE);
        if (gb->count <= 0)
                return NULL;

        fp = gb->base;
        while (fp < buffer_ptr(gb)) {
                if (*fp++ & HIGHBIT) {
                        if (options.kflag) {
                                crblock(perm, gb->base, gb->count + 1, count);
                        }
                        break;
                }
        }
        return gb->base;
}

void
file_reset_state(void)
{
        buffer_reset(&filebuf);
}

/* Fills lb; Returns 0 or EOF */
int
file_next_line(struct buffer_t *lb)
{
        int c;
        struct buffer_t *gb = &filebuf;

        buffer_reset(lb);
        do {
                c = buffer_getc(gb);
                if (c == EOF) {
                        buffer_reset(gb);
                        if (file_filbuf(gb) == NULL)
                                return EOF;

                        c = buffer_getc(gb);
                        assert(c != EOF);
                }
                if (c == '\0')
                        continue;
                if (!!(c & HIGHBIT))
                        qerror();

                buffer_putc(lb, c == '\n' ? '\0' : c);
                count++;
        } while (c != '\n');
        return 0;
}

static void
file_flushbuf(char *buf, size_t size)
{
        if (options.kflag)
                crblock(perm, buf, size, count - size);
        if (write(io, buf, size) != size) {
                error("WRITE ERROR");
        }
}

void
putfile(int *a1, int *a2)
{
        char *lp;
        struct buffer_t lb = BUFFER_INITIAL();
        struct buffer_t gb = BUFFER_INITIAL();

        assert(a1 <= a2);

        do {
                lp = tempf_getline(*a1++, &lb);
                for (;;) {
                        int c;
                        /* XXX: This could grow the buffer very large. */
                        if (buffer_rem(&gb) <= 0) {
                                file_flushbuf(gb.base, gb.count);
                                buffer_reset(&gb);
                        }
                        count++;
                        c = *lp++;
                        buffer_putc(&gb, c == '\0' ? '\n' : c);
                        if (c == '\0')
                                break;
                }
        } while (a1 <= a2);
        file_flushbuf(gb.base, gb.count);

        buffer_free(&lb);
        buffer_free(&gb);
}

void
closefile(void)
{
        if (io >= 0) {
                close(io);
                io = -1;
        }
}

int
openfile(const char *nm, int type, int wrap)
{
        if (type == IOMREAD) {
                io = open(nm, O_RDONLY);
        } else if (type == IOMWRITE) {
                if (!!wrap)
                        return 0;

                if (((io = open(nm, O_WRONLY)) == -1)
                    || ((lseek(io, 0L, SEEK_SET)) == -1)) {
                        io = open(nm, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                }
        } else {
                io = open(nm, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        }
        return io;
}

void
file_initkey(void)
{
        perm = getkey(NULL, perm);
}
