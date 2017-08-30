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
int fchange;

/* Used also by code.c */
char *loc1;
char *loc2;

struct gbl_options_t options = {
        .xflag = false,
        .vflag = true,
        .kflag = false,
};

enum {
       NNAMES = 26,
       FNSIZE = 64,
       GBSIZE = 256,
};

/**
 * struct addr_t - Addresses of lines in temp file.
 * @zero: Pointer to base of array of addresses.
 * @addr1: Pointer into .zero[] of address of lower line in a range.
 * @addr2: Pointer into .zero[] of address of higher line in a range.
 * @dot: Pointer into .zero[] of address of current active line.
 * @dol: Pointer into .zero[] of address of last line in file.
 * @nlall: Number of indices currently allocated for .zero[].
 *
 * .zero[] is allocated at startup, and reallocated if necessary -
 * if the file has more lines than .nlall's initial default.
 */
static struct addr_t {
        int *addr1;
        int *addr2;
        int *dot;
        int *dol;
        int *zero;
        unsigned int nlall;
} addrs = {
        .nlall = 128
};

static char savedfile[FNSIZE];
static char file[FNSIZE];
static int printflag;
static int names[NNAMES];
static int anymarks;
static int subnewa;
static int subolda;
static int wrapp;

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
static int tty_to_line(void);

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
                                a1 = addrs.zero;
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
                                a1 = addrs.dot;
                        continue;

                case '-':
                case '^':
                        minus--;
                        if (a1 == NULL)
                                a1 = addrs.dot;
                        continue;

                case '?':
                case '/':
                        compile(c);
                        a1 = addrs.dot;
                        for (;;) {
                                if (c == '/') {
                                        a1++;
                                        if (a1 > addrs.dol)
                                                a1 = addrs.zero;
                                } else {
                                        a1--;
                                        if (a1 < addrs.zero)
                                                a1 = addrs.dol;
                                }
                                if (execute(a1, addrs.zero))
                                        break;
                                if (a1 == addrs.dot)
                                        qerror();
                        }
                        break;

                case '$':
                        a1 = addrs.dol;
                        break;

                case '.':
                        a1 = addrs.dot;
                        break;

                case '\'':
                        if (!islower(c = getchr()))
                                qerror();
                        for (a1 = addrs.zero; a1 <= addrs.dol; a1++)
                                if (names[c - 'a'] == (*a1 & ~01))
                                        break;
                        break;

                default:
                        ungetchr(c);
                        if (a1 == NULL)
                                return NULL;
                        a1 += minus;
                        if (a1 < addrs.zero || a1 > addrs.dol)
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
        if (addrs.addr2 == NULL)
                addrs.addr1 = addrs.addr2 = addrs.dot;
        if (addrs.addr1 > addrs.addr2)
                qerror();
}

static void
setall(void)
{
        if (addrs.addr2 == NULL) {
                addrs.addr1 = addrs.zero + 1;
                addrs.addr2 = addrs.dol;
                if (addrs.dol == addrs.zero)
                        addrs.addr1 = addrs.zero;
        }
        setdot();
}

static void
setnoaddr(void)
{
        if (addrs.addr2 != NULL)
                qerror();
}

static void
nonzero(void)
{
        if (addrs.addr1 <= addrs.zero || addrs.addr2 > addrs.dol)
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
        int nline, tl;

        nline = 0;
        addrs.dot = a;
        while (action() == 0) {
                int *a1, *a2, *rdot;
                if ((addrs.dol - addrs.zero) + 1 >= addrs.nlall) {
                        int *ozero = addrs.zero;
                        addrs.nlall += 512;
                        addrs.zero = realloc(addrs.zero, addrs.nlall * sizeof(*addrs.zero));
                        if (addrs.zero == NULL) {
                                addrs.zero = ozero;
                                error("MEM?", true);
                        }
                        addrs.dot += addrs.zero - ozero;
                        addrs.dol += addrs.zero - ozero;
                }
                tl = line_to_tempf();
                nline++;
                a1 = ++addrs.dol;
                a2 = a1 + 1;
                rdot = ++addrs.dot;
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
                if (addrs.dol > addrs.zero) {
                        int fd;
                        addrs.addr1 = addrs.zero + 1;
                        addrs.addr2 = addrs.dol;
                        fd = openfile("ed.hup", IOMCREAT, 0);
                        if (fd > 0)
                                putfile(addrs.addr1, addrs.addr2);
                }
        } else if (options.vflag && fchange && addrs.dol != addrs.zero) {
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
        rdelete(addrs.addr1, addrs.addr2);
}

static void
rdelete(int *ad1, int *ad2)
{
        int *a1, *a2, *a3;

        a1 = ad1;
        a2 = ad2 + 1;
        a3 = addrs.dol;
        addrs.dol -= a2 - a1;
        do {
                *a1++ = *a2++;
        } while (a2 <= a3);
        a1 = ad1;
        if (a1 > addrs.dol)
                a1 = addrs.dol;
        addrs.dot = a1;
        fchange = 1;
}

static void
gdelete(void)
{
        int *a1, *a2, *a3;

        a3 = addrs.dol;
        for (a1 = addrs.zero + 1; (*a1 & 01) == 0; a1++)
                if (a1 >= a3)
                        return;
        for (a2 = a1 + 1; a2 <= a3;) {
                if (*a2 & 01) {
                        a2++;
                        addrs.dot = a1;
                } else {
                        *a1++ = *a2++;
                }
        }
        addrs.dol = a1 - 1;
        if (addrs.dot > addrs.dol)
                addrs.dot = addrs.dol;
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
        for (a1 = addrs.zero; a1 <= addrs.dol; a1++) {
                *a1 &= ~01;
                if (a1 >= addrs.addr1
                    && a1 <= addrs.addr2
                    && execute(a1, addrs.zero) == k) {
                        *a1 |= 01;
                }
        }

        /*
         * Special case: g/.../d (avoid n^2 algorithm)
         */
        if (globuf[0]=='d' && globuf[1]=='\n' && globuf[2]=='\0') {
                gdelete();
                return;
        }
        for (a1 = addrs.zero; a1 <= addrs.dol; a1++) {
                if (*a1 & 01) {
                        *a1 &= ~01;
                        addrs.dot = a1;
                        set_inp_buf(globuf);
                        commands();
                        a1 = addrs.zero;
                }
        }
}

static void
join(void)
{
        char *gp;
        int *a1;

        for (gp = genbuf, a1 = addrs.addr1; a1 <= addrs.addr2; a1++) {
                gp = genbuf_puts(gp, tempf_to_line(*a1));
        }
        strcpy(linebuf, genbuf);
        *addrs.addr1 = line_to_tempf();
        if (addrs.addr1 < addrs.addr2)
                rdelete(addrs.addr1 + 1, addrs.addr2);
        addrs.dot = addrs.addr1;
}


/* isbuff = true if not getting from tty */
static void
substitute(int isbuff)
{
        int *markp, *a1, nl;
        int gsubf;

        gsubf = compsub();
        newline();
        for (a1 = addrs.addr1; a1 <= addrs.addr2; a1++) {
                int *ozero;
                if (execute(a1, addrs.zero) == 0)
                        continue;

                isbuff |= 01;
                dosub();
                if (gsubf) {
                        while (*loc2) {
                                if (execute(NULL, addrs.zero) == 0)
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
                ozero = addrs.zero;
                nl = append(line_getsub, a1);
                nl += addrs.zero - ozero;
                a1 += nl;
                addrs.addr2 += nl;
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
                ad1 = addrs.dol;
                ozero = addrs.zero;
                append(getcopy, ad1++);
                ad2 = addrs.dol;
                delta = addrs.zero - ozero;
                ad1 += delta;
                adt += delta;
        } else {
                ad2 = addrs.addr2;
                for (ad1 = addrs.addr1; ad1 <= ad2;)
                        *ad1++ &= ~01;
                ad1 = addrs.addr1;
        }
        ad2++;
        if (adt < ad1) {
                addrs.dot = adt + (ad2 - ad1);
                if ((++adt) == ad1)
                        return;
                reverse(adt, ad1);
                reverse(ad1, ad2);
                reverse(adt, ad2);
        } else if (adt >= ad2) {
                addrs.dot = adt++;
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
tty_to_line(void)
{
        int c;
        int gf;
        char *p;

        p = linebuf;
        gf = !istt();
        while ((c = getchr()) != '\n') {
                if (c == EOF) {
                        if (gf)
                                ungetchr(c);
                        return c;
                }
                c &= 0177;
                if (c == '\0')
                        continue;
                p = linebuf_putc(p, c);
        }
        linebuf_putc(p, '\0');
        if (linebuf[0] == '.' && linebuf[1] == '\0')
                return EOF;
        return '\0';
}

static int
getcopy(void)
{
        if (addrs.addr1 > addrs.addr2)
                return EOF;
        tempf_to_line(*addrs.addr1++);
        return 0;
}

static void
init(void)
{
        if (addrs.zero == NULL)
                addrs.zero = malloc(addrs.nlall * sizeof(int));
        memset(names, 0, sizeof(names));
        subnewa = 0;
        anymarks = 0;
        lineinit();
        addrs.dot = addrs.dol = addrs.zero;
}

static void
caseread(void)
{
        int changed;
        if (openfile(file, IOMREAD, 0) < 0)
                error(file, true);

        setall();
        ninbuf = 0;
        changed = (addrs.zero != addrs.dol);
        append(getfile, addrs.addr2);
        exfile();
        fchange = changed;
}

static void
print(void)
{
        int *a1;

        setdot();
        nonzero();
        a1 = addrs.addr1;
        do {
                putstr(tempf_to_line(*a1++));
        } while (a1 <= addrs.addr2);
        addrs.dot = addrs.addr2;
        ttlwrap(false);
}

static void
commands(void)
{
        int c;

        for (;;) {
                if (printflag != 0) {
                        printflag = 0;
                        addrs.addr1 = addrs.addr2 = addrs.dot;
                        print();
                        continue;
                }
                addrs.addr1 = NULL;
                addrs.addr2 = NULL;
                do {
                        int *a1;

                        addrs.addr1 = addrs.addr2;
                        if ((a1 = address()) == NULL) {
                                c = getchr();
                                break;
                        }
                        addrs.addr2 = a1;

                        if ((c = getchr()) == ';') {
                                c = ',';
                                addrs.dot = a1;
                        }
                } while (c == ',');

                if (addrs.addr1 == NULL)
                        addrs.addr1 = addrs.addr2;

                switch (c) {

                case 'a':
                        setdot();
                        newline();
                        append(tty_to_line, addrs.addr2);
                        continue;

                case 'c':
                        delete();
                        append(tty_to_line, addrs.addr1 - 1);
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
                        addrs.addr2 = addrs.zero;
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
                        append(tty_to_line, addrs.addr2 - 1);
                        continue;


                case 'j':
                        if (addrs.addr2 == NULL) {
                                addrs.addr1 = addrs.dot;
                                addrs.addr2 = addrs.dot + 1;
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
                        names[c - 'a'] = *addrs.addr2 & ~01;
                        anymarks |= 01;
                        continue;

                case 'm':
                        move(false);
                        continue;

                case '\n':
                        if (addrs.addr2 == NULL)
                                addrs.addr2 = addrs.dot + 1;
                        addrs.addr1 = addrs.addr2;
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
                        if ((*addrs.addr2 & ~01) != subnewa)
                                qerror();
                        *addrs.addr2 = subolda;
                        addrs.dot = addrs.addr2;
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
                        putfile(addrs.addr1, addrs.addr2);
                        exfile();
                        if (addrs.addr1 == addrs.zero + 1
                            && addrs.addr2 == addrs.dol) {
                                fchange = 0;
                        }
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
                        count = (addrs.addr2 - addrs.zero) & 077777;
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
