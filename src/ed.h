#ifndef ED_H
#define ED_H

#include <stdio.h> /* for ssize_t definition */

enum {
       NNAMES  = 26,

       FNSIZE = 64,
       LBSIZE = 512,
       ESIZE = 128,
       GBSIZE = 256,
       NBRA = 5,
       KSIZE = 9,

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

enum {
        READ = 0,
        WRITE = 1,
};


/* term.c */
extern void putchr(int ac);
extern int getchr(void);
extern void putstr(char *sp);

/* file.c */
extern void putfile(void);
extern int getfile(void);

/* blk.c */
extern char *getblock(int atl, int iof);
extern void blkio(int b, char *buf, ssize_t (*iofcn)());

/* cr.c */
extern void crblock(char *permp, char *buf, int nchar, long startn);
extern int crinit(char *keyp, char *permp);

/* ed.c */
extern int peekc;
extern int lastc;
extern char *globp;
extern int listf;

extern char genbuf[LBSIZE];
extern int ninbuf;

extern long count;
extern int *addr1;
extern int *addr2;
extern int kflag;
extern char perm[768];
extern char WRERR[];
extern char Q[];
extern char T[];
extern char linebuf[LBSIZE];
extern char *nextip;
extern int xtflag;
extern char crbuf[512];
extern int tfile;
extern int iblock;
extern int oblock;
extern char tperm[768];
extern int ichanged;
extern int nleft;

extern int io; /* file being mussed with */

extern void error(char *s);
extern char *ed_getline(int tl);

#endif /* ED_H */
