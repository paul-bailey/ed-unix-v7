#ifndef ED_H
#define ED_H

#include <stdio.h> /* for ssize_t definition */
#include <stdbool.h>

enum {
        LBSIZE = 512,
        HIGHBIT = 0200,
};

struct bralist_t {
        char *start;
        char *end;
};

struct buffer_t {
        char *base;
        size_t size;
        int count;
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
extern char *ttgetdelim(int delim);

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
extern char *linebp; /* Pointer in block buffer or NULL */
extern struct buffer_t linebuf;
extern void blkquit(void);
extern void lineinit(void);
extern char *tempf_to_line(int tl, struct buffer_t *lbuf);
extern int line_to_tempf(struct buffer_t *lbuf);
extern int line_getsub(void);

/* cr.c */
extern void crblock(char *permp, char *buf, int nchar, long startn);
extern char *getkey(int *result, char *buf);
extern char *makekey(char *buf);

/* code.c */
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
#define BUFFER_INITIAL()  \
        { .base = NULL, .count = 0, .size = 0 }
static inline void buffer_reset(struct buffer_t *b) { b->count = 0; }
static inline char *buffer_ptr(struct buffer_t *b)
{
        return &b->base[b->count];
}
static inline size_t buffer_rem(struct buffer_t *b)
{
        /* TODO: -1? */
        return b->size - b->count;
}
extern void buffer_putc(struct buffer_t *b, int c);
extern void buffer_append(struct buffer_t *dst, struct buffer_t *src);
extern void buffer_strcpy(struct buffer_t *dst, struct buffer_t *src);
extern void buffer_strapp(struct buffer_t *dst, const char *s);
extern void buffer_memapp(struct buffer_t *dst, char *start, char *end);

/* ed.c */
extern struct gbl_options_t {
        int xflag;
        int vflag;
        int kflag;
} options;
extern struct buffer_t genbuf;
extern int ninbuf;

extern long count;

extern int fchange; /* dirty flag */

/*
 * Pointers into linebuf, used for pattern
 * searching and substitution.
 */
extern char *loc1;
extern char *loc2;

extern void error(const char *s, int nl);
extern void quit(int signo);

/* Quietest error msg. Our most frequently used. */
static inline void qerror(void) { error("", false); }

#endif /* ED_H */
