#ifndef ED_H
#define ED_H

#include <stdio.h> /* for ssize_t definition */

enum {
        LBSIZE = 512,
};



/* term.c */
extern void putchr(int ac);
extern int getchr(void);
extern void putstr(const char *sp);
extern void putd(long v);

/* file.c */
enum {
        IOMREAD = 0,
        IOMWRITE = 1,
        IOMCREAT = 2,
}; /* type arg to openfile */
extern void putfile(void);
extern int getfile(void);
extern void closefile(void);
extern int openfile(const char *nm, int type, int wrap);
extern void file_initkey(void);

/* blk.c */
enum {
        READ = 0,
        WRITE = 1,
}; /* Arg to getblock */
extern char *getblock(int atl, int iof, int *nleft);
extern void blkinit(void);
extern void blkquit(void);

/* cr.c */
extern void crblock(char *permp, char *buf, int nchar, long startn);
extern char *getkey(int *result, char *buf);
extern char *makekey(char *buf);

/* ed.c */
extern struct gbl_options_t {
        int xflag;
        int vflag;
        int kflag;
} options;

extern const char WRERR[];
extern const char Q[];
extern const char T[];

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
extern char linebuf[LBSIZE];
extern int nleft;
extern int xflag;

extern void error(const char *s);
extern char *ed_getline(int tl);

#endif /* ED_H */
