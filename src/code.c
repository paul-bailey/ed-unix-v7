#include "ed.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

enum {
#if 1
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
#else
        /* Or'd with CBACK, CDOT, CCHR, CCL, or NCCL */
        STAR = 01,

        /* Since STAR is not a solo act, multi-task its number */
        CBRA = STAR,

        /* Keep these even. They may be OR'd with STAR */
        CCHR = 1 << 1,
        CDOT = 2 << 1,
        CCL = 3 << 1,
        NCCL = 4 << 1,
        CBACK = 5 << 1,

        /*
         * Keep these even, too, so they aren't confused with
         * one of the above OR'd with STAR
         */
        CKET = 6 << 1,
        CDOL = 7 << 1,
        C_EOF = 8 << 1,
#endif
};

/*
 * TODO: These should all technically be part of the struct code_t state
 * machine.
 */
/* Backrefs */

/* Length of a backref in bralist, by index */
static size_t
bralen(struct code_t *cd, int idx)
{
        return cd->bralist[idx].end - cd->bralist[idx].start;
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
backref(struct code_t *cd, int i, char *lp)
{
        char *bp;
        struct bralist_t *b = &cd->bralist[i];

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
advance(char *lp, char *ep, struct code_t *cd)
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
                        cd->loc2 = lp;
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
                        cd->bralist[(int)(*ep++)].start = lp;
                        continue;

                case CKET:
                        cd->bralist[(int)(*ep++)].end = lp;
                        continue;

                case CBACK:
                        if (cd->bralist[i = *ep++].end == NULL)
                                qerror();
                        if (backref(cd, i, lp)) {
                                lp += bralen(cd, i);
                                continue;
                        }
                        return 0;

                case CBACK|STAR:
                        if (cd->bralist[i = *ep++].end == NULL)
                                qerror();
                        curlp = lp;
                        while (backref(cd, i, lp))
                                lp += bralen(cd, i);
                        while (lp >= curlp) {
                                if (advance(lp, ep, cd))
                                        return 1;
                                lp -= bralen(cd, i);
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
                                if (lp == cd->locs)
                                        break;
                                if (advance(lp, ep, cd))
                                        return 1;
                        } while (lp > curlp);
                        return 0;

                default:
                        qerror();
                }
        }
}

static int
execute_helper(char *p1, struct code_t *cd)
{
        char *p2, c;
        int i;

        /* Clear backrefs */
        for (i = 0; i < NBRA; i++) {
                cd->bralist[i].start = NULL;
                cd->bralist[i].end = NULL;
        }

        assert(p1 != NULL);
        p2 = cd->expbuf.base;
        if (cd->circfl) {
                cd->loc1 = p1;
                return advance(p1, p2, cd);
        }

        /* fast check for first character */
        if (*p2 == CCHR) {
                c = p2[1];
                do {
                        if (*p1 != c)
                                continue;
                        if (advance(p1, p2, cd)) {
                                cd->loc1 = p1;
                                return 1;
                        }
                } while (*p1++);
                return 0;
        }

        /* regular algorithm */
        do {
                if (advance(p1, p2, cd)) {
                        cd->loc1 = p1;
                        return 1;
                }
        } while (*p1++ != '\0');
        return 0;
}

int
execute(int *addr, struct code_t *cd)
{
        assert(addr != NULL);
        if (addr == addrs.zero)
                return 0;
        cd->locs = NULL;
        return execute_helper(tempf_getline(*addr, &(cd->lb)), cd);
}

int
subexecute(struct buffer_t *gb, struct code_t *cd)
{
        if (cd->circfl)
                return 0;
        buffer_strcpy(&cd->lb, gb);
        assert(cd->loc2 >= cd->lb.base);
        assert(cd->loc2 < &cd->lb.base[cd->lb.size]);
        cd->locs = cd->loc2;
        return execute_helper(cd->loc2, cd);
}

static int
lastexp(struct code_t *cd)
{
        return *(buffer_ptr(&cd->expbuf) - 1);
}

static void
bufinit(struct buffer_t *b)
{
        b->base = NULL;
        b->count = b->size = b->tail = 0;
}

/*
 * Helper to compile().
 * *pps points to the line after '['.
 */
static int
compile_bracket(struct code_t *cd, char **pps, int eof)
{
        int cclcnt = 1;
        int c;
        char *s = *pps;

        buffer_putc(&cd->expbuf, CCL);
        buffer_putc(&cd->expbuf, '\0');
        if ((c = *s++) == '^') {
                c = *s++;
                *(buffer_ptr(&cd->expbuf) - 2) = NCCL;
        }
        do {
                if (c == '\n' || c == '\0')
                        return -1;
                if (c == '-' && lastexp(cd) != '\0') {
                        int c2;
                        if ((c = *s++) == ']') {
                                buffer_putc(&cd->expbuf, '-');
                                cclcnt++;
                                break;
                        } else if (c == '\0') {
                                return -1;
                        }
                        while ((c2 = lastexp(cd)) < c) {
                                if (cd->expbuf.count >= cd->expbuf.size)
                                        return -1;
                                buffer_putc(&cd->expbuf, c2 + 1);
                                cclcnt++;
                        }
                }
                if (cd->expbuf.count >= cd->expbuf.size)
                        return -1;
                buffer_putc(&cd->expbuf, c);
                cclcnt++;
        } while ((c = *s++) != ']');
        *pps = s;
        return cclcnt;
}

static void
code_init(struct code_t *cd)
{
        /*
         * We just happen to know this also puts
         * the struct buffer_t's into their initial state.
         */
        memset(cd, 0, sizeof(*cd));
        bufinit(&cd->lb);
        bufinit(&cd->expbuf);
}

void
compile(struct code_t *cd, int aeof)
{
        int eof, c;
        char *lastep;
        char bracket[NBRA], *bracketp;
        char *tbuf;
        char *s;

        code_init(cd);

        buffer_reset(&cd->expbuf);
        eof = aeof;
        bracketp = bracket;
        if ((c = getchr()) == eof) {
                if (cd->expbuf.base[0] == '\0')
                        qerror();
                return;
        }

        cd->circfl = 0;
        cd->nbra = 0;
        if (c == '^') {
                c = getchr();
                cd->circfl++;
        }

        ungetchr(c);
        lastep = NULL;

        s = tbuf = ttgetdelim(eof);
        if (s == NULL)
                goto cerror;

        assert(s[strlen(s) - 1] == aeof);

        for (;;) {
                c = *s++;
                if (c == '\0')
                        goto cerror;
                if (c == eof) {
                        if (bracketp != bracket)
                                goto cerror;
                        buffer_putc(&cd->expbuf, C_EOF);
                        assert(tbuf != NULL);
                        free(tbuf);
                        return;
                }

                if (c != '*')
                        lastep = buffer_ptr(&cd->expbuf);

                switch (c) {

                case '\\':
                        if ((c = *s++) == '(') {
                                if (cd->nbra >= NBRA)
                                        goto cerror;
                                *bracketp++ = cd->nbra;
                                buffer_putc(&cd->expbuf, CBRA);
                                buffer_putc(&cd->expbuf, cd->nbra++);
                                continue;
                        }
                        if (c == ')') {
                                if (bracketp <= bracket)
                                        goto cerror;
                                buffer_putc(&cd->expbuf, CKET);
                                buffer_putc(&cd->expbuf, *--bracketp);
                                continue;
                        }
                        if (c >= '1' && c < '1' + NBRA) {
                                buffer_putc(&cd->expbuf, CBACK);
                                buffer_putc(&cd->expbuf, c - '1');
                                continue;
                        }
                        buffer_putc(&cd->expbuf, CCHR);
                        if (c == '\n')
                                goto cerror;
                        buffer_putc(&cd->expbuf, c);
                        continue;

                case '.':
                        buffer_putc(&cd->expbuf, CDOT);
                        continue;

                case '\n':
                        goto cerror;

                case '*':
                        /* determine if wildcard or if actual char */
                        if (lastep == NULL || *lastep == CBRA || *lastep == CKET)
                                goto defchar;
                        else
                                *lastep |= STAR;
                        continue;

                case '$':
                        /* end-of-line or actual dollar char */
                        if ((int)(*s) != eof)
                                goto defchar;
                        else
                                buffer_putc(&cd->expbuf, CDOL);
                        continue;

                case '[':
                        assert(lastep != NULL);
                        lastep[1] = compile_bracket(cd, &s, eof);
                        if (lastep[1] < 0)
                                goto cerror;
                        continue;

                defchar:
                default:
                        buffer_putc(&cd->expbuf, CCHR);
                        buffer_putc(&cd->expbuf, c);
                }
        }
   cerror:
        if (tbuf != NULL)
                free(tbuf);
        buffer_reset(&cd->expbuf);
        buffer_putc(&cd->expbuf, '\0');
        cd->nbra = 0;
        qerror();
}

struct bralist_t *
get_backref(struct code_t *cd, int cidx)
{
        if (!!(cidx & HIGHBIT)) {
                cidx = toascii(cidx);
                if (cidx >= '1' && cidx < cd->nbra + '1')
                        return &cd->bralist[cidx - '1'];
        }
        return NULL;
}
