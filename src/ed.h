#ifndef ED_H
#define ED_H

#include <stdio.h> /* for ssize_t definition */
#include <stdbool.h>

enum {
        LBSIZE = 512,
};

/* term.c */
extern int regetchr(void);
extern void set_inp_buf(const char *s);
extern int istt(void);
extern void ungetchr(int c);
extern void putchr(int ac);
extern int getchr(void);
extern void putstr(const char *sp);
extern void putd(long v);
extern void ttlwrap(int en);

/* file.c */
enum {
        IOMREAD = 0,
        IOMWRITE = 1,
        IOMCREAT = 2,
}; /* type arg to openfile */
extern void putfile(int *a1, int *a2);
extern int getfile(void);
extern void closefile(void);
extern int openfile(const char *nm, int type, int wrap);
extern void file_initkey(void);

/* lines.c */
extern char linebuf[LBSIZE];
extern void blkquit(void);
extern void lineinit(void);
extern char *tempf_to_line(int tl);
extern int line_to_tempf(void);
extern int line_getsub(void);

/* cr.c */
extern void crblock(char *permp, char *buf, int nchar, long startn);
extern char *getkey(int *result, char *buf);
extern char *makekey(char *buf);

/* code.c */
struct bralist_t {
        char *start;
        char *end;
};
extern struct bralist_t *get_backref(int cidx); /* cidx >= '1' */
extern int execute(int *addr, int *zaddr);
extern void compile(int aeof);

/* line.c */

/* signal.c */
extern void callunix(void);
extern void signal_lateinit(void);
extern void signal_init(void);
extern void onquit(int signo);
extern void onhup(int signo);
extern void onintr(int signo);

/* subst.c */
extern void dosub(void);
extern int compsub(void);

/* simplebuf.c */
struct simplebuf_t {
        char buf[LBSIZE];
        int count;
        int istty;
};
#define SIMPLEBUF_INIT(istty_) { .count = 0, .istty = istty_, }
extern void simplebuf_init(struct simplebuf_t *b, int istty);
extern void simplebuf_putc(struct simplebuf_t *b, int c);
extern void simplebuf_flush(struct simplebuf_t *b);
extern char *genbuf_puts(char *sp, char *src);
extern char *linebuf_putc(char *sp, int c);
extern char *genbuf_putc(char *sp, int c);
extern char *genbuf_putm(char *sp, char *start, char *end);

/* ed.c */
extern struct gbl_options_t {
        int xflag;
        int vflag;
        int kflag;
} options;
extern char genbuf[LBSIZE];
extern int ninbuf;

extern long count;

extern int fchange; /* dirty flag */

extern char *loc1;
extern char *loc2;

extern void error(const char *s, int nl);
extern char *tempf_to_line(int tl);
extern void quit(int signo);

/* Quietest error msg. Our most frequently used. */
static inline void qerror(void) { error("", false); }

#endif /* ED_H */
