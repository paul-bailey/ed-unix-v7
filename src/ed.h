#ifndef ED_H
#define ED_H

#include <stdio.h> /* for ssize_t definition */
#include <stdbool.h>

enum {
        NMARKS = 26,
        LBSIZE = 512,
        HIGHBIT = 0200,
        NBRA = 5,
};



struct buffer_t {
        char *base;
        size_t size;
        int count;
        int tail;
};
#define BUFFER_INITIAL()  \
        { .base = NULL, .count = 0, .size = 0, .tail = 0, }

struct code_t {
        struct buffer_t lb;
        struct buffer_t expbuf;
        char *loc1;
        char *loc2;
        char *locs;
        int nbra;
        int circfl;
        struct bralist_t {
                char *start;
                char *end;
        } bralist[NBRA];
};

/* term.c */
extern void set_inp_buf(const char *s);
extern int istt(void);
extern void ungetchr(int c);
extern int getchr(void);
extern char *ttgetdelim(int delim);
extern int tty_get_line(struct buffer_t *lb);

/* file.c */
enum {
        IOMREAD = 0,
        IOMWRITE = 1,
        IOMCREAT = 2,
}; /* type arg to openfile */
extern void putfile(int *a1, int *a2);
extern int file_next_line(struct buffer_t *lb);
extern void closefile(void);
extern int openfile(const char *nm, int type, int wrap);
extern void file_initkey(void);
extern void file_reset_state(void);

/* lines.c */
extern void tempf_quit(void);
extern void tempf_init(void);
extern char *tempf_getline(int tl, struct buffer_t *lbuf);
extern int tempf_putline(struct buffer_t *lbuf);
extern int line_getsub(void);

/* cr.c */
extern void crblock(char *permp, char *buf, int nchar, long startn);
extern char *getkey(int *result, char *buf);
extern char *makekey(char *buf);

/* code.c */
extern struct bralist_t *get_backref(struct code_t *cd, int cidx);
extern int execute(int *addr, struct code_t *cd);
extern int subexecute(struct buffer_t *gb, struct code_t *cd);
extern void compile(struct code_t *cd, int aeof);
#define code_free(cdp_) do { \
        buffer_free(&(cdp_)->lb); \
        buffer_free(&(cdp_)->expbuf); \
} while (0)

/* signal.c */
extern void callunix(void);
extern void signal_lateinit(void);
extern void signal_init(void);
extern void onquit(int signo);
extern void onhup(int signo);
extern void onintr(int signo);

/* subst.c */
extern struct subst_t {
        int newaddr;
        int oldaddr;
} subst;
extern void substitute(int isbuff);

/* buffer.c */
static inline void buffer_reset(struct buffer_t *b)
{
        b->tail = b->count = 0;
}
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
extern int buffer_getc(struct buffer_t *b);
extern void buffer_append(struct buffer_t *dst, struct buffer_t *src);
extern void buffer_strcpy(struct buffer_t *dst, struct buffer_t *src);
extern void buffer_strapp(struct buffer_t *dst, const char *s);
extern void buffer_memapp(struct buffer_t *dst, char *start, char *end);
extern void buffer_guarantee_size(struct buffer_t *b, size_t size);
extern void buffer_free(struct buffer_t *b);

/* append.c */
enum {
        A_GETSUB = 0,
        A_GETLINE,
        A_GETFILE,
        A_GETCOPY,
}; /* "action" args to append() */
extern int append(int action, int *a);

/* address.c */
extern int *address(void);

/* move.c */
extern void move(int cflag);

/* ed.c */
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
extern struct addr_t {
        int *addr1;
        int *addr2;
        int *dot;
        int *dol;
        int *zero;
        unsigned int nlall;
} addrs;
extern struct gbl_options_t {
        int xflag;
        int vflag;
        int kflag;
} options;
extern struct mark_t {
        int names[NMARKS];
        int any;
} marks;
extern long count;
extern int fchange; /* dirty flag */

extern void error(const char *s);
extern void quit(int signo);
extern void newline(void);

/* Quietest error msg. Our most frequently used. */
static inline void qerror(void) { error(""); }

static inline int toeven(int v) { return v & ~01U; }
static inline int iseven(int v) { return (v & 01) == 0; }
static inline int is_address_marked(int a) { return !iseven(a); }
static inline int marked_address(int a) { return a | 01; }
static inline int unmarked_address(int a) { return toeven(a); }
static inline void mark_address(int *a) { *a = marked_address(*a); }
static inline void unmark_address(int *a) { *a = unmarked_address(*a); }

#endif /* ED_H */
