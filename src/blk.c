#include "ed.h"
#include <unistd.h>
char ibuff[512];
char obuff[512];

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
        nleft = 512 - off;
        if (bno == iblock) {
                ichanged |= iof;
                return ibuff + off;
        }
        if (bno == oblock)
                return obuff + off;
        if (iof == READ) {
                if (ichanged) {
                        if (xtflag)
                                crblock(tperm, ibuff, 512, (long)0);
                        blkio(iblock, ibuff, write);
                }
                ichanged = 0;
                iblock = bno;
                blkio(bno, ibuff, read);
                if (xtflag)
                        crblock(tperm, ibuff, 512, (long)0);
                return ibuff + off;
        }
        if (oblock >= 0) {
                if (xtflag) {
                        p1 = obuff;
                        p2 = crbuf;
                        n = 512;
                        while (n--)
                                *p2++ = *p1++;
                        crblock(tperm, crbuf, 512, (long)0);
                        blkio(oblock, crbuf, write);
                } else
                        blkio(oblock, obuff, write);
        }
        oblock = bno;
        return obuff + off;
}

void
blkio(int b, char *buf, ssize_t (*iofcn)())
{
        lseek(tfile, (long)b << 9, 0);
        if ((*iofcn)(tfile, buf, 512) != 512) {
                error(T);
        }
}
