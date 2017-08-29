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

/* Globals (so far) */
char genbuf[LBSIZE];
int ninbuf;
long count;
int *addr1;
int *addr2;
int fchange;

/* Used also by code.c */
char *loc1;
char *loc2;
/*
 * Array of addresses, one index per line.
 * Its actual allocated number of indicess is nlall.
 */
int *zero;

struct gbl_options_t options = {
        .xflag = false,
        .vflag = true,
        .kflag = false,
};

enum {
       NNAMES  = 26,
       FNSIZE = 64,
       GBSIZE = 256,
};

/* Points to offset of zero[] marking last line of file */
static int *dol;
/* Points to somewhere between zero and dol */
static int *dot;

static char savedfile[FNSIZE];
static char file[FNSIZE];
static int printflag;
static int names[NNAMES];
static int anymarks;
static int subnewa;
static int subolda;
static int wrapp;
static unsigned nlall = 128;

static jmp_buf savej;

static int getcopy(void);
static void reverse(int *a1, int *a2);
static void move(int cflag);
static void join(void);
static void global(int k);
static void gdelete(void);
static void rdelete(int *ad1, int *ad2);
static void delete(void);
static int append(int (*action)(void), int *a);
static void exfile(void);
static void filename(int comm);
static void newline(void);
static void nonzero(void);
static void setnoaddr(void);
static void setall(void);
static void setdot(void);
static int *address(void);
static void commands(void);

static char *
genbuf_puts(char *sp, char *src)
{
        /* TODO: Why -2? */
        while ((*sp = *src++) != '\0') {
                if (sp++ >= &genbuf[LBSIZE - 2])
                        qerror();
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
                        ungetchr(c);
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
                                if (execute(false, a1))
                                        break;
                                if (a1 == dot)
                                        qerror();
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
                                qerror();
                        for (a1 = zero; a1 <= dol; a1++)
                                if (names[c - 'a'] == (*a1 & ~01))
                                        break;
                        break;

                default:
                        ungetchr(c);
                        if (a1 == NULL)
                                return NULL;
                        a1 += minus;
                        if (a1 < zero || a1 > dol)
                                qerror();
                        return a1;
                }
                if (relerr)
                        qerror();
        }
}

static void
setdot(void)
{
        if (addr2 == NULL)
                addr1 = addr2 = dot;
        if (addr1 > addr2)
                qerror();
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
        if (addr2 != NULL)
                qerror();
}

static void
nonzero(void)
{
        if (addr1 <= zero || addr2 > dol)
                qerror();
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
                        ttlwrap(true);
                if (getchr() == '\n')
                        return;
        }
        qerror();
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
                        qerror();
                strcpy(file, savedfile);
                return;
        }
        if (c != ' ')
                qerror();
        while ((c = getchr()) == ' ')
                ;
        if (c == '\n')
                qerror();
        p1 = file;
        do {
                *p1++ = c;
                if (c == ' ' || c == EOF)
                        qerror();
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

void
error(const char *s, int nl)
{
        int c;
        int lc = nl ? '\n' : regetchr();

        wrapp = 0;
        ttlwrap(false);
        putchr('?');
        putstr(s);
        count = 0;
        lseek(STDIN_FILENO, (long)0, SEEK_END);
        printflag = 0;
        if (!istt())
                lc = '\n';
        set_inp_buf(NULL);
        ungetchr(lc);
        if (lc != '\0')
                while ((c = getchr()) != '\n' && c != EOF)
                        ;
        closefile();
        longjmp(savej, 1);
}

static int
append(int (*action)(void), int *a)
{
        int *a1, *a2, *rdot;
        int nline, tl;

        nline = 0;
        dot = a;
        while (action() == 0) {
                if ((dol - zero) + 1 >= nlall) {
                        int *ozero = zero;
                        nlall += 512;
                        zero = realloc(zero, nlall * sizeof(int));
                        if (zero == NULL) {
                                zero = ozero;
                                error("MEM?", true);
                        }
                        dot += zero - ozero;
                        dol += zero - ozero;
                }
                tl = line_to_tempf();
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

void
quit(int signo)
{
        if (signo == SIGHUP) {
                /* Don't join; don't want to fall through else if below */
                if (dol > zero) {
                        int fd;
                        addr1 = zero + 1;
                        addr2 = dol;
                        fd = openfile("ed.hup", IOMCREAT, 0);
                        if (fd > 0)
                                putfile();
                }
        } else if (options.vflag && fchange && dol != zero) {
                fchange = 0;
                qerror();
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

static void
global(int k)
{
        char *gp;
        int c;
        int *a1;
        char globuf[GBSIZE];

        if (!istt())
                qerror();
        setall();
        nonzero();
        if ((c = getchr()) == '\n')
                qerror();
        compile(c);
        gp = globuf;
        while ((c = getchr()) != '\n') {
                if (c == EOF)
                        qerror();
                if (c == '\\') {
                        c = getchr();
                        if (c != '\n')
                                *gp++ = '\\';
                }
                *gp++ = c;
                if (gp >= &globuf[GBSIZE - 2])
                        qerror();
        }
        *gp++ = '\n';
        *gp++ = '\0';
        for (a1 = zero; a1 <= dol; a1++) {
                *a1 &= ~01;
                if (a1 >= addr1 && a1 <= addr2 && execute(false, a1) == k)
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
                        set_inp_buf(globuf);
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
                gp = genbuf_puts(gp, tempf_to_line(*a1));
        }
        strcpy(linebuf, genbuf);
        *addr1 = line_to_tempf();
        if (addr1 < addr2)
                rdelete(addr1 + 1, addr2);
        dot = addr1;
}


/* isbuff = true if not getting from tty */
static void
substitute(int isbuff)
{
        int *markp, *a1, nl;
        int gsubf;

        gsubf = compsub();
        newline();
        for (a1 = addr1; a1 <= addr2; a1++) {
                int *ozero;
                if (execute(false, a1) == 0)
                        continue;

                isbuff |= 01;
                dosub();
                if (gsubf) {
                        while (*loc2) {
                                if (execute(true, NULL) == 0)
                                        break;
                                dosub();
                        }
                }
                subnewa = line_to_tempf();
                *a1 &= ~01;
                if (anymarks) {
                        for (markp = names; markp < &names[NNAMES]; markp++)
                                if (*markp == *a1)
                                        *markp = subnewa;
                }
                subolda = *a1;
                *a1 = subnewa;
                ozero = zero;
                nl = append(line_getsub, a1);
                nl += zero - ozero;
                a1 += nl;
                addr2 += nl;
        }

        if (!isbuff)
                qerror();
}

static void
move(int cflag)
{
        int *adt, *ad1, *ad2;

        setdot();
        nonzero();
        if ((adt = address()) == NULL)
                qerror();
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
        } else {
                qerror();
        }
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
        tempf_to_line(*addr1++);
        return 0;
}

static void
init(void)
{
        if (zero == NULL)
                zero = malloc(nlall * sizeof(int));
        memset(names, 0, sizeof(names));
        subnewa = 0;
        anymarks = 0;
        lineinit();
        blkinit();
        dot = dol = zero;
}

static void
caseread(void)
{
        int changed;
        if (openfile(file, IOMREAD, 0) < 0)
                error(file, true);

        setall();
        ninbuf = 0;
        changed = (zero != dol);
        append(getfile, addr2);
        exfile();
        fchange = changed;
}

static void
print(void)
{
        int *a1;

        setdot();
        nonzero();
        a1 = addr1;
        do {
                putstr(tempf_to_line(*a1++));
        } while (a1 <= addr2);
        dot = addr2;
        ttlwrap(false);
}

static void
commands(void)
{
        int c;

        for (;;) {
                if (printflag != 0) {
                        printflag = 0;
                        addr1 = addr2 = dot;
                        print();
                        continue;
                }
                addr1 = NULL;
                addr2 = NULL;
                do {
                        int *a1;

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
                        append(tty_to_line, addr2);
                        continue;

                case 'c':
                        delete();
                        append(tty_to_line, addr1 - 1);
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
                                qerror();
                        }
                        filename(c);
                        init();
                        addr2 = zero;
                        caseread();
                        continue;

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
                        append(tty_to_line, addr2 - 1);
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
                                qerror();
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
                        print();
                        continue;

                case 'l':
                        ttlwrap(true);
                case 'p':
                case 'P':
                        newline();
                        print();
                        continue;

                case 'Q':
                        fchange = 0;
                case 'q':
                        setnoaddr();
                        newline();
                        quit(0); /* TODO: proper signal arg? */

                case 'r':
                        filename(c);
                        caseread();
                        continue;

                case 's':
                        setdot();
                        nonzero();
                        substitute(!istt());
                        continue;

                case 't':
                        move(true);
                        continue;

                case 'u':
                        setdot();
                        nonzero();
                        newline();
                        if ((*addr2 & ~01) != subnewa)
                                qerror();
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
                                error(file, false);
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
                        setnoaddr();
                        callunix();
                        putstr("!");
                        continue;

                case EOF:
                        return;

                }
                qerror();
        }
}

int
main(int argc, char **argv)
{
        signal_init();

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
                set_inp_buf("r");
        }

        init();

        signal_lateinit();

        setjmp(savej);
        commands();
        quit(0); /* TODO: Proper signal arg? */
}
