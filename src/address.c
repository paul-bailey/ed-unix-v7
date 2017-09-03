#include "ed.h"
#include <ctype.h>
#include <assert.h>

static int *
dosearch(int c)
{
        struct code_t cd;
        int *a;

        assert(c == '/' || c == '?');
        compile(&cd, c);
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
                if (execute(a, &cd))
                        break;
                if (a == addrs.dot)
                        qerror();
        }
        code_free(&cd);
        return a;
}

/*
 * Get input, return NULL if not an address, or pointer to address
 * if it is.
 */
int *
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
                                if (marks.names[c - 'a'] == unmarked_address(*a))
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
