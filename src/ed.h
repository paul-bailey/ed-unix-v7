#ifndef ED_H
#define ED_H

extern int peekc;
extern int lastc;
extern char *globp;
extern int listf;

/* term.c */
extern void putchr(int ac);
extern int getchr(void);
extern void putstr(char *sp);

#endif /* ED_H */
