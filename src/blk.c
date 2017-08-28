#include "ed.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

enum {
        BLKSIZ = 512,
};
static char ibuff[BLKSIZ];
static char obuff[BLKSIZ];
static int iblock = -1;
static int oblock = -1;
static int ichanged;
static int tfile = -1;
static char *tfname = NULL;

static void
blkio(int b, char *buf, ssize_t (*iofcn)())
{
        lseek(tfile, (long)b * BLKSIZ, SEEK_SET);
        if ((*iofcn)(tfile, buf, BLKSIZ) != BLKSIZ) {
                error(T);
        }
}

char *
getblock(int atl, int iof)
{
        int bno, off;
        char *p1, *p2;
        int n;

        bno = (atl >> 8) & 0377;
        off = (atl << 1) & 0774;
        if (bno >= 255) {
                lastc = '\n';
                error(T);
        }
        nleft = BLKSIZ - off;
        if (bno == iblock) {
                ichanged |= iof;
                return ibuff + off;
        }
        if (bno == oblock)
                return obuff + off;
        if (iof == READ) {
                if (ichanged) {
                        if (xtflag)
                                crblock(tperm, ibuff, BLKSIZ, (long)0);
                        blkio(iblock, ibuff, write);
                }
                ichanged = 0;
                iblock = bno;
                blkio(bno, ibuff, read);
                if (xtflag)
                        crblock(tperm, ibuff, BLKSIZ, (long)0);
                return ibuff + off;
        }
        if (oblock >= 0) {
                if (xtflag) {
                        p1 = obuff;
                        p2 = crbuf;
                        n = BLKSIZ;
                        while (n--)
                                *p2++ = *p1++;
                        crblock(tperm, crbuf, BLKSIZ, (long)0);
                        blkio(oblock, crbuf, write);
                } else
                        blkio(oblock, obuff, write);
        }
        oblock = bno;
        return obuff + off;
}

void
blkinit(void)
{
        static char tmpname[] = { "/tmp/eXXXXXX\0" };

        iblock = -1;
        oblock = -1;
        ichanged = 0;
        if (tfile >= 0)
                close(tfile);
        if (tfname == NULL) {
                /* IE first call to blkinit() */
                tfname = mkdtemp(tmpname);
                /* TODO: What if error return from mkdtemp()? */
        }
        close(creat(tfname, 0600));
        tfile = open(tfname, 2);
        if (xflag) {
                xtflag = 1;
                makekey(key, tperm);
        }
}

void
blkquit(void)
{
        unlink(tfname);
}
