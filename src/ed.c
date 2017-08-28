/*
 * Editor
 */

#include "ed.h"
#include <signal.h>
#include <sgtty.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>

/* Literals */
const char Q[] = "";
const char T[] = "TMP";
const char WRERR[] = "WRITE ERROR";

/* Globals (so far) */
int peekc;
int lastc;
char *globp;
int listf;
char genbuf[LBSIZE];
char linebuf[LBSIZE];
int ninbuf;
long count;
int *addr1;
int *addr2;

struct gbl_options_t options = {
        .xflag = false,
        .vflag = true,
        .kflag = false,
};

enum {
       NNAMES  = 26,
       FNSIZE = 64,
       ESIZE = 128,
       GBSIZE = 256,
       NBRA = 5,
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

static char savedfile[FNSIZE];
static char file[FNSIZE];
static char rhsbuf[LBSIZE / 2];
static char expbuf[ESIZE + 4];
static int circfl;
static int *zero;
static int *dot;
static int *dol;
static char *linebp;
static int printflag;
static void (*oldhup)(int) = SIG_ERR;
static void (*oldquit)(int) = SIG_ERR;
static int tline;
static char *loc1;
static char *loc2;
static char *locs;
static int names[NNAMES];
static int anymarks;
static struct bralist_t {
        char *start;
        char *end;
} bralist[NBRA];
static int nbra;
static int subnewa;
static int subolda;
static int fchange;
static int wrapp;
static unsigned nlall = 128;

static jmp_buf savej;

static int cclass(char *set, char c, int af);
static int backref(int i, char *lp);
static int advance(char *lp, char *ep);
static int execute(int gf, int *addr);
static void compile(int aeof);
static int getcopy(void);
static void reverse(int *a1, int *a2);
static void move(int cflag);
static void dosub(void);
static int getsub(void);
static int compsub(void);
static void substitute(int inglob);
static void join(void);
static void global(int k);
static int putline(void);
static void gdelete(void);
static void rdelete(int *ad1, int *ad2);
static void delete(void);
static void quit(int signo);
static void callunix(void);
static int append(int (*f)(void), int *a);
static int gettty(void);
static void onhup(int signo);
static void onintr(int signo);
static void exfile(void);
static void filename(int comm);
static void newline(void);
static void nonzero(void);
static void setnoaddr(void);
static void setall(void);
static void setdot(void);
static int * address(void);
static void commands(void);

/* Length of a backref in bralist, by index */
static size_t
bralen(int idx)
{
        return bralist[idx].end - bralist[idx].start;
}

/* genbuf_putc, genbuf_puts, genbuf_putm - Genbuf helpers */
static char *
genbuf_putc(char *sp, int c)
{
        assert(sp >= &genbuf[0] && sp < &genbuf[LBSIZE]);
        *sp++ = c;
        if (sp >= &genbuf[LBSIZE])
                error(Q);
        return sp;
}

static char *
genbuf_puts(char *sp, char *src)
{
        /* TODO: Why -2? */
        while ((*sp = *src++) != '\0') {
                if (sp++ >= &genbuf[LBSIZE - 2])
                        error(Q);
        }
        return sp;
}

static char *
genbuf_putm(char *sp, char *start, char *end)
{
        char *p = start;
        while (p < end) {
                sp = genbuf_putc(sp, *p++);
        }
        return sp;
}


static int *
address(void)
{
        int *a1, minus, c;
        int n, relerr;

        minus = 0;
        a1 = NULL;
        for (;;) {
                c = getchr();
                if (isdigit(c)) {
                        n = 0;
                        do {
                                n *= 10;
                                n += c - '0';
                        } while (isdigit(c = getchr()));
                        peekc = c;
                        if (a1 == NULL)
                                a1 = zero;
                        if (minus < 0)
                                n = -n;
                        a1 += n;
                        minus = 0;
                        continue;
                }
                relerr = 0;
                if (a1 || minus)
                        relerr++;
                switch (c) {
                case ' ':
                case '\t':
                        continue;

                case '+':
                        minus++;
                        if (a1 == NULL)
                                a1 = dot;
                        continue;

                case '-':
                case '^':
                        minus--;
                        if (a1 == NULL)
                                a1 = dot;
                        continue;

                case '?':
                case '/':
                        compile(c);
                        a1 = dot;
                        for (;;) {
                                if (c == '/') {
                                        a1++;
                                        if (a1 > dol)
                                                a1 = zero;
                                } else {
                                        a1--;
                                        if (a1 < zero)
                                                a1 = dol;
                                }
                                if (execute(0, a1))
                                        break;
                                if (a1 == dot)
                                        error(Q);
                        }
                        break;

                case '$':
                        a1 = dol;
                        break;

                case '.':
                        a1 = dot;
                        break;

                case '\'':
                        if (!islower(c = getchr()))
                                error(Q);
                        for (a1 = zero; a1 <= dol; a1++)
                                if (names[c - 'a'] == (*a1 & ~01))
                                        break;
                        break;

                default:
                        peekc = c;
                        if (a1 == NULL)
                                return NULL;
                        a1 += minus;
                        if (a1 < zero || a1 > dol)
                                error(Q);
                        return a1;
                }
                if (relerr)
                        error(Q);
        }
}

static void
setdot(void)
{
        if (addr2 == NULL)
                addr1 = addr2 = dot;
        if (addr1 > addr2)
                error(Q);
}

static void
setall(void)
{
        if (addr2 == NULL) {
                addr1 = zero + 1;
                addr2 = dol;
                if (dol == zero)
                        addr1 = zero;
        }
        setdot();
}

static void
setnoaddr(void)
{
        if (addr2)
                error(Q);
}

static void
nonzero(void)
{
        if (addr1 <= zero || addr2 > dol)
                error(Q);
}

static void
newline(void)
{
        int c;

        if ((c = getchr()) == '\n')
                return;
        if (c == 'p' || c == 'l') {
                printflag++;
                if (c == 'l')
                        listf++;
                if (getchr() == '\n')
                        return;
        }
        error(Q);
}

static void
filename(int comm)
{
        char *p1;
        int c;

        count = 0;
        c = getchr();
        if (c == '\n' || c == EOF) {
                if (*savedfile == '\0' && comm != 'f')
                        error(Q);
                strcpy(file, savedfile);
                return;
        }
        if (c != ' ')
                error(Q);
        while ((c = getchr()) == ' ')
                ;
        if (c == '\n')
                error(Q);
        p1 = file;
        do {
                *p1++ = c;
                if (c == ' ' || c == EOF)
                        error(Q);
        } while ((c = getchr()) != '\n');
        *p1++ = '\0';
        if (savedfile[0] == '\0' || comm == 'e' || comm == 'f')
                strcpy(savedfile, file);
}

static void
exfile(void)
{
        closefile();
        if (options.vflag) {
                putd(count);
                putchr('\n');
        }
}

static void
onintr(int signo)
{
        signal(SIGINT, onintr);
        putchr('\n');
        lastc = '\n';
        error(Q);
}

static void
onhup(int signo)
{
        int fd;
        signal(SIGINT, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
        if (dol > zero) {
                addr1 = zero + 1;
                addr2 = dol;
                fd = openfile("ed.hup", IOMCREAT, 0);
                if (fd > 0)
                        putfile();
        }
        fchange = 0;
        quit(signo);
}

void
error(const char *s)
{
        int c;

        wrapp = 0;
        listf = 0;
        putchr('?');
        putstr(s);
        count = 0;
        lseek(STDIN_FILENO, (long)0, SEEK_END);
        printflag = 0;
        if (globp)
                lastc = '\n';
        globp = NULL;
        peekc = lastc;
        if (lastc)
                while ((c = getchr()) != '\n' && c != EOF)
                        ;
        closefile();
        longjmp(savej, 1);
}

static int
gettty(void)
{
        int c;
        char *gf;
        char *p;

        p = linebuf;
        gf = globp;
        while ((c = getchr()) != '\n') {
                if (c == EOF) {
                        if (gf)
                                peekc = c;
                        return c;
                }
                if ((c &= 0177) == '\0')
                        continue;
                *p++ = c;
                if (p >= &linebuf[LBSIZE - 2])
                        error(Q);
        }
        *p++ = '\0';
        if (linebuf[0] == '.' && linebuf[1] == '\0')
                return EOF;
        return '\0';
}

static int
append(int (*f)(void), int *a)
{
        int *a1, *a2, *rdot;
        int nline, tl;

        nline = 0;
        dot = a;
        while ((*f)() == 0) {
                if ((dol - zero) + 1 >= nlall) {
                        int *ozero = zero;
                        nlall += 512;
                        free((char *)zero);
                        zero = realloc(zero, nlall * sizeof(int));
                        if (zero == NULL) {
                                lastc = '\n';
                                zero = ozero;
                                error("MEM?");
                        }
                        dot += zero - ozero;
                        dol += zero - ozero;
                }
                tl = putline();
                nline++;
                a1 = ++dol;
                a2 = a1 + 1;
                rdot = ++dot;
                while (a1 > rdot)
                        *--a2 = *--a1;
                *rdot = tl;
        }
        return nline;
}

static void
callunix(void)
{
        void (*savint)(int);
        pid_t pid, rpid;
        int retcode;

        setnoaddr();
        if ((pid = fork()) == (pid_t)0) {
                signal(SIGHUP, oldhup);
                signal(SIGQUIT, oldquit);
                execl("/bin/sh", "sh", "-t", (char *)NULL);
                exit(EXIT_FAILURE);
        }
        savint = signal(SIGINT, SIG_IGN);
        while ((rpid = wait(&retcode)) != pid && rpid != (pid_t)-1)
                ;
        signal(SIGINT, savint);
        putstr("!");
}

static void
quit(int signo)
{
        if (options.vflag && fchange && dol != zero) {
                fchange = 0;
                error(Q);
        }
        blkquit();
        exit(EXIT_SUCCESS);
}

static void
delete(void)
{
        setdot();
        newline();
        nonzero();
        rdelete(addr1, addr2);
}

static void
rdelete(int *ad1, int *ad2)
{
        int *a1, *a2, *a3;

        a1 = ad1;
        a2 = ad2 + 1;
        a3 = dol;
        dol -= a2 - a1;
        do {
                *a1++ = *a2++;
        } while (a2 <= a3);
        a1 = ad1;
        if (a1 > dol)
                a1 = dol;
        dot = a1;
        fchange = 1;
}

static void
gdelete(void)
{
        int *a1, *a2, *a3;

        a3 = dol;
        for (a1 = zero + 1; (*a1 & 01) == 0; a1++)
                if (a1 >= a3)
                        return;
        for (a2 = a1 + 1; a2 <= a3;) {
                if (*a2 & 01) {
                        a2++;
                        dot = a1;
                } else {
                        *a1++ = *a2++;
                }
        }
        dol = a1 - 1;
        if (dot > dol)
                dot = dol;
        fchange = 1;
}

char *
ed_getline(int tl)
{
        char *bp, *lp;
        int nl;

        lp = linebuf;
        bp = getblock(tl, READ, &nl);
        tl &= ~0377;
        /* TODO: What if insanely long line! */
        while (!!(*lp++ = *bp++)) {
                if (--nl == 0)
                        bp = getblock(tl += 0400, READ, &nl);
        }
        return linebuf;
}

static int
putline(void)
{
        char *bp, *lp;
        int nl;
        int tl;
        int c;

        fchange = 1;
        lp = linebuf;
        tl = tline;
        bp = getblock(tl, WRITE, &nl);
        tl &= ~0377;
        while ((c = *lp++) != '\0') {
                if (c == '\n') {
                        *bp = '\0';
                        linebp = lp;
                        break;
                }
                *bp++ = c;
                if (--nl == 0)
                        bp = getblock(tl += 0400, WRITE, &nl);
        }
        nl = tline;
        /* XXX: What the hell is this! */
        tline += (((lp - linebuf) + 03) >> 1) & 077776;
        return nl;
}

static void
global(int k)
{
        char *gp;
        int c;
        int *a1;
        char globuf[GBSIZE];

        if (globp)
                error(Q);
        setall();
        nonzero();
        if ((c = getchr()) == '\n')
                error(Q);
        compile(c);
        gp = globuf;
        while ((c = getchr()) != '\n') {
                if (c == EOF)
                        error(Q);
                if (c == '\\') {
                        c = getchr();
                        if (c != '\n')
                                *gp++ = '\\';
                }
                *gp++ = c;
                if (gp >= &globuf[GBSIZE - 2])
                        error(Q);
        }
        *gp++ = '\n';
        *gp++ = '\0';
        for (a1 = zero; a1 <= dol; a1++) {
                *a1 &= ~01;
                if (a1 >= addr1 && a1 <= addr2 && execute(0, a1) == k)
                        *a1 |= 01;
        }
        /*
         * Special case: g/.../d (avoid n^2 algorithm)
         */
        if (globuf[0]=='d' && globuf[1]=='\n' && globuf[2]=='\0') {
                gdelete();
                return;
        }
        for (a1 = zero; a1 <= dol; a1++) {
                if (*a1 & 01) {
                        *a1 &= ~01;
                        dot = a1;
                        globp = globuf;
                        commands();
                        a1 = zero;
                }
        }
}

static void
join(void)
{
        char *gp;
        int *a1;

        for (gp = genbuf, a1 = addr1; a1 <= addr2; a1++) {
                gp = genbuf_puts(gp, ed_getline(*a1));
        }
        strcpy(linebuf, genbuf);
        *addr1 = putline();
        if (addr1 < addr2)
                rdelete(addr1 + 1, addr2);
        dot = addr1;
}


static void
substitute(int inglob)
{
        int *markp, *a1, nl;
        int gsubf;

        gsubf = compsub();
        for (a1 = addr1; a1 <= addr2; a1++) {
                int *ozero;
                if (execute(0, a1) == 0)
                        continue;
                inglob |= 01;
                dosub();
                if (gsubf) {
                        while (*loc2) {
                                if (execute(1, NULL) == 0)
                                        break;
                                dosub();
                        }
                }
                subnewa = putline();
                *a1 &= ~01;
                if (anymarks) {
                        for (markp = names; markp < &names[NNAMES]; markp++)
                                if (*markp == *a1)
                                        *markp = subnewa;
                }
                subolda = *a1;
                *a1 = subnewa;
                ozero = zero;
                nl = append(getsub, a1);
                nl += zero - ozero;
                a1 += nl;
                addr2 += nl;
        }
        if (inglob == 0)
                error(Q);
}

static int
compsub(void)
{
        int seof, c;
        char *p;

        if ((seof = getchr()) == '\n' || seof == ' ')
                error(Q);
        compile(seof);
        p = rhsbuf;
        for (;;) {
                c = getchr();
                if (c == '\\')
                        c = getchr() | 0200;
                if (c == '\n') {
                        if (globp)
                                c |= 0200;
                        else
                                error(Q);
                }
                if (c == seof)
                        break;
                *p++ = c;
                if (p >= &rhsbuf[LBSIZE / 2])
                        error(Q);
        }
        *p++ = '\0';
        if ((peekc = getchr()) == 'g') {
                peekc = '\0';
                newline();
                return 1;
        }
        newline();
        return 0;
}

static int
getsub(void)
{
        if (linebp == NULL)
                return EOF;
        strcpy(linebuf, linebp);
        linebp = NULL;
        return 0;
}

static void
dosub(void)
{
        char *lp, *sp, *rp;
        int c;

        lp = linebuf;
        sp = genbuf;
        rp = rhsbuf;
        sp = genbuf_putm(sp, lp, loc1);
        while ((c = *rp++ & 0377) != '\0') {
                if (c == '&') {
                        sp = genbuf_putm(sp, loc1, loc2);
                        continue;
                } else if (c & 0200 && (c &= 0177) >='1' && c < nbra + '1') {
                        struct bralist_t *b = &bralist[c - '1'];
                        sp = genbuf_putm(sp, b->start, b->end);
                        continue;
                }
                sp = genbuf_putc(sp, c & 0177);
        }
        lp = loc2;
        loc2 = sp - genbuf + linebuf;
        do {
                sp = genbuf_putc(sp, *lp);
        } while (*lp++ != '\0');

        strcpy(linebuf, genbuf);
}

static void
move(int cflag)
{
        int *adt, *ad1, *ad2;
        int getcopy();

        setdot();
        nonzero();
        if ((adt = address()) == NULL)
                error(Q);
        newline();
        if (cflag) {
                int *ozero, delta;
                ad1 = dol;
                ozero = zero;
                append(getcopy, ad1++);
                ad2 = dol;
                delta = zero - ozero;
                ad1 += delta;
                adt += delta;
        } else {
                ad2 = addr2;
                for (ad1 = addr1; ad1 <= ad2;)
                        *ad1++ &= ~01;
                ad1 = addr1;
        }
        ad2++;
        if (adt < ad1) {
                dot = adt + (ad2 - ad1);
                if ((++adt) == ad1)
                        return;
                reverse(adt, ad1);
                reverse(ad1, ad2);
                reverse(adt, ad2);
        } else if (adt >= ad2) {
                dot = adt++;
                reverse(ad1, ad2);
                reverse(ad2, adt);
                reverse(ad1, adt);
        } else
                error(Q);
        fchange = 1;
}

static void
reverse(int *a1, int *a2)
{
        int t;

        for (;;) {
                t = *--a2;
                if (a2 <= a1)
                        return;
                *a2 = *a1;
                *a1++ = t;
        }
}

static int
getcopy(void)
{
        if (addr1 > addr2)
                return EOF;
        ed_getline(*addr1++);
        return 0;
}

static void
compile(int aeof)
{
        int eof, c;
        char *ep;
        char *lastep;
        char bracket[NBRA], *bracketp;
        int cclcnt;

        ep = expbuf;
        eof = aeof;
        bracketp = bracket;
        if ((c = getchr()) == eof) {
                if (*ep == '\0')
                        error(Q);
                return;
        }
        circfl = 0;
        nbra = 0;
        if (c == '^') {
                c = getchr();
                circfl++;
        }
        peekc = c;
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
                        if ((peekc = getchr()) != eof)
                                goto defchar;
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
        error(Q);
}

static int
execute(int gf, int *addr)
{
        char *p1, *p2, c;

        for (c = 0; c < NBRA; c++) {
                bralist[(int)c].start = NULL;
                bralist[(int)c].end = NULL;
        }
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

static int
advance(char *lp, char *ep)
{
        char *curlp;
        int i;

        for (;;)
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
                        error(Q);
                if (backref(i, lp)) {
                        lp += bralen(i);
                        continue;
                }
                return 0;

        case CBACK|STAR:
                if (bralist[i = *ep++].end == NULL)
                        error(Q);
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
                error(Q);
        }
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

static void
init(int firstinit)
{
        if (firstinit)
                zero = malloc(nlall * sizeof(int));
        tline = 2;
        memset(names, 0, sizeof(names));
        subnewa = 0;
        anymarks = 0;
        blkinit();
        dot = dol = zero;
}

static void
commands(void)
{
        int *a1, c;

        for (;;) {
                if (printflag != 0) {
                        printflag = 0;
                        addr1 = addr2 = dot;
                        goto print;
                }
                addr1 = NULL;
                addr2 = NULL;
                do {
                        addr1 = addr2;
                        if ((a1 = address()) == NULL) {
                                c = getchr();
                                break;
                        }
                        addr2 = a1;
                        if ((c = getchr()) == ';') {
                                c = ',';
                                dot = a1;
                        }
                } while (c == ',');
                if (addr1 == NULL)
                        addr1 = addr2;

                switch (c) {

                case 'a':
                        setdot();
                        newline();
                        append(gettty, addr2);
                        continue;

                case 'c':
                        delete();
                        append(gettty, addr1 - 1);
                        continue;

                case 'd':
                        delete();
                        continue;

                case 'E':
                        fchange = 0;
                        c = 'e';
                case 'e':
                        setnoaddr();
                        if (options.vflag && fchange) {
                                fchange = 0;
                                error(Q);
                        }
                        filename(c);
                        init(false);
                        addr2 = zero;
                        goto caseread;

                case 'f':
                        setnoaddr();
                        filename(c);
                        putstr(savedfile);
                        continue;

                case 'g':
                        global(1);
                        continue;

                case 'i':
                        setdot();
                        nonzero();
                        newline();
                        append(gettty, addr2 - 1);
                        continue;


                case 'j':
                        if (addr2 == NULL) {
                                addr1 = dot;
                                addr2 = dot + 1;
                        }
                        setdot();
                        newline();
                        nonzero();
                        join();
                        continue;

                case 'k':
                        c = getchr();
                        if (!islower(c))
                                error(Q);
                        newline();
                        setdot();
                        nonzero();
                        names[c - 'a'] = *addr2 & ~01;
                        anymarks |= 01;
                        continue;

                case 'm':
                        move(false);
                        continue;

                case '\n':
                        if (addr2 == NULL)
                                addr2 = dot + 1;
                        addr1 = addr2;
                        goto print;

                case 'l':
                        listf++;
                case 'p':
                case 'P':
                        newline();
                print:
                        setdot();
                        nonzero();
                        a1 = addr1;
                        do {
                                putstr(ed_getline(*a1++));
                        } while (a1 <= addr2);
                        dot = addr2;
                        listf = 0;
                        continue;

                case 'Q':
                        fchange = 0;
                case 'q':
                        setnoaddr();
                        newline();
                        quit(0); /* TODO: proper signal arg? */

                case 'r':
                        filename(c);
                caseread:
                        if (openfile(file, IOMREAD, 0) < 0) {
                                lastc = '\n';
                                error(file);
                        }
                        setall();
                        ninbuf = 0;
                        c = (zero != dol);
                        append(getfile, addr2);
                        exfile();
                        fchange = c;
                        continue;

                case 's':
                        setdot();
                        nonzero();
                        substitute(globp != NULL);
                        continue;

                case 't':
                        move(true);
                        continue;

                case 'u':
                        setdot();
                        nonzero();
                        newline();
                        if ((*addr2 & ~01) != subnewa)
                                error(Q);
                        *addr2 = subolda;
                        dot = addr2;
                        continue;

                case 'v':
                        global(0);
                        continue;

                case 'W':
                        wrapp++;
                case 'w':
                        setall();
                        nonzero();
                        filename(c);
                        if (openfile(file, IOMWRITE, wrapp) < 0)
                                error(file);
                        wrapp = 0;
                        putfile();
                        exfile();
                        if (addr1 == zero + 1 && addr2 == dol)
                                fchange = 0;
                        continue;

                case 'x':
                        setnoaddr();
                        newline();
                        options.xflag = true;
                        putstr("Entering encrypting mode!");
                        file_initkey();
                        continue;


                case '=':
                        setall();
                        newline();
                        count = (addr2 - zero) & 077777;
                        putd(count);
                        putchr('\n');
                        continue;

                case '!':
                        callunix();
                        continue;

                case EOF:
                        return;

                }
                error(Q);
        }
}

int
main(int argc, char **argv)
{
        void (*oldintr)(int);

        oldquit = signal(SIGQUIT, SIG_IGN);
        oldhup = signal(SIGHUP, SIG_IGN);
        oldintr = signal(SIGINT, SIG_IGN);
        if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
                signal(SIGTERM, quit);
        argv++;
        while (argc > 1 && **argv == '-') {
                switch ((*argv)[1]) {

                case '\0':
                        options.vflag = false;
                        break;

                case 'q':
                        signal(SIGQUIT, SIG_DFL);
                        options.vflag = true;
                        break;

                case 'x':
                        options.xflag = true;
                        break;
                }
                argv++;
                argc--;
        }

        if (options.xflag)
                file_initkey();

        if (argc > 1) {
                strcpy(savedfile, *argv);
                globp = "r";
        }

        /* FIXME: No handling of errror return on mkdtemp!!! */
        init(true);

        if (oldintr == SIG_ERR)
                signal(SIGINT, onintr);
        if (oldhup == SIG_ERR)
                signal(SIGHUP, onhup);

        setjmp(savej);
        commands();
        quit(0); /* TODO: Proper signal arg? */
}
