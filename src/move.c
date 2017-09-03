#include "ed.h"

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

void
move(int cflag)
{
        int *adt, *ad1, *ad2;

        if ((adt = address()) == NULL)
                qerror();
        newline();
        if (cflag) {
                int *ozero, delta;
                ad1 = addrs.dol;
                ozero = addrs.zero;
                append(A_GETCOPY, ad1++);
                ad2 = addrs.dol;
                delta = addrs.zero - ozero;
                ad1 += delta;
                adt += delta;
        } else {
                ad2 = addrs.addr2;
                for (ad1 = addrs.addr1; ad1 <= ad2; ad1++)
                        *ad1 = unmark_address(*ad1);

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
