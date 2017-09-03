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


long count;
int fchange;

struct gbl_options_t options = {
        .xflag = false,
        .vflag = true,
        .kflag = false,
};

enum {
        FNSIZE = 1024,
};

struct addr_t addrs = {
        .nlall = 128
};

struct mark_t marks;

static char savedfile[FNSIZE];
static char file[FNSIZE];
static int printflag;
static int wrapp;
static int listflag;

static jmp_buf savej;

static void global(int k);
static void commands(void);

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

/**
 * newline - Expect a newline from user.
 *
 * If there is a 'p' or 'l' (print or list command),
 * set flags and then get expect newline.
 * Otherwise error().
 */
void
newline(void)
{
        int c;

        if ((c = getchr()) == '\n')
                return;
        if (c == 'p' || c == 'l') {
                printflag++;
                if (c == 'l')
                        listflag = true;
                if (getchr() == '\n')
                        return;
        }
        qerror();
}

static void
fname_strip_in_place(char *s)
{
        int c;
        do {
                c = *s;
                if (!isgraph(c)) {
                        /* TODO: Support backslash-space combo. */
                        if (c == ' ')
                                qerror();
                        *s = '\0';
                        break;
                }
                ++s;
        } while (*s != '\0');
}

static void
strncpy_safe(char *dst, const char *src, size_t size)
{
        /* strncpy() does not guarantee nul-char termination */
        strncpy(dst, src, size);
        dst[size - 1] = '\0';
}

#define fname_copy_safe(d_, s_) strncpy_safe((d_), (s_), FNSIZE)

static void
filename(int cmd)
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
        if (s == NULL || *s == '\n') {
                /* "r" command without file argument */
                if (savedfile[0] == '\0' && cmd != 'f')
                        qerror();
                strcpy(file, savedfile);
                goto done;
        }

        c = *s++;
        if (!isblank(c))
                qerror();

        do {
                c = *s++;
        } while (isblank(c));
        if (c == '\n')
                qerror();
        --s;

        fname_strip_in_place(s);
        fname_copy_safe(file, s);

        if (savedfile[0] == '\0' || cmd == 'e' || cmd == 'f')
                fname_copy_safe(savedfile, file);

done:
        if (line != NULL)
                free(line);
}

static void
exfile(void)
{
        closefile();
        if (options.vflag)
                printf("%lu\n", count);
}

/**
 * error - Print ed's famously verbose '?' and reset state.
 */
void
error(const char *s)
{
        printf("? %s\n", s);
        longjmp(savej, 1);
}

/**
 * quit - Maybe exit program
 * @signo: A signal number.
 *
 * If @signo is not SIGHUP, and the file state is dirty, send a
 * '?' and make user try to quit again.
 */
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
delete(void)
{
        setdot();
        newline();
        nonzero();
        rdelete(addrs.addr1, addrs.addr2);
}

static void
gdelete(void)
{
        int *a1, *a2, *a3;

        a3 = addrs.dol;
        for (a1 = addrs.zero + 1; !is_address_marked(*a1); a1++) {
                if (a1 >= a3)
                        return;
        }

        for (a2 = a1 + 1; a2 <= a3;) {
                if (is_address_marked(*a2)) {
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
        int c;
        int *a;
        struct code_t cd;
        struct buffer_t gb = BUFFER_INITIAL();

        if (!istt())
                qerror();
        setall();
        nonzero();
        if ((c = getchr()) == '\n')
                qerror();
        compile(&cd, c);

        if (tty_get_line(&gb) == EOF)
                qerror();

        for (a = addrs.zero; a <= addrs.dol; a++) {
                unmark_address(*a);
                if (a >= addrs.addr1 && a <= addrs.addr2
                    && execute(a, &cd) == k) {
                        unmark_address(*a);
                }
        }
        code_free(&cd);

        /*
         * Special case: g/.../d (avoid n^2 algorithm)
         */
        if (!strcmp(gb.base, "d\n")) {
                gdelete();
                goto out;
        }

        /*
         * Run commands() for every address, resetting the
         * getchr() input buffer to our saved command every
         * time.
         */
        for (a = addrs.zero; a <= addrs.dol; a++) {
                if (is_address_marked(*a)) {
                        unmark_address(*a);
                        addrs.dot = a;
                        set_inp_buf(gb.base);
                        commands();
                        a = addrs.zero;
                }
        }

out:
        buffer_free(&gb);
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


static void
init(void)
{
        if (addrs.zero == NULL)
                addrs.zero = malloc(addrs.nlall * sizeof(int));
        subst.newaddr = 0;
        memset(&marks, 0, sizeof(marks));
        tempf_init();
        addrs.dot = addrs.dol = addrs.zero;
}

static void
caseread(void)
{
        int changed;
        if (openfile(file, IOMREAD, 0) < 0)
                error(file);

        setall();
        file_reset_state();
        changed = (addrs.zero != addrs.dol);
        append(A_GETFILE, addrs.addr2);
        exfile();
        fchange = changed;
}

/*
 * Helper to print().
 *
 * Print line from a file. If list, make everything explicit
 * (dollar signs for eol, etc.).
 */
static void
printline(const char *s, int list)
{
        enum { NCOL = 72 };
        int c;
        int col = 0;
        while ((c = *s++) != '\0') {
                if (list) {
                        ++col;
                        if (col >= NCOL) {
                                putchar('\\');
                                putchar('\n');
                                col = 0;
                        }

                        if (c == '\t' || c == '\b') {
                                printf("-\b%c", c == '\t' ? '>' : '<');
                                continue;
                        }
                        if (c == '\n') {
                                /* Shouldn't be possible, but... */
                                putchar('\\');
                                putchar('n');
                                continue;
                        }
                        if (!isprint(c)) {
                                printf("\\%o", c);
                                continue;
                        }
                        if (c == '$')
                                putchar('\\');
                }
                putchar(c);
        }
        if (list)
                putchar('$');
        putchar('\n');
}

static void
print(void)
{
        int *a;
        struct buffer_t lb = BUFFER_INITIAL();

        setdot();
        nonzero();
        a = addrs.addr1;
        assert(a != NULL);
        do {
                printline(tempf_getline(*a++, &lb), listflag);
        } while (a <= addrs.addr2);
        addrs.dot = addrs.addr2;
        listflag = false;
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
                        int *a;

                        addrs.addr1 = addrs.addr2;
                        if ((a = address()) == NULL) {
                                c = getchr();
                                break;
                        }
                        addrs.addr2 = a;

                        if ((c = getchr()) == ';') {
                                c = ',';
                                addrs.dot = a;
                        }
                } while (c == ',');

                if (addrs.addr1 == NULL)
                        addrs.addr1 = addrs.addr2;

                switch (c) {

                case 'a':
                        setdot();
                        newline();
                        append(A_GETLINE, addrs.addr2);
                        continue;

                case 'c':
                        delete();
                        append(A_GETLINE, addrs.addr1 - 1);
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
                        printf("%s\n", savedfile);
                        continue;

                case 'g':
                        global(1);
                        continue;

                case 'i':
                        setdot();
                        nonzero();
                        newline();
                        append(A_GETLINE, addrs.addr2 - 1);
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
                        marks.names[c - 'a'] = unmarked_address(*addrs.addr2);
                        marks.any = true;
                        continue;

                case 'm':
                        setdot();
                        nonzero();
                        move(false);
                        continue;

                case '\n':
                        if (addrs.addr2 == NULL)
                                addrs.addr2 = addrs.dot + 1;
                        addrs.addr1 = addrs.addr2;
                        print();
                        continue;

                case 'l':
                        listflag = true;
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
                        setdot();
                        nonzero();
                        move(true);
                        continue;

                case 'u':
                        setdot();
                        nonzero();
                        newline();
                        if (unmarked_address(*addrs.addr2) != subst.newaddr)
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
                                error(file);
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
                        printf("Entering encrypting mode!\n");
                        file_initkey();
                        continue;


                case '=':
                        setall();
                        newline();
                        count = (addrs.addr2 - addrs.zero) & 077777;
                        printf("%lu\n", count);
                        continue;

                case '!':
                        setnoaddr();
                        callunix();
                        printf("!\n");
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
                        /*
                         * Do not allow this, until cr.c
                         * is better maintained.
                         */
                        if (false)
                                options.xflag = true;
                        break;
                default:
                        /* Simply ignore extra options. */
                        break;
                }
                argv++;
                argc--;
        }

        if (options.xflag)
                file_initkey();

        if (argc > 1) {
                fname_copy_safe(savedfile, *argv);
                set_inp_buf("r");
        }

        init();

        signal_lateinit();

        if (setjmp(savej) != 0) {
                wrapp = 0;
                listflag = false;
                count = 0;
                printflag = 0;
                set_inp_buf(NULL);
                closefile();
        }
        commands();
        quit(0); /* TODO: Proper signal arg? */
        return 0;
}
