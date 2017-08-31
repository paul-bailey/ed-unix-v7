#include "ed.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

static void (*oldintr)(int) = SIG_ERR;
static void (*oldhup)(int) = SIG_ERR;
static void (*oldquit)(int) = SIG_ERR;

void
onintr(int signo)
{
        signal(SIGINT, onintr);
        putchar('\n');
        qerror();
}

void
onhup(int signo)
{
        signal(SIGINT, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
        quit(signo);
}

void
onquit(int signo)
{
        quit(signo);
}

void
signal_init(void)
{
        oldquit = signal(SIGQUIT, SIG_IGN);
        oldhup = signal(SIGHUP, SIG_IGN);
        oldintr = signal(SIGINT, SIG_IGN);
        if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
                signal(SIGTERM, onquit);
}

void
signal_lateinit(void)
{
        if (oldintr == SIG_ERR)
                signal(SIGINT, onintr);
        if (oldhup == SIG_ERR)
                signal(SIGHUP, onhup);
}

void
callunix(void)
{
        /* TODO: This should be replaced with the simpler "system()" */
        void (*savint)(int);
        pid_t pid, rpid;
        int retcode;

        pid = fork();
        if (pid == (pid_t)-1) {
                /* TODO: error? */
                return;
        } else if (pid == (pid_t)0) {
                /* child process */
                signal(SIGHUP, oldhup);
                signal(SIGQUIT, oldquit);
                execl("/bin/sh", "sh", "-t", (char *)NULL);
                exit(EXIT_FAILURE);
        } else {
                savint = signal(SIGINT, SIG_IGN);
                while ((rpid = wait(&retcode)) != pid && rpid != (pid_t)-1)
                        ;
                signal(SIGINT, savint);
        }
}
