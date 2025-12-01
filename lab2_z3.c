#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include "utils.h"

volatile sig_atomic_t czy_dzialam = 0;
volatile sig_atomic_t licznik = 0;

void child_sigusr1(int sig) {
    czy_dzialam=1;
}
void child_sigusr2(int sig) {
    czy_dzialam=0;
}
void child_sigint(int sig) {
    char buff[20];
    snprintf(buff, sizeof(buff), "%d.txt", getpid());

    int fd = open(buff, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open");
        return;
    }

    char num_str[20] = "";
    snprintf(num_str, sizeof(num_str), "%d\n", licznik);
    bulk_write(fd, num_str, sizeof(num_str));

    close(fd);
    exit(0);
}

void childrenwork(int numer) {
    set_sigact(SIGUSR1,child_sigusr1);
    set_sigact(SIGUSR2,child_sigusr2);
    set_sigact(SIGINT, child_sigint);
    printf("Moje PID: %d, mój numer: %d\n", getpid(), numer);
    struct timespec ts;
    ts.tv_nsec = 10000;
    while (1) {
        if (czy_dzialam==0) {
            nanosleep(&ts,0);
        }else{
            random_time_milliseconds(100,200);
            licznik++;
            printf("%d: %d\n", getpid(), licznik);
        }

    }
}

pid_t* create_children(int ile) {
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
            childrenwork(i);
            exit(0);
        } else {
            // Proces rodzica
            children[i] = pid;
            printf("Utworzono proces potomny o PID: %d\n", pid);
        }
    }

    return children;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Program musi być wywołany z parametrem wejściowym\n");
        exit(1);
    }
    printf("PID: %d\n", getpid());
    sleep(1);
    int n = strtol(argv[1],NULL,10);
    pid_t * dzieci = create_children(n);
    kill(dzieci[0],SIGUSR1);

    sigset_t x;
    sigemptyset(&x);
    sigaddset(&x, SIGINT);
    sigaddset(&x, SIGUSR1);
    sigaddset(&x, SIGUSR2);
    sigprocmask(SIG_BLOCK, &x, NULL);
    int a;
    kill(dzieci[0],SIGUSR1);
    int pracujace = 0;
    while (1) {
        sigwait(&x, &a);
        if (a == SIGINT) {
            for (int i=0;i<n;i++) {
                kill(dzieci[i],SIGINT);
            }
            while (waitpid(-1,NULL,0)>0);
            printf("Dzieci sie skonczyly\n");
            free(dzieci);
            exit(0);
        }
        if (a == SIGUSR1) {
            kill(dzieci[pracujace],SIGUSR2);
            pracujace++;
            if (pracujace >= n) {
                pracujace -= n;
            }
            kill(dzieci[pracujace],SIGUSR1);
        }
    }
    free(dzieci);
    return 0;
}
