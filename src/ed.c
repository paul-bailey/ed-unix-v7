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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

char Q[] = "";
char T[] = "TMP";

int peekc;
int lastc;
char savedfile[FNSIZE];
char file[FNSIZE];
char linebuf[LBSIZE];
char rhsbuf[LBSIZE / 2];
char expbuf[ESIZE + 4];
int circfl;
int *zero;
int *dot;
int *dol;
int *addr1;
int *addr2;
char genbuf[LBSIZE];
long count;
char *nextip;
char *linebp;
int ninbuf;
int io;
int pflag;
void (*oldhup)(int) = SIG_ERR;
void (*oldquit)(int) = SIG_ERR;
int vflag = 1;
int xflag;
int xtflag;
int kflag;
char key[KSIZE + 1];
char crbuf[512];
char perm[768];
char tperm[768];
int listf;
char *globp;
int tfile = -1;
int tline;
char *tfname;
char *loc1;
char *loc2;
char *locs;
int iblock = -1;
int oblock = -1;
int ichanged;
int nleft;
char WRERR[] = "WRITE ERROR";
int names[NNAMES];
int anymarks;
char *braslist[NBRA];
char *braelist[NBRA];
int nbra;
int subnewa;
int subolda;
int fchange;
int wrapp;
unsigned nlall = 128;

jmp_buf savej;


static void makekey(char *a, char *b);
static int getkey(void);
static void putd(void);
static int cclass(char *set, char c, int af);
static int backref(int i, char *lp);
static int advance(char *lp, char *ep);
static int execute(int gf, int *addr);
static void compile(int aeof);
static int getcopy(void);
static void reverse(int *a1, int *a2);
static void move(int cflag);
static char * place(char *sp, char *l1, char *l2);
static void dosub(void);
static int getsub(void);
static int compsub(void);
static void substitute(int inglob);
static void join(void);
static void global(int k);
static void init(void);
static int putline(void);
static void gdelete(void);
static void rdelete(int *ad1, int *ad2);
static void delete(void);
static void quit(int signo);
static void callunix(void);
static int append(int (*f)(), int *a);
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

int
main(int argc, char **argv)
{
        char *p1, *p2;
        void (*oldintr)(int);

        oldquit = signal(SIGQUIT, SIG_IGN);
        oldhup = signal(SIGHUP, SIG_IGN);
        oldintr = signal(SIGINT, SIG_IGN);
        if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
                signal(SIGTERM, quit);
        argv++;
        while (argc > 1 && **argv=='-') {
                switch ((*argv)[1]) {

                case '\0':
                        vflag = 0;
                        break;

                case 'q':
                        signal(SIGQUIT, SIG_DFL);
                        vflag = 1;
                        break;

                case 'x':
                        xflag = 1;
                        break;
                }
                argv++;
                argc--;
        }

        if (xflag){
                getkey();
                kflag = crinit(key, perm);
        }

        if (argc>1) {
                p1 = *argv;
                p2 = savedfile;
                while (!!(*p2++ = *p1++))
                        ;
                globp = "r";
        }
        zero = (int *)malloc(nlall * sizeof(int));
        tfname = mktemp("/tmp/eXXXXX");
        init();

        if (oldintr == SIG_ERR)
                signal(SIGINT, onintr);
        if (oldhup == SIG_ERR)
                signal(SIGHUP, onhup);

        setjmp(savej);
        commands();
        quit(0); /* TODO: Proper signal arg? */
}


static void
commands(void)
{
        int *a1, c;

        for (;;) {
        if (pflag) {
                pflag = 0;
                addr1 = addr2 = dot;
                goto print;
        }
        addr1 = 0;
        addr2 = 0;
        do {
                addr1 = addr2;
                if ((a1 = address()) == 0) {
                        c = getchr();
                        break;
                }
                addr2 = a1;
                if ((c = getchr()) == ';') {
                        c = ',';
                        dot = a1;
                }
        } while (c == ',');
        if (addr1 == 0)
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
                if (vflag && fchange) {
                        fchange = 0;
                        error(Q);
                }
                filename(c);
                init();
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
                append(gettty, addr2-1);
                continue;


        case 'j':
                if (addr2 == 0) {
                        addr1 = dot;
                        addr2 = dot+1;
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
                move(0);
                continue;

        case '\n':
                if (addr2==0)
                        addr2 = dot+1;
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
                if ((io = open(file, 0)) < 0) {
                        lastc = '\n';
                        error(file);
                }
                setall();
                ninbuf = 0;
                c = zero != dol;
                append(getfile, addr2);
                exfile();
                fchange = c;
                continue;

        case 's':
                setdot();
                nonzero();
                substitute(globp != 0);
                continue;

        case 't':
                move(1);
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
                if (!wrapp ||
                  ((io = open(file, 1)) == -1) ||
                  ((lseek(io, 0L, 2)) == -1))
                        if ((io = creat(file, 0666)) < 0)
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
                xflag = 1;
                putstr("Entering encrypting mode!");
                getkey();
                kflag = crinit(key, perm);
                continue;


        case '=':
                setall();
                newline();
                count = (addr2 - zero) & 077777;
                putd();
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

static int *
address(void)
{
        int *a1, minus, c;
        int n, relerr;

        minus = 0;
        a1 = 0;
        for (;;) {
                c = getchr();
                if (isdigit(c)) {
                        n = 0;
                        do {
                                n *= 10;
                                n += c - '0';
                        } while (isdigit(c = getchr()));
                        peekc = c;
                        if (a1 == 0)
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
                        if (a1 == 0)
                                a1 = dot;
                        continue;

                case '-':
                case '^':
                        minus--;
                        if (a1 == 0)
                                a1 = dot;
                        continue;

                case '?':
                case '/':
                        compile(c);
                        a1 = dot;
                        for (;;) {
                                if (c=='/') {
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
                        if (a1 == 0)
                                return 0;
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
        if (addr2 == 0)
                addr1 = addr2 = dot;
        if (addr1 > addr2)
                error(Q);
}

static void
setall(void)
{
        if (addr2 == 0) {
                addr1 = zero+1;
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
        if (c=='p' || c=='l') {
                pflag++;
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
        char *p1, *p2;
        int c;

        count = 0;
        c = getchr();
        if (c == '\n' || c == EOF) {
                p1 = savedfile;
                if (*p1 == 0 && comm != 'f')
                        error(Q);
                p2 = file;
                while (!!(*p2++ = *p1++))
                        ;
                return;
        }
        if (c != ' ')
                error(Q);
        while ((c = getchr()) == ' ')
                ;
        if (c=='\n')
                error(Q);
        p1 = file;
        do {
                *p1++ = c;
                if (c == ' ' || c == EOF)
                        error(Q);
        } while ((c = getchr()) != '\n');
        *p1++ = 0;
        if (savedfile[0] == 0 || comm == 'e' || comm == 'f') {
                p1 = savedfile;
                p2 = file;
                while (!!(*p1++ = *p2++))
                        ;
        }
}

static void
exfile(void)
{
        close(io);
        io = -1;
        if (vflag) {
                putd();
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
        signal(SIGINT, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
        if (dol > zero) {
                addr1 = zero+1;
                addr2 = dol;
                io = creat("ed.hup", 0666);
                if (io > 0)
                        putfile();
        }
        fchange = 0;
        quit(signo);
}

void
error(char *s)
{
        int c;

        wrapp = 0;
        listf = 0;
        putchr('?');
        putstr(s);
        count = 0;
        lseek(STDIN_FILENO, (long)0, 2);
        pflag = 0;
        if (globp)
                lastc = '\n';
        globp = 0;
        peekc = lastc;
        if (lastc)
                while ((c = getchr()) != '\n' && c != EOF)
                        ;
        if (io > 0) {
                close(io);
                io = -1;
        }
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
                if ((c &= 0177) == 0)
                        continue;
                *p++ = c;
                if (p >= &linebuf[LBSIZE - 2])
                        error(Q);
        }
        *p++ = 0;
        if (linebuf[0] == '.' && linebuf[1] == 0)
                return EOF;
        return 0;
}

static int
append(int (*f)(), int *a)
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
                        if ((zero = (int *)realloc((char *)zero, nlall * sizeof(int))) == NULL) {
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
        int pid, rpid;
        int retcode;

        setnoaddr();
        if ((pid = fork()) == 0) {
                signal(SIGHUP, oldhup);
                signal(SIGQUIT, oldquit);
                execl("/bin/sh", "sh", "-t", (char *)NULL);
                exit(0100);
        }
        savint = signal(SIGINT, SIG_IGN);
        while ((rpid = wait(&retcode)) != pid && rpid != -1)
                ;
        signal(SIGINT, savint);
        putstr("!");
}

static void
quit(int signo)
{
        if (vflag && fchange && dol != zero) {
                fchange = 0;
                error(Q);
        }
        unlink(tfname);
        exit(0);
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
                } else
                        *a1++ = *a2++;
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
        bp = getblock(tl, READ);
        nl = nleft;
        tl &= ~0377;
        while (!!(*lp++ = *bp++))
                if (--nl == 0) {
                        bp = getblock(tl += 0400, READ);
                        nl = nleft;
                }
        return linebuf;
}

static int
putline(void)
{
        char *bp, *lp;
        int nl;
        int tl;

        fchange = 1;
        lp = linebuf;
        tl = tline;
        bp = getblock(tl, WRITE);
        nl = nleft;
        tl &= ~0377;
        while (!!(*bp = *lp++)) {
                if (*bp++ == '\n') {
                        *--bp = 0;
                        linebp = lp;
                        break;
                }
                if (--nl == 0) {
                        bp = getblock(tl += 0400, WRITE);
                        nl = nleft;
                }
        }
        nl = tline;
        tline += (((lp - linebuf) + 03) >> 1) & 077776;
        return nl;
}

static void
init(void)
{
        int *markp;

        close(tfile);
        tline = 2;
        for (markp = names; markp < &names[NNAMES]; )
                *markp++ = 0;
        subnewa = 0;
        anymarks = 0;
        iblock = -1;
        oblock = -1;
        ichanged = 0;
        close(creat(tfname, 0600));
        tfile = open(tfname, 2);
        if (xflag) {
                xtflag = 1;
                makekey(key, tperm);
        }
        dot = dol = zero;
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
        *gp++ = 0;
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
        char *gp, *lp;
        int *a1;

        gp = genbuf;
        for (a1 = addr1; a1 <= addr2; a1++) {
                lp = ed_getline(*a1);
                while (!!(*gp = *lp++))
                        if (gp++ >= &genbuf[LBSIZE - 2])
                                error(Q);
        }
        lp = linebuf;
        gp = genbuf;
        while (!!(*lp++ = *gp++))
                ;
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
                                if (execute(1, (int *)0) == 0)
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
        *p++ = 0;
        if ((peekc = getchr()) == 'g') {
                peekc = 0;
                newline();
                return 1;
        }
        newline();
        return 0;
}

static int
getsub(void)
{
        char *p1, *p2;

        p1 = linebuf;
        if ((p2 = linebp) == 0)
                return EOF;
        while (!!(*p1++ = *p2++))
                ;
        linebp = 0;
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
        while (lp < loc1)
                *sp++ = *lp++;
        while (!!(c = *rp++ & 0377)) {
                if (c == '&') {
                        sp = place(sp, loc1, loc2);
                        continue;
                } else if (c & 0200 && (c &= 0177) >='1' && c < nbra + '1') {
                        sp = place(sp, braslist[c - '1'], braelist[c - '1']);
                        continue;
                }
                *sp++ = c&0177;
                if (sp >= &genbuf[LBSIZE])
                        error(Q);
        }
        lp = loc2;
        loc2 = sp - genbuf + linebuf;
        while (!!(*sp++ = *lp++))
                if (sp >= &genbuf[LBSIZE])
                        error(Q);
        lp = linebuf;
        sp = genbuf;
        while (!!(*lp++ = *sp++))
                ;
}

static char *
place(char *sp, char *l1, char *l2)
{
        while (l1 < l2) {
                *sp++ = *l1++;
                if (sp >= &genbuf[LBSIZE])
                        error(Q);
        }
        return sp;
}

static void
move(int cflag)
{
        int *adt, *ad1, *ad2;
        int getcopy();

        setdot();
        nonzero();
        if ((adt = address())==0)
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
                if (*ep == 0)
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
        lastep = 0;
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
                        if (lastep == 0 || *lastep == CBRA || *lastep == CKET)
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
                        *ep++ = 0;
                        cclcnt = 1;
                        if ((c = getchr()) == '^') {
                                c = getchr();
                                ep[-2] = NCCL;
                        }
                        do {
                                if (c == '\n')
                                        goto cerror;
                                if (c == '-' && ep[-1] != 0) {
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
        expbuf[0] = 0;
        nbra = 0;
        error(Q);
}

static int
execute(int gf, int *addr)
{
        char *p1, *p2, c;

        for (c = 0; c < NBRA; c++) {
                braslist[(int)c] = 0;
                braelist[(int)c] = 0;
        }
        if (gf) {
                if (circfl)
                        return 0;
                p1 = linebuf;
                p2 = genbuf;
                while (!!(*p1++ = *p2++))
                        ;
                locs = p1 = loc2;
        } else {
                if (addr == zero)
                        return 0;
                p1 = ed_getline(*addr);
                locs = 0;
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
                if (*lp == 0)
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
                braslist[(int)(*ep++)] = lp;
                continue;

        case CKET:
                braelist[(int)(*ep++)] = lp;
                continue;

        case CBACK:
                if (braelist[i = *ep++] == 0)
                        error(Q);
                if (backref(i, lp)) {
                        lp += braelist[i] - braslist[i];
                        continue;
                }
                return 0;

        case CBACK|STAR:
                if (braelist[i = *ep++] == 0)
                        error(Q);
                curlp = lp;
                while (backref(i, lp))
                        lp += braelist[i] - braslist[i];
                while (lp >= curlp) {
                        if (advance(lp, ep))
                                return 1;
                        lp -= braelist[i] - braslist[i];
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

        bp = braslist[i];
        while (*bp++ == *lp++)
                if (bp >= braelist[i])
                        return 1;
        return 0;
}

static int
cclass(char *set, char c, int af)
{
        int n;

        if (c == 0)
                return 0;
        n = *set++;
        while (--n)
                if (*set++ == c)
                        return af;
        return !af;
}

static void
putd(void)
{
        int r;

        r = count % 10;
        count /= 10;
        if (count)
                putd();
        putchr(r + '0');
}


static int
getkey(void)
{
        struct termios b;
        void (*sig)(int);
        tcflag_t save;
        char *p;
        int c;

        sig = signal(SIGINT, SIG_IGN);
        if (tcgetattr(STDIN_FILENO, &b) < 0)
                error("Input not tty");
        save = b.c_lflag;
        b.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &b);
        putstr("Key:");
        p = key;
        while (((c = getchr()) != EOF) && (c != '\n')) {
                if (p < &key[KSIZE])
                        *p++ = c;
        }
        *p = 0;
        b.c_lflag = save;
        tcsetattr(STDIN_FILENO, TCSANOW|TCSASOFT, &b);
        signal(SIGINT, sig);
        return key[0] != 0;
}

static void
makekey(char *a, char *b)
{
        int i;
        long t;
        char temp[KSIZE + 1];

        for (i = 0; i < KSIZE; i++)
                temp[i] = *a++;
        time(&t);
        t += getpid();
        for (i = 0; i < 4; i++)
                temp[i] ^= (t >> (8 * i)) & 0377;
        crinit(temp, b);
}
