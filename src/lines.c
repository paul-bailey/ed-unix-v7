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

        /* Args to getblock */
        READ = 0,
        WRITE = 1,
};

static char ibuff[BLKSIZ];
static char obuff[BLKSIZ];
static char crbuf[BLKSIZ];
static char *tperm = NULL;
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
        if (iofcn(tfile, buf, BLKSIZ) != BLKSIZ)
                error("TMP");
}

static char *
getblock(int atl, int iof, int *nleft)
{
        int bno, off;

        bno = (atl >> 8);
        off = (atl & 254) << 1;
        if (bno >= 255)
                error("TMP");

        assert(nleft != NULL);
        *nleft = BLKSIZ - off;

        if (bno == iblock) {
                if (iof == WRITE)
                        ichanged = true;
                return ibuff + off;
        }
        if (bno == oblock) {
                return obuff + off;
        }

        assert(iof == READ || iof == WRITE);

        /* Not same as prev. block; need IO */
        if (iof == READ) {
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
        } else {
                /* WRITE */
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
tempf_quit(void)
{
        if (tfile >= 0)
                close(tfile);

        if (tfname != NULL)
                unlink(tfname);
}

static int tline;

int
tempf_putline(struct buffer_t *lbuf)
{
        char *bp, *lp;
        int nleft;
        int tl;

        fchange = 1;
        lp = lbuf->base;
        tl = tline;
        bp = getblock(tl, WRITE, &nleft);
        tl &= ~255;
        while ((*bp = *lp++) != '\0') {
                if (*bp++ == '\n') {
                        *--bp = '\0';
                        break;
                }
                if (--nleft == 0)
                        bp = getblock(tl += 256, WRITE, &nleft);
        }
        nleft = tline;
        /* XXX: What the hell is this! */
        tline += (((lp - lbuf->base) + 03) >> 1) & 077776;
        return nleft;
}

char *
tempf_getline(int tl, struct buffer_t *lbuf)
{
        char *bp;
        int nleft;
        int c;

        buffer_reset(lbuf);
        bp = getblock(tl, READ, &nleft);
        tl &= ~255;
        /* TODO: What if insanely long line! */
        do {
                if (nleft <= 0)
                        bp = getblock(tl += 256, READ, &nleft);
                c = *bp;
                buffer_putc(lbuf, c);
                ++bp;
                --nleft;
        } while (c != '\0');
        return lbuf->base;
}

void
tempf_init(void)
{
        static char tmpname[] = { "/tmp/eduv7_XXXXXX\0" };

        iblock = -1;
        oblock = -1;
        ichanged = 0;

        if (tfname == NULL) {
                /* IE first call to blkinit() */
                tfile = mkstemp(tmpname);
                /* TODO: What if error return from mkstemp()? */
                tfname = tmpname;
        }

        assert(tfile >= 0);
        if (options.xflag) {
                xtflag = true;
                tperm = makekey(tperm);
        }

        tline = 2;
}
