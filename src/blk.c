#include "ed.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

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

static int xtflag = false;

static void
blkio(int b, char *buf, ssize_t (*iofcn)())
{
        lseek(tfile, (long)b * BLKSIZ, SEEK_SET);
        if ((*iofcn)(tfile, buf, BLKSIZ) != BLKSIZ) {
                error(T);
        }
}

char *
getblock(int atl, int iof, int *nleft)
{
        int bno, off;

        bno = (atl >> 8) & 0377;
        off = (atl << 1) & 0774;
        if (bno >= 255) {
                lastc = '\n';
                error(T);
        }

        if (nleft != NULL)
                *nleft = BLKSIZ - off;

        if (bno == iblock) {
                if (iof == WRITE)
                        ichanged = true;
                return ibuff + off;
        } else if (bno == oblock) {
                return obuff + off;
        }

        /* Not same as prev. block; need IO */
        switch (iof) {
        case READ:
                if (ichanged) {
                        if (xtflag)
                                crblock(tperm, ibuff, BLKSIZ, (long)0);
                        blkio(iblock, ibuff, write);
                }
                ichanged = false;
                iblock = bno;
                blkio(bno, ibuff, read);
                if (xtflag)
                        crblock(tperm, ibuff, BLKSIZ, (long)0);
                return ibuff + off;
        default:
                assert(false);
                /* fall through, assume WRITE */
        case WRITE:
                if (oblock >= 0) {
                        if (xtflag) {
                                memcpy(crbuf, obuff, BLKSIZ);
                                crblock(tperm, crbuf, BLKSIZ, (long)0);
                                blkio(oblock, crbuf, write);
                        } else {
                                blkio(oblock, obuff, write);
                        }
                }
                oblock = bno;
                return obuff + off;
        }
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
        if (options.xflag) {
                xtflag = true;
                makekey(key, tperm);
        }
}

void
blkquit(void)
{
        unlink(tfname);
}
