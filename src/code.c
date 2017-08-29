#include "ed.h"
#include <string.h>

enum {
       ESIZE = 128,
       NBRA = 5,

       /* Codes */
       CBRA = 1,
       CCHR = 2,
       CDOT = 4,
       CCL = 6,
       NCCL = 8,
       CDOL = 10,
       C_EOF = 11,
       CKET = 12,
       CBACK = 14,
       STAR = 01,
};

/* Backrefs */
static struct bralist_t bralist[NBRA];
static int nbra;

static int circfl;
static char expbuf[ESIZE + 4];
static char *locs;

/* Length of a backref in bralist, by index */
static size_t
bralen(int idx)
{
        return bralist[idx].end - bralist[idx].start;
}

static void
bralist_clear(void)
{
        int i;
        for (i = 0; i < NBRA; i++) {
                bralist[i].start = NULL;
                bralist[i].end = NULL;
        }
}

static int
cclass(char *set, char c, int af)
{
        int n;

        if (c == '\0')
                return 0;
        n = *set++;
        while (--n) {
                if (*set++ == c)
                        return af;
        }
        return !af;
}

static int
backref(int i, char *lp)
{
        char *bp;
        struct bralist_t *b = &bralist[i];

        bp = b->start;
        while (*bp++ == *lp++) {
                if (bp >= b->end)
                        return 1;
        }
        return 0;
}

/*
 * XXX: Saving pointers into lp and returning them later
 * with get_backref() et al.
 * This assumes calling code de-references stuff during correct
 * part of program flow
 */
static int
advance(char *lp, char *ep)
{
        char *curlp;
        int i;

        for (;;) {
                switch (*ep++) {

                case CCHR:
                        if (*ep++ == *lp++)
                                continue;
                        return 0;

                case CDOT:
                        if (*lp++)
                                continue;
                        return 0;

                case CDOL:
                        if (*lp == '\0')
                                continue;
                        return 0;

                case C_EOF:
                        loc2 = lp;
                        return 1;

                case CCL:
                        if (cclass(ep, *lp++, 1)) {
                                ep += *ep;
                                continue;
                        }
                        return 0;

                case NCCL:
                        if (cclass(ep, *lp++, 0)) {
                                ep += *ep;
                                continue;
                        }
                        return 0;

                case CBRA:
                        bralist[(int)(*ep++)].start = lp;
                        continue;

                case CKET:
                        bralist[(int)(*ep++)].end = lp;
                        continue;

                case CBACK:
                        if (bralist[i = *ep++].end == NULL)
                                qerror();
                        if (backref(i, lp)) {
                                lp += bralen(i);
                                continue;
                        }
                        return 0;

                case CBACK|STAR:
                        if (bralist[i = *ep++].end == NULL)
                                qerror();
                        curlp = lp;
                        while (backref(i, lp))
                                lp += bralen(i);
                        while (lp >= curlp) {
                                if (advance(lp, ep))
                                        return 1;
                                lp -= bralen(i);
                        }
                        continue;

                case CDOT|STAR:
                        curlp = lp;
                        while (*lp++)
                                ;
                        goto star;

                case CCHR|STAR:
                        curlp = lp;
                        while (*lp++ == *ep)
                                ;
                        ep++;
                        goto star;

                case CCL|STAR:
                case NCCL|STAR:
                        curlp = lp;
                        while (cclass(ep, *lp++, ep[-1] == (CCL | STAR)))
                                ;
                        ep += *ep;
                        goto star;

                star:
                        do {
                                lp--;
                                if (lp == locs)
                                        break;
                                if (advance(lp, ep))
                                        return 1;
                        } while (lp > curlp);
                        return 0;

                default:
                        qerror();
                }
        }
}

int
execute(int gf, int *addr)
{
        char *p1, *p2, c;

        bralist_clear();

        if (gf) {
                if (circfl)
                        return 0;
                strcpy(linebuf, genbuf);
                p1 = loc2;
                locs = loc2;
        } else {
                if (addr == zero)
                        return 0;
                p1 = ed_getline(*addr);
                locs = NULL;
        }
        p2 = expbuf;
        if (circfl) {
                loc1 = p1;
                return advance(p1, p2);
        }

        /* fast check for first character */
        if (*p2 == CCHR) {
                c = p2[1];
                do {
                        if (*p1 != c)
                                continue;
                        if (advance(p1, p2)) {
                                loc1 = p1;
                                return 1;
                        }
                } while (*p1++);
                return 0;
        }

        /* regular algorithm */
        do {
                if (advance(p1, p2)) {
                        loc1 = p1;
                        return 1;
                }
        } while (*p1++);
        return 0;
}

void
compile(int aeof)
{
        int eof, c, c2;
        char *ep;
        char *lastep;
        char bracket[NBRA], *bracketp;
        int cclcnt;

        ep = expbuf;
        eof = aeof;
        bracketp = bracket;
        if ((c = getchr()) == eof) {
                if (*ep == '\0')
                        qerror();
                return;
        }

        circfl = 0;
        nbra = 0;
        if (c == '^') {
                c = getchr();
                circfl++;
        }

        ungetchr(c);
        lastep = NULL;

        for (;;) {
                if (ep >= &expbuf[ESIZE])
                        goto cerror;
                c = getchr();
                if (c == eof) {
                        if (bracketp != bracket)
                                goto cerror;
                        *ep++ = C_EOF;
                        return;
                }
                if (c != '*')
                        lastep = ep;

                switch (c) {

                case '\\':
                        if ((c = getchr()) == '(') {
                                if (nbra >= NBRA)
                                        goto cerror;
                                *bracketp++ = nbra;
                                *ep++ = CBRA;
                                *ep++ = nbra++;
                                continue;
                        }
                        if (c == ')') {
                                if (bracketp <= bracket)
                                        goto cerror;
                                *ep++ = CKET;
                                *ep++ = *--bracketp;
                                continue;
                        }
                        if (c >= '1' && c < '1' + NBRA) {
                                *ep++ = CBACK;
                                *ep++ = c - '1';
                                continue;
                        }
                        *ep++ = CCHR;
                        if (c == '\n')
                                goto cerror;
                        *ep++ = c;
                        continue;

                case '.':
                        *ep++ = CDOT;
                        continue;

                case '\n':
                        goto cerror;

                case '*':
                        if (lastep == NULL || *lastep == CBRA || *lastep == CKET)
                                goto defchar;
                        *lastep |= STAR;
                        continue;

                case '$':
                        c2 = getchr();
                        ungetchr(c2);
                        if (c2 != eof) {
                                goto defchar;
                        }
                        *ep++ = CDOL;
                        continue;

                case '[':
                        *ep++ = CCL;
                        *ep++ = '\0';
                        cclcnt = 1;
                        if ((c = getchr()) == '^') {
                                c = getchr();
                                ep[-2] = NCCL;
                        }
                        do {
                                if (c == '\n')
                                        goto cerror;
                                if (c == '-' && ep[-1] != '\0') {
                                        if ((c = getchr()) == ']') {
                                                *ep++ = '-';
                                                cclcnt++;
                                                break;
                                        }
                                        while (ep[-1] < c) {
                                                *ep = ep[-1] + 1;
                                                ep++;
                                                cclcnt++;
                                                if (ep >= &expbuf[ESIZE])
                                                        goto cerror;
                                        }
                                }
                                *ep++ = c;
                                cclcnt++;
                                if (ep >= &expbuf[ESIZE])
                                        goto cerror;
                        } while ((c = getchr()) != ']');
                        lastep[1] = cclcnt;
                        continue;

                defchar:
                default:
                        *ep++ = CCHR;
                        *ep++ = c;
                }
        }
   cerror:
        expbuf[0] = '\0';
        nbra = 0;
        qerror();
}

struct bralist_t *
get_backref(int cidx)
{
        if (!!(cidx & 0200)) {
                cidx &= 0177;
                if (cidx >= '1' && cidx < nbra + '1')
                        return &bralist[cidx - '1'];
        }
        return NULL;
}
