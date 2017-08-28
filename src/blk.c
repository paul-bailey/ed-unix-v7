#include "ed.h"
#include <unistd.h>

enum {
        BLKSIZ = 512,
};
static char ibuff[BLKSIZ];
static char obuff[BLKSIZ];
static int iblock = -1;
static int oblock = -1;
static int ichanged;

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
        iblock = -1;
        oblock = -1;
        ichanged = 0;
}
