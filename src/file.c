#include "ed.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

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
                if (!!(c & 0200) || lp >= &linebuf[LBSIZE]) {
                        lastc = '\n';
                        error(Q);
                }
                *lp++ = c;
                count++;
        } while (c != '\n');
        *--lp = 0;
        nextip = fp;
        return 0;
}

void
putfile(void)
{
        int *a1, n;
        char *fp, *lp;
        int nib;

        nib = LBSIZE;
        fp = genbuf;
        a1 = addr1;
        do {
                lp = ed_getline(*a1++);
                for (;;) {
                        if (--nib < 0) {
                                n = fp - genbuf;
                                if (options.kflag)
                                        crblock(perm, genbuf, n, count - n);
                                if (write(io, genbuf, n) != n) {
                                        putstr(WRERR);
                                        error(Q);
                                }
                                nib = 511;
                                fp = genbuf;
                        }
                        count++;
                        if ((*fp++ = *lp++) == 0) {
                                fp[-1] = '\n';
                                break;
                        }
                }
        } while (a1 <= addr2);
        n = fp - genbuf;
        if (options.kflag)
                crblock(perm, genbuf, n, count - n);
        if (write(io, genbuf, n) != n) {
                putstr(WRERR);
                error(Q);
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
