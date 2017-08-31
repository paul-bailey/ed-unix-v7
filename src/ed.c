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

struct buffer_t genbuf = BUFFER_INITIAL();

long count;
int fchange;

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

static struct subst_t {
        int newaddr;
        int oldaddr;
} subst;

static char savedfile[FNSIZE];
static char file[FNSIZE];
static int printflag;
static int names[NNAMES];
static int anymarks;
static int wrapp;

static jmp_buf savej;

/* append() actions */
static int getsub(struct buffer_t *);
static int getcopy(struct buffer_t *);
static int getfile(struct buffer_t *);
static int tty_to_line(struct buffer_t *);
static int append(int (*action)(struct buffer_t *), int *a);

static void reverse(int *a1, int *a2);
static void move(int cflag);
static void join(void);
static void global(int k);
static void gdelete(void);
static void rdelete(int *ad1, int *ad2);
static void delete(void);
static void exfile(void);
static void filename(int comm);
static void newline(void);
static void nonzero(void);
static void setnoaddr(void);
static void setall(void);
static void setdot(void);
static int *address(void);
static void commands(void);

/* Helper to address(), handle '/' and '?' specifically */
static int *
dosearch(int c)
{
        struct code_t cd = CODE_INITIAL();
        int *a;

        assert(c == '/' || c == '?');
        compile(c);
        a = addrs.dot;
        for (;;) {
                if (c == '/') {
                        a++;
                        if (a > addrs.dol)
                                a = addrs.zero;
                } else {
                        a--;
                        if (a < addrs.zero)
                                a = addrs.dol;
                }
                if (execute(a, addrs.zero, &cd))
                        break;
                if (a == addrs.dot)
                        qerror();
        }
        code_free(&cd);
        return a;
}

static int *
address(void)
{
        int *a, minus, c;
        int n, relerr;

        minus = 0;
        a = NULL;
        for (;;) {
                c = getchr();
                if (isdigit(c)) {
                        n = 0;
                        do {
                                n *= 10;
                                n += c - '0';
                        } while (isdigit(c = getchr()));
                        ungetchr(c);
                        if (a == NULL)
                                a = addrs.zero;
                        if (minus < 0)
                                n = -n;
                        a += n;
                        minus = 0;
                        continue;
                }
                relerr = 0;
                if (a || minus)
                        relerr++;
                switch (c) {
                case ' ':
                case '\t':
                        continue;

                case '+':
                        minus++;
                        if (a == NULL)
                                a = addrs.dot;
                        continue;

                case '-':
                case '^':
                        minus--;
                        if (a == NULL)
                                a = addrs.dot;
                        continue;

                case '?':
                case '/':
                        a = dosearch(c);
                        break;

                case '$':
                        a = addrs.dol;
                        break;

                case '.':
                        a = addrs.dot;
                        break;

                case '\'':
                        if (!islower(c = getchr()))
                                qerror();
                        for (a = addrs.zero; a <= addrs.dol; a++)
                                if (names[c - 'a'] == (*a & ~01))
                                        break;
                        break;

                default:
                        ungetchr(c);
                        if (a != NULL) {
                                a += minus;
                                if (a < addrs.zero || a > addrs.dol)
                                        qerror();
                        }
                        return a;
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
fname_strip_in_place(char *s)
{
        int c;
        for (;;) {
                c = *s;
                if (!isgraph(c)) {
                        /* TODO: Allow this instead? */
                        if (c == ' ')
                                qerror();
                        *s = '\0';
                        break;
                }
                ++s;
        }
}

static void
filename(int comm)
{
        int c;
        char *line;
        char *s;

        count = 0;

        /*
         * We could be at the end of a string buffer,
         * not on term
         */
        s = line = ttgetdelim('\n');
        if (s == NULL || (c = *s++) == '\n') {
                if (savedfile[0] == '\0' && comm != 'f') {
                        qerror();
                }
                strcpy(file, savedfile);
                goto done;
        }

        if (c != ' ')
                qerror();
        while ((c = *s++) == ' ')
                ;
        if (c == '\n')
                qerror();

        strncpy(file, s, FNSIZE);
        file[FNSIZE - 1] = '\0';
        fname_strip_in_place(file);

        if (savedfile[0] == '\0' || comm == 'e' || comm == 'f')
                strcpy(savedfile, file);

done:
        free(line);
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
append(int (*action)(struct buffer_t *), int *a)
{
        int nline, tl;
        struct buffer_t lb = BUFFER_INITIAL();

        nline = 0;
        addrs.dot = a;
        while (action(&lb) == 0) {
                int *a1, *a2, *rdot;
                if ((addrs.dol - addrs.zero) + 1 >= addrs.nlall) {
                        int *ozero = addrs.zero;
                        addrs.nlall += 512;
                        addrs.zero = realloc(addrs.zero,
                                        addrs.nlall * sizeof(*addrs.zero));
                        if (addrs.zero == NULL) {
                                addrs.zero = ozero;
                                error("MEM?", true);
                        }
                        addrs.dot += addrs.zero - ozero;
                        addrs.dol += addrs.zero - ozero;
                }
                tl = tempf_putline(&lb);
                nline++;
                a1 = ++addrs.dol;
                a2 = a1 + 1;
                rdot = ++addrs.dot;
                while (a1 > rdot)
                        *--a2 = *--a1;
                *rdot = tl;
        }
        buffer_free(&lb);
        return nline;
}

void
quit(int signo)
{
        if (signo == SIGHUP) {
                /* Don't join if's -- read closely */
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

        tempf_quit();
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
        for (a1 = addrs.zero + 1; (*a1 & 01) == 0; a1++) {
                if (a1 >= a3)
                        return;
        }

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
globuf_esc_in_place(char *gp)
{
        int c;
        char *tail = gp;
        while ((c = *gp++) != '\n') {
                if (c == '\\') {
                        c = *gp++;
                        if (c != '\n') {
                                *tail++ = '\\';
                                --gp;
                        }
                }
                *tail++ = c;
        }
        *tail++ = '\n';
        *tail++ = '\0';
}

static void
global(int k)
{
        char *gp;
        int c;
        int *a1;
        struct code_t cd = CODE_INITIAL();

        if (!istt())
                qerror();
        setall();
        nonzero();
        if ((c = getchr()) == '\n')
                qerror();
        compile(c);

        gp = ttgetdelim('\n');
        if (gp == NULL)
                qerror();
        globuf_esc_in_place(gp);

        for (a1 = addrs.zero; a1 <= addrs.dol; a1++) {
                *a1 &= ~01;
                if (a1 >= addrs.addr1
                    && a1 <= addrs.addr2
                    && execute(a1, addrs.zero, &cd) == k) {
                        *a1 |= 01;
                }
        }
        code_free(&cd);

        /*
         * Special case: g/.../d (avoid n^2 algorithm)
         */
        if (gp[0]=='d' && gp[1]=='\n' && gp[2]=='\0') {
                gdelete();
                goto out;
        }

        /* Use gp as the "globp" command for all addresses */
        for (a1 = addrs.zero; a1 <= addrs.dol; a1++) {
                if (*a1 & 01) {
                        *a1 &= ~01;
                        addrs.dot = a1;
                        set_inp_buf(gp);
                        commands();
                        a1 = addrs.zero;
                }
        }

out:
        free(gp);
}

static void
join(void)
{
        int *a;
        struct buffer_t lb = BUFFER_INITIAL();
        struct buffer_t gb = BUFFER_INITIAL();

        buffer_reset(&gb);
        for (a = addrs.addr1; a <= addrs.addr2; a++) {
                buffer_strapp(&gb, tempf_getline(*a, &lb));
        }
        buffer_reset(&lb);
        buffer_strcpy(&lb, &gb);
        *addrs.addr1 = tempf_putline(&lb);
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
        struct code_t cd = CODE_INITIAL();

        gsubf = compsub();
        newline();
        for (a1 = addrs.addr1; a1 <= addrs.addr2; a1++) {
                int *ozero;
                if (execute(a1, addrs.zero, &cd) == 0)
                        continue;

                isbuff |= 01;
                dosub(&cd);
                if (gsubf) {
                        while (*cd.loc2 != '\0') {
                                if (execute(NULL, addrs.zero, &cd) == 0)
                                        break;
                                dosub(&cd);
                        }
                }
                subst.newaddr = tempf_putline(&cd.lb);
                *a1 &= ~01;
                if (anymarks) {
                        for (markp = names; markp < &names[NNAMES]; markp++)
                                if (*markp == *a1)
                                        *markp = subst.newaddr;
                }
                subst.oldaddr = *a1;
                *a1 = subst.newaddr;
                ozero = addrs.zero;
                nl = append(getsub, a1);
                nl += addrs.zero - ozero;
                a1 += nl;
                addrs.addr2 += nl;
        }
        code_free(&cd);

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
tty_to_line(struct buffer_t *lb)
{
        int c;
        int gf;

        buffer_reset(lb);
        gf = !istt();
        while ((c = getchr()) != '\n') {
                if (c == EOF) {
                        if (gf)
                                ungetchr(c);
                        return c;
                }
                c = toascii(c);
                if (c == '\0')
                        continue;
                buffer_putc(lb, c);
        }
        buffer_putc(lb, '\0');
        if (lb->base[0] == '.' && lb->base[1] == '\0')
                return EOF;
        return '\0';
}

static int
getsub(struct buffer_t *lb)
{
        char *lp = lb->base;
        char *top = &lb->base[lb->size];
        char *linebp = NULL;
        int c;

        /*
         * XXX: This is a lot to do
         * to remove the linebp
         * interdependency with tempf_getline().
         */
        while (lp < top && (c = *lp++) != '\0') {
                if (c == '\n') {
                        linebp = lp;
                        break;
                }
        }

        if (linebp == NULL)
                return EOF;

        /* TODO: Reset and buffer_strapp instead? */
        buffer_reset(lb);
        buffer_strapp(lb, linebp);
        return 0;
}

static int
getcopy(struct buffer_t *lb)
{
        if (addrs.addr1 > addrs.addr2)
                return EOF;
        tempf_getline(*addrs.addr1++, lb);
        return 0;
}

static int
getfile(struct buffer_t *lb)
{
        return file_next_line(lb);
}

static void
init(void)
{
        if (addrs.zero == NULL)
                addrs.zero = malloc(addrs.nlall * sizeof(int));
        memset(names, 0, sizeof(names));
        subst.newaddr = 0;
        anymarks = 0;
        tempf_init();
        addrs.dot = addrs.dol = addrs.zero;
}

static void
caseread(void)
{
        int changed;
        if (openfile(file, IOMREAD, 0) < 0)
                error(file, true);

        setall();
        file_reset_state();
        changed = (addrs.zero != addrs.dol);
        append(getfile, addrs.addr2);
        exfile();
        fchange = changed;
}

static void
print(void)
{
        int *a1;
        struct buffer_t lb;

        setdot();
        nonzero();
        a1 = addrs.addr1;
        do {
                putstr(tempf_getline(*a1++, &lb));
        } while (a1 <= addrs.addr2);
        addrs.dot = addrs.addr2;
        ttlwrap(false);
        buffer_free(&lb);
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
                        if ((*addrs.addr2 & ~01) != subst.newaddr)
                                qerror();
                        *addrs.addr2 = subst.oldaddr;
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
