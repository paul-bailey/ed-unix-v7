/*
 * TODO: This whole module is OOOOLD.
 * There are better ways to create keys.
 */
#include "ed.h"
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <assert.h>

enum {
        CRBUFSIZE = 768,
        KSIZE = 9,
};

/* global */
static char key[KSIZE + 1];

/*
 * Besides initializing the encryption machine, this routine
 * returns 0 if the key is null, and 1 if it is non-null.
 */
static int
crinit(char *keyp, char *permp)
{
        char *t1, *t2, *t3;
        int i;
        int ic, k, temp, pf[2];
        unsigned random;
        char buf[13];
        long seed;

        t1 = permp;
        t2 = &permp[256];
        t3 = &permp[512];
        if (*keyp == '\0')
                return 0;

        strncpy(buf, keyp, 8);
        while (*keyp)
                *keyp++ = '\0';
        buf[8] = buf[0];
        buf[9] = buf[1];
        if (pipe(pf) < 0)
                pf[0] = pf[1] = -1;
        if (fork() == 0) {
                close(0);
                close(1);
                dup(pf[0]);
                dup(pf[1]);
                /* FIXME: Hardly implemented this way anymore. */
                execl("/usr/lib/makekey", "-", (char *)NULL);
                execl("/lib/makekey", "-", (char *)NULL);
                exit(EXIT_FAILURE);
        }
        write(pf[1], buf, 10);
        if (wait(NULL) == -1 || read(pf[0], buf, 13) != 13)
                error("crypt: cannot generate key");

        close(pf[0]);
        close(pf[1]);
        seed = 123;
        for (i = 0; i < 13; i++)
                seed = seed * buf[i] + i;

        for (i = 0; i < 256; i++){
                t1[i] = i;
                t3[i] = 0;
        }

        for (i = 0; i < 256; i++) {
                seed = 5 * seed + buf[i % 13];
                random = seed % 65521;
                k = 256 - 1 - i;
                ic = (random & 0377) % (k + 1);
                random >>= 8;
                temp = t1[k];
                t1[k] = t1[ic];
                t1[ic] = temp;
                if (t3[k] != 0)
                        continue;
                ic = (random & 0377) % k;
                while (t3[ic] != 0)
                        ic = (ic + 1) % k;
                t3[k] = ic;
                t3[ic] = k;
        }

        for (i = 0; i < 256; i++)
                t2[t1[i] & 0377] = i;
        return 1;
}


void
crblock(char *permp, char *buf, int nchar, long startn)
{
        char *p1;
        int n1;
        int n2;
        char *t1, *t2, *t3;

        t1 = permp;
        t2 = &permp[256];
        t3 = &permp[512];

        n1 = startn & 0377;
        n2 = (startn >> 8) & 0377;
        p1 = buf;
        while (nchar--) {
                *p1 = t2[(t3[(t1[(*p1 + n1) & 0377] + n2) & 0377] - n2) & 0377] - n1;
                n1++;
                if (n1 == 256){
                        n1 = 0;
                        n2++;
                        if (n2 == 256)
                                n2 = 0;
                }
                p1++;
        }
}

static char *
keybuf_alloc(char *buf)
{
        static int nbufs = 0;
        static char bufs[2][CRBUFSIZE];
        if (buf == bufs[0] || buf == bufs[1])
                return buf;

        /*
         * We should not call this more than twice, unless we're re-using
         * an old buf.
         */
        assert(buf == NULL);
        assert(nbufs <= 1);

        return bufs[nbufs++];
}

/* buf is either NULL or the previous return value
 * to this function.
 * result may be NULL if not used.
 */
char *
getkey(int *result, char *buf)
{
        struct termios b;
        void (*sig)(int);
        tcflag_t save;
        char *p;
        int c;
        int ret;

        sig = signal(SIGINT, SIG_IGN);
        if (tcgetattr(STDIN_FILENO, &b) < 0)
                error("Input not tty");
        save = b.c_lflag;
        b.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &b);
        printf("Key:\n");
        p = key;
        while (((c = getchr()) != EOF) && (c != '\n')) {
                if (p < &key[KSIZE])
                        *p++ = c;
        }
        *p = 0;
        b.c_lflag = save;
        tcsetattr(STDIN_FILENO, TCSANOW, &b);
        signal(SIGINT, sig);
        ret = key[0] != 0;
        buf = keybuf_alloc(buf);
        /*
         * TODO: if realloc fails?
         * When to clean up?
         */

        /*
         * XXX: extern alert!
         *
         * options.kflag should not need to be set here.
         */
        options.kflag = crinit(key, buf);

        /* XXX: This doesn't seem to be used */
        if (result)
                *result = ret;

        return buf;
}

char *
makekey(char *b)
{
        int i;
        long t;
        char temp[KSIZE + 1];

        memcpy(temp, key, KSIZE);

        b = keybuf_alloc(b);
        /*
         * TODO: If realloc fails?
         * When to clean up?
         */

        time(&t);
        t += getpid();
        for (i = 0; i < 4; i++)
                temp[i] ^= (t >> (8 * i)) & 0377;
        crinit(temp, b);
        return b;
}
