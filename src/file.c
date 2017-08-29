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

int
getfile(void)
{
        int c;
        char *lp, *fp;

        lp = linebuf;
        fp = nextip;
        do {
                if (--ninbuf < 0) {
                        if ((ninbuf = read(io, genbuf, LBSIZE) - 1) < 0)
                                return EOF;
                        fp = genbuf;
                        while (fp < &genbuf[ninbuf]) {
                                if (*fp++ & 0200) {
                                        if (options.kflag)
                                                crblock(perm, genbuf, ninbuf + 1, count);
                                        break;
                                }
                        }
                        fp = genbuf;
                }
                c = *fp++;
                if (c == '\0')
                        continue;
                if (!!(c & 0200))
                        error("", true);

                lp = linebuf_putc(lp, c);
                count++;
        } while (c != '\n');
        *--lp = 0;
        nextip = fp;
        return 0;
}

void
putfile(int *a1, int *a2)
{
        int n;
        char *fp, *lp;
        int nib;

        assert(a1 < a2);

        nib = LBSIZE;
        fp = genbuf;
        do {
                lp = tempf_to_line(*a1++);
                for (;;) {
                        if (--nib < 0) {
                                n = fp - genbuf;
                                if (options.kflag)
                                        crblock(perm, genbuf, n, count - n);
                                if (write(io, genbuf, n) != n) {
                                        putstr(WRERR);
                                        qerror();
                                }
                                nib = LBSIZE - 1;
                                fp = genbuf;
                        }
                        count++;
                        if ((*fp++ = *lp++) == '\0') {
                                fp[-1] = '\n';
                                break;
                        }
                }
        } while (a1 <= a2);
        n = fp - genbuf;
        if (options.kflag)
                crblock(perm, genbuf, n, count - n);
        if (write(io, genbuf, n) != n) {
                putstr(WRERR);
                qerror();
        }
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
