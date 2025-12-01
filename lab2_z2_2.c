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

volatile sig_atomic_t okaszlane = 0;
volatile sig_atomic_t czychore = 0;
volatile sig_atomic_t czyodebrane = 0;
volatile sig_atomic_t wypisz = 0;

void child_SIGUSR1(int sig, siginfo_t * info, void *context) {
    okaszlane = info->si_pid;
    //printf("Child[%d]: %d has coughed at me!",getpid(),info->si_pid);
}

void set_child_sigusr1() {
    struct sigaction sa = {0,0,0,0,0};
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &child_SIGUSR1;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void child_sigterm(int sig) {
    wypisz=1;
}

void child_sigalarm(int sig) {
    czyodebrane=1;
    wypisz=1;
}

void childrenwork(int k,int p,int startsill) {
    czychore = startsill;
    set_child_sigusr1();
    set_sigact(SIGTERM, child_sigterm);
    set_sigact(SIGALRM,child_sigalarm);
    printf("Child[%d] starts day in the kindergarten, ill: %d\n", getpid(), czychore);
    int ile_kaszlniec = 0;
    struct timespec t = {.tv_sec = 0, .tv_nsec = 1000000L * 10};
    while (1) {
        if (wypisz > 0) {
            printf("%d: Coughed %d times and is still in the kindergarten!\n",getpid(),ile_kaszlniec);
            exit(0);
        }
        if (okaszlane > 0) {
            printf("Child[%d]: %d has coughed at me!\n",getpid(),okaszlane);
            if (random_int(1,100) < p) {
                czychore = 1;
            }
            okaszlane=0;
        }
        if (czychore > 0) {
            set_sigact(SIGUSR1,SIG_IGN);
            printf("Child[%d] get sick!\n", getpid());
            alarm(k);
            while (1) {
                random_time_milliseconds(50,200);
                kill(-getppid(),SIGUSR1);
                ile_kaszlniec++;
                printf("Child[%d] is coughing %d\n", getpid(), ile_kaszlniec);
                if (wypisz > 0) {
                    if (czyodebrane > 0) {
                        printf("%d: Coughed %d times and parents picked them up!\n",getpid(),ile_kaszlniec);
                        exit(0);
                    }else{
                        printf("%d: Coughed %d times and is still in the kindergarten!\n",getpid(),ile_kaszlniec);
                        exit(0);
                    }
                }
            }
        }
        nanosleep(&t,NULL);
    }
}

pid_t* create_children(int ile, int k, int p) {
    if (ile <= 0) {
        return NULL;
    }

    pid_t *children = malloc(ile * sizeof(pid_t));
    if (!children) {
        perror("malloc");
        return NULL;
    }

    for (int i = 0; i < ile; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            free(children);
            return NULL;
        } else if (pid == 0) {
            // Proces potomny
            free(children); // dziecko nie potrzebuje tej tablicy
            int isIll = 0;
            if (i == 0) {
                isIll = 1;
            }
            childrenwork(k,p,isIll);
            exit(0);
        } else {
            // Proces rodzica
            children[i] = pid;
            //printf("Utworzono proces potomny o PID: %d\n", pid);
        }
    }

    return children;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Program musi być uruchomiony z 4 parametrami\n");
        exit(1);
    }
    int t,k,n,p;
    t = strtol(argv[1],NULL,10);
    k = strtol(argv[2],NULL,10);
    n = strtol(argv[3],NULL,10);
    p = strtol(argv[4],NULL,10);
    pid_t * dzieci = create_children(n,k,p);

    alarm(t);
    printf("KG[%d]: Alarm has been set for %d sec\n", getpid(), t);

    sigset_t x;
    sigemptyset(&x);
    sigaddset(&x,SIGALRM);
    sigaddset(&x,SIGCHLD);
    int sig=0;
    sigprocmask(SIG_BLOCK, &x, NULL);
    set_sigact(SIGUSR1, SIG_IGN);
    set_sigact(SIGTERM, SIG_IGN);
    while (1) {
        sigwait(&x,&sig);
        if (sig == SIGCHLD) {
            wypisz++;
        }
        if (sig == SIGALRM) {
            printf("KG[%d]: Simulation has ended!\n", getpid());
            kill(-getpid(), SIGTERM);
            printf("%d out of %d children stayed in the kindergarten!\n", n-wypisz, n);
            exit(0);
        }
    }

    while (waitpid(-1,NULL,0)>0);



    free(dzieci);
    return 0;
}
