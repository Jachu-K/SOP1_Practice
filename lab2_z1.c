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
#include <stdio.h>
#include "utils.h"
#include <sys/file.h>


typedef struct {
    int expected_writes;  // Oczekiwana liczba zapisów (wartość 's')
    int actual_writes;    // Rzeczywista liczba zapisów
    time_t start_time;    // Czas rozpoczęcia pracy dziecka
} child_data_t;

child_data_t *child_data = NULL;

void childrenwork(int war) {
    printf("Proces potomny (PID: %d, PPID: %d) pracuje...\n", getpid(), getppid());

    int a = random_int(10,100);  // To jest 's' - liczba oczekiwanych zapisów
    printf("%d - n, %d - s\n", war, a);

    // Inicjalizacja danych dziecka
    child_data_t my_data = {
        .expected_writes = a,
        .actual_writes = 0,
        .start_time = time(NULL)
    };

    char buff[20];
    snprintf(buff, sizeof(buff), "%d.txt", getpid());

    int fd = open(buff, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open");
        return;
    }

    char num_str[20];
    snprintf(num_str, sizeof(num_str), "%d\n", war);

    sigset_t x;
    sigemptyset(&x);
    sigaddset(&x, SIGUSR1);
    sigaddset(&x, SIGINT);
    sigprocmask(SIG_BLOCK, &x, NULL);

    int sig = 0;
    while (1) {
        // Sprawdź czas życia - jeśli przekroczono 1 sekundę, zakończ
        if (time(NULL) - my_data.start_time >= 1) {
            printf("Proces potomny (PID: %d) kończy pracę - przekroczono czas życia\n", getpid());

            // Dopisz brakujące bloki
            int missing_writes = my_data.expected_writes - my_data.actual_writes;
            if (missing_writes > 0) {
                printf("Dopisuję %d brakujących bloków\n", missing_writes);
                for (int i = 0; i < missing_writes; i++) {
                    // OCHRONA ZAPISU PRZEZ BLOKADĘ PLIKU
                    if (flock(fd, LOCK_EX) == -1) {
                        perror("flock");
                    }

                    if (bulk_write(fd, num_str, strlen(num_str)) == -1) {
                        perror("bulk_write");
                    } else {
                        my_data.actual_writes++;
                    }

                    flock(fd, LOCK_UN);  // Zwolnij blokadę
                }
            }

            printf("Proces %d: oczekiwano %d zapisów, wykonano %d\n",
                   getpid(), my_data.expected_writes, my_data.actual_writes);
            close(fd);
            exit(0);
        }

        sigwait(&x, &sig);

        if (sig == SIGINT) {
            printf("Proces potomny (PID: %d) kończy pracę po SIGINT\n", getpid());

            // Dopisz brakujące bloki przed zakończeniem
            int missing_writes = my_data.expected_writes - my_data.actual_writes;
            if (missing_writes > 0) {
                printf("Dopisuję %d brakujących bloków przed SIGINT\n", missing_writes);
                for (int i = 0; i < missing_writes; i++) {
                    // OCHRONA ZAPISU
                    if (flock(fd, LOCK_EX) == -1) {
                        perror("flock");
                    }

                    if (bulk_write(fd, num_str, strlen(num_str)) == -1) {
                        perror("bulk_write");
                    } else {
                        my_data.actual_writes++;
                    }

                    flock(fd, LOCK_UN);
                }
            }

            close(fd);
            exit(0);
        }

        if (sig == SIGUSR1) {
            printf("Proces potomny (PID: %d) zapisuje (%d/%d)\n",
                   getpid(), my_data.actual_writes + 1, my_data.expected_writes);

            // OCHRONA ZAPISU DO PLIKU - BLOKADA
            if (flock(fd, LOCK_EX) == -1) {
                perror("flock");
            }

            if (bulk_write(fd, num_str, strlen(num_str)) == -1) {
                perror("bulk_write");
            } else {
                my_data.actual_writes++;
            }

            flock(fd, LOCK_UN);  // Zwolnij blokadę
        }
    }
}

void parentwork() {
    printf("Proces rodzicielski (PID: %d) pracuje...\n", getpid());
    // Tutaj kod specyficzny dla rodzica
    int ile = 100;
    while (ile--) {
        kill(-getpid(),SIGUSR1);
        printf("Rodzic wysyła SIGUSR1\n");
        struct timespec ts = {
            .tv_nsec = (10) * 1000000L
        };
        nanosleep(&ts,NULL);
    }
    while (waitpid(-1, NULL, 0) > 0);
}

pid_t* create_children(int ile, int war) {
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
            childrenwork(war);
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
    set_sigact(SIGUSR1,SIG_IGN);
    for (int i=1;i<argc;i++) {
        free(create_children(1,argv[i][0]-'0'));
    }
    parentwork();
}
