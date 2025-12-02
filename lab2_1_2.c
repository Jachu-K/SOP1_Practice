#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

volatile sig_atomic_t ile = 0;

void childrenSIGUSR1(int sig) {
    ile++;
}

void childrenwork(int war) {
    sigset_t x;
    sigemptyset(&x);
    sigaddset(&x, SIGUSR1);
    sigaddset(&x,SIGALRM);
    sigaddset(&x, SIGINT);
    sigprocmask(SIG_BLOCK, &x, NULL);
    int s = random_int(10,100) * 1000;
    int n = war;
    printf("N : %d, S : %d\n", n, s);

    char buff[20];
    snprintf(buff, sizeof(buff), "%d.txt", getpid());
    int sig=0;
    int fd = open(buff, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open");
        return;
    }
    alarm(2);
    while (1) {
        sigwait(&x,&sig);
        if (sig == SIGUSR1) {
            ile++;
            char num_str[s];
            for (int i=0;i<s;i++) {
                num_str[i]=n+'0';
            }
           ssize_t a = bulk_write(fd, num_str, sizeof(num_str));
            printf("PID : %d, Zapisano %zu\n", getpid(), a);
        }
        if (sig == SIGALRM || sig == SIGINT) {
            close(fd);
            printf("PID: %d kończy pracę, wypisałem %d razy\n", getpid(), ile);
            exit(0);
        }
    }





}

void create_children(int war) {

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            return;
        } else if (pid == 0) {
            // Proces potomny
            childrenwork(war);
            exit(0);
        } else {
            // Proces rodzica
            printf("Utworzono proces potomny o PID: %d\n", pid);
        }
}

int main(int argc, char** argv) {
    for (int i = 1; i<argc;i++) {
        create_children(strtol(argv[i],NULL,10));
    }
    struct timespec ts = {ts.tv_sec = 0, .tv_nsec = 1000000L * 10};
    set_sigact(SIGUSR1,SIG_IGN);
    for (int i=0;i<100;i++) {

        nanosleep(&ts,&ts);
        kill(-getpid(),SIGUSR1);
    }
    //set_sigact(SIGINT, SIG_IGN);
    //kill(-getpid(), SIGINT);
    while (waitpid(-1,NULL,0)>0);
    printf("Dzieci zakonczyly prace\n");
    return 0;
}
