#include "ed.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

static const char WRERR[] = "WRITE ERROR";
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
                        error("", true);

                buffer_putc(lb, c);
                count++;
        } while (c != '\n');
        *(buffer_ptr(lb) - 1) = '\0';
        return 0;
}

static void
file_flushbuf(char *buf, size_t size)
{
        if (options.kflag)
                crblock(perm, buf, size, count - size);
        if (write(io, buf, size) != size) {
                putstr(WRERR);
                qerror();
        }
}

void
putfile(int *a1, int *a2)
{
        char *lp;
        struct buffer_t lb = BUFFER_INITIAL();
        struct buffer_t gb = BUFFER_INITIAL();

        assert(a1 < a2);

        do {
                lp = tempf_to_line(*a1++, &lb);
                for (;;) {
                        int c;
                        /* XXX: This could grow the buffer very large. */
                        if (buffer_rem(&gb) <= 0) {
                                file_flushbuf(gb.base, gb.count);
                                buffer_reset(&gb);
                        }
                        count++;
                        c = *lp++;
                        buffer_putc(&gb, c);
                        if (c == '\0') {
                                *(buffer_ptr(&gb) - 1) = '\n';
                                break;
                        }
                }
        } while (a1 <= a2);
        file_flushbuf(gb.base, gb.count);

        if (lb.base)
                free(lb.base);
        if (gb.base)
                free(gb.base);
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
                io = open(nm, 0);
        } else if (type == IOMWRITE) {
                if (!!wrap)
                        return 0;

                if (((io = open(nm, 1)) == -1)
                    || ((lseek(io, 0L, 2)) == -1)) {
                        io = creat(nm, 0666);
                }
        } else {
                /* type = IOMHUP */
                io = creat(nm, 0666);
        }
        return io;
}

void
file_initkey(void)
{
        perm = getkey(NULL, perm);
}
