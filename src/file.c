#include "ed.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

static const char WRERR[] = "WRITE ERROR";
static char *perm = NULL;
static int io = -1;
static char *nextip;

static char *
file_filbuf()
{
        char *fp;
        genbuf.count = read(io, genbuf.base, LBSIZE);
        ninbuf = genbuf.count;
        if (genbuf.count <= 0)
                return NULL;
        fp = genbuf.base;
        while (fp < buffer_ptr(&genbuf)) {
                if (*fp++ & 0200) {
                        if (options.kflag) {
                                crblock(perm, genbuf.base,
                                        genbuf.count + 1, count);
                        }
                        break;
                }
        }
        return genbuf.base;
}

int
getfile(void)
{
        int c;
        char *fp;

        buffer_reset(&linebuf);
        fp = nextip;
        do {
                if (--ninbuf < 0) {
                        fp = file_filbuf();
                        if (!fp)
                                return EOF;
                }
                c = *fp++;
                if (c == '\0')
                        continue;
                if (!!(c & 0200))
                        error("", true);

                buffer_putc(&linebuf, c);
                count++;
        } while (c != '\n');
        *(buffer_ptr(&linebuf) - 1) = '\0';
        nextip = fp;
        return 0;
}

static void
file_flushbuf()
{
        int n;
        n = genbuf.count;
        if (options.kflag)
                crblock(perm, genbuf.base, n, count - n);
        if (write(io, genbuf.base, n) != n) {
                putstr(WRERR);
                qerror();
        }
        buffer_reset(&genbuf);
}

void
putfile(int *a1, int *a2)
{
        char *lp;

        assert(a1 < a2);

        buffer_reset(&genbuf);
        do {
                lp = tempf_to_line(*a1++, &linebuf);
                for (;;) {
                        int c;
                        if (buffer_rem(&genbuf) <= 0)
                                file_flushbuf();
                        count++;
                        c = *lp++;
                        buffer_putc(&genbuf, c);
                        if (c == '\0') {
                                *(buffer_ptr(&genbuf) - 1) = '\n';
                                break;
                        }
                }
        } while (a1 <= a2);
        file_flushbuf();
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
