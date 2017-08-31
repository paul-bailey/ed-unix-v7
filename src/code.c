#include "ed.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

enum {
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
static struct buffer_t expbuf = BUFFER_INITIAL();
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
execute(int *addr, int *zaddr, struct buffer_t *lb)
{
        char *p1, *p2, c;

        bralist_clear();

        if (addr == NULL) {
                if (circfl)
                        return 0;
                buffer_strcpy(lb, &genbuf);
                assert(loc2 >= lb->base);
                assert(loc2 < &lb->base[lb->size]);
                p1 = loc2;
                locs = loc2;
        } else {
                if (addr == zaddr)
                        return 0;
                p1 = tempf_to_line(*addr, lb);
                locs = NULL;
        }
        p2 = expbuf.base;
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
        } while (*p1++ != '\0');
        return 0;
}

int
lastexp(void)
{
        return *(buffer_ptr(&expbuf) - 1);
}

/*
 * Helper to compile().
 * *pps points to the line after '['.
 */
static int
compile_bracket(char **pps, int eof)
{
        int cclcnt = 1;
        int c;
        char *s = *pps;

        buffer_putc(&expbuf, CCL);
        buffer_putc(&expbuf, '\0');
        if ((c = *s++) == '^') {
                c = *s++;
                *(buffer_ptr(&expbuf) - 2) = NCCL;
        }
        do {
                if (c == '\n' || c == '\0')
                        return -1;
                if (c == '-' && lastexp() != '\0') {
                        int c2;
                        if ((c = *s++) == ']') {
                                buffer_putc(&expbuf, '-');
                                cclcnt++;
                                break;
                        } else if (c == '\0') {
                                return -1;
                        }
                        while ((c2 = lastexp()) < c) {
                                if (expbuf.count >= expbuf.size)
                                        return -1;
                                buffer_putc(&expbuf, c2 + 1);
                                cclcnt++;
                        }
                }
                if (expbuf.count >= expbuf.size)
                        return -1;
                buffer_putc(&expbuf, c);
                cclcnt++;
        } while ((c = *s++) != ']');
        *pps = s;
        return cclcnt;
}

void
compile(int aeof)
{
        int eof, c;
        char *lastep;
        char bracket[NBRA], *bracketp;
        char *tbuf;
        char *s;
        size_t len;

        buffer_reset(&expbuf);
        eof = aeof;
        bracketp = bracket;
        if ((c = getchr()) == eof) {
                if (expbuf.base[0] == '\0')
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

        s = tbuf = ttgetdelim(eof);
        if (s == NULL
            || (len = strlen(s) + 1) >= expbuf.size
            || s[len - 1] != aeof) {
                goto cerror;
        }

        for (;;) {
                c = *s++;
                if (c == '\0')
                        goto cerror;
                if (c == eof) {
                        if (bracketp != bracket)
                                goto cerror;
                        buffer_putc(&expbuf, C_EOF);
                        free(tbuf);
                        return;
                }
                if (c != '*')
                        lastep = buffer_ptr(&expbuf);

                switch (c) {

                case '\\':
                        if ((c = *s++) == '(') {
                                if (nbra >= NBRA)
                                        goto cerror;
                                *bracketp++ = nbra;
                                buffer_putc(&expbuf, CBRA);
                                buffer_putc(&expbuf, nbra++);
                                continue;
                        }
                        if (c == ')') {
                                if (bracketp <= bracket)
                                        goto cerror;
                                buffer_putc(&expbuf, CKET);
                                buffer_putc(&expbuf, *--bracketp);
                                continue;
                        }
                        if (c >= '1' && c < '1' + NBRA) {
                                buffer_putc(&expbuf, CBACK);
                                buffer_putc(&expbuf, c - '1');
                                continue;
                        }
                        buffer_putc(&expbuf, CCHR);
                        if (c == '\n')
                                goto cerror;
                        buffer_putc(&expbuf, c);
                        continue;

                case '.':
                        buffer_putc(&expbuf, CDOT);
                        continue;

                case '\n':
                        goto cerror;

                case '*':
                        if (lastep == NULL || *lastep == CBRA || *lastep == CKET)
                                goto defchar;
                        *lastep |= STAR;
                        continue;

                case '$':
                    {
                        int c2 = *s++;
                        --s;
                        if (c2 != eof)
                                goto defchar;
                        buffer_putc(&expbuf, CDOL);
                        continue;
                    }

                case '[':
                        lastep[1] = compile_bracket(&s, eof);
                        if (lastep[1] < 0)
                                goto cerror;
                        continue;

                defchar:
                default:
                        buffer_putc(&expbuf, CCHR);
                        buffer_putc(&expbuf, c);
                }
        }
   cerror:
        free(tbuf);
        expbuf.base[0] = '\0';
        nbra = 0;
        qerror();
}

struct bralist_t *
get_backref(int cidx)
{
        if (!!(cidx & HIGHBIT)) {
                cidx = toascii(cidx);
                if (cidx >= '1' && cidx < nbra + '1')
                        return &bralist[cidx - '1'];
        }
        return NULL;
}
