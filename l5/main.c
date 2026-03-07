#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}

typedef struct {
    pthread_t tid;
    int * working;
    pthread_mutex_t * mxkon;
} signal_data_t;

void* signal_routine(void* arg) {
    signal_data_t* args = (signal_data_t*)arg;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    while (1) {
        int sig;
        int rc = sigwait(&set, &sig);
        if (rc != 0) {
            continue;
        }

        if (sig == SIGINT) {
            printf("Handluje SIGINT\n");
            pthread_mutex_lock(args->mxkon);
            *args->working = 0;
            pthread_mutex_unlock(args->mxkon);
            pthread_exit(NULL);
        }
    }
    return NULL;
}

typedef struct {
    unsigned int seed;
    pthread_t tid;
    int * working;
    pthread_mutex_t * mxkon;
} thread1_t;

void* thread1_work(void * arg) {
    thread1_t* data = (thread1_t*)arg;
    printf("Thread1 starting\n");
    int a = rand_r(&data->seed)%300+1;
    msleep(a);

    printf("Thread1 finishing\n");
    return NULL;
}

typedef struct {
    unsigned int seed;
    pthread_t tid;
    int * working;
    pthread_mutex_t * mxkon;
} thread2_t;

void* thread2_work(void * arg) {
    thread2_t* data = (thread2_t*)arg;
    printf("Thread2 starting\n");
    int a = rand_r(&data->seed)%300+1;
    msleep(a);

    printf("Thread2 finishing\n");
    return NULL;
}

typedef struct {
    unsigned int seed;
    pthread_t tid;
    int * working;
    pthread_mutex_t * mxkon;
    int write_fd;  // do wysyłania do main
    int read_fd;   // do odbierania od main
    int id;
} thread3_t;

void* thread3_work(void * arg) {
    thread3_t* data = (thread3_t*)arg;
    printf("Thread3[%d] starting\n", data->id);

    char buffer[256];
    fd_set readfds;
    struct timeval tv;

    while(1) {
        pthread_mutex_lock(data->mxkon);
        int should_work = *data->working;
        pthread_mutex_unlock(data->mxkon);

        if(!should_work) {
            break;
        }

        // Sprawdź czy pipe jest nadal otwarty do main
        FD_ZERO(&readfds);
        FD_SET(data->read_fd, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(data->read_fd + 1, &readfds, NULL, NULL, &tv);

        if(ret < 0) {
            if(errno == EINTR) continue;
            printf("Thread3[%d]: Select error, exiting\n", data->id);
            break;
        }

        if(ret > 0 && FD_ISSET(data->read_fd, &readfds)) {
            // Czytaj dane od main
            ssize_t bytes_read = read(data->read_fd, buffer, sizeof(buffer) - 1);

            if(bytes_read <= 0) {
                if(bytes_read == 0) {
                    printf("Thread3[%d]: Main closed pipe, exiting\n", data->id);
                } else {
                    perror("Thread3 read error");
                }
                break;
            }

            buffer[bytes_read] = '\0';
            printf("Thread3[%d] received: %s\n", data->id, buffer);

            // Symuluj pracę
            int a = rand_r(&data->seed)%200+1;
            msleep(a);

            // Wyślij odpowiedź do main
            char response[256];
            snprintf(response, sizeof(response), "Response from Thread3[%d] to: %s", data->id, buffer);

            ssize_t bytes_written = write(data->write_fd, response, strlen(response));

            if(bytes_written < 0) {
                if(errno == EPIPE) {
                    printf("Thread3[%d]: Broken pipe (SIGPIPE) - main closed write pipe, exiting\n", data->id);
                } else {
                    perror("Thread3 write error");
                }
                break;
            }
        }
    }

    // Zamknij deskryptory przed zakończeniem
    if(data->read_fd >= 0) {
        close(data->read_fd);
        data->read_fd = -1;
    }

    printf("Thread3[%d] finishing\n", data->id);
    return NULL;
}

int main(int argc,char** argv) {
    if (argc < 2) {
        printf("Program musi być wywołany z parametrem n\n");
        exit(1);
    }
    int n = atoi(argv[1]);

    int working = 1;
    pthread_mutex_t stop_mx = PTHREAD_MUTEX_INITIALIZER;
    int master_seed = time(NULL);

    sigset_t set, oldset;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &set, &oldset) != 0) {
        perror("pthread_sigmask");
        exit(1);
    }

    signal_data_t* sigthread = malloc(sizeof(signal_data_t));
    sigthread->working = &working;
    sigthread->mxkon = &stop_mx;
    if (pthread_create(&sigthread->tid, NULL, signal_routine, sigthread) != 0) {
        perror("Blad przy tworzeniu watku sygnalow");
        exit(1);
    }

    // Tablica potoków do thread3 (jeden dla każdego wątku)
    int pipes_to_thread3[n][2];  // [0] = read, [1] = write (main -> thread)
    // Jeden wspólny potok od thread3 do main
    int pipe_from_thread3[2];    // [0] = read (main), [1] = write (thread -> main)

    // Utwórz wspólny potok od thread3 do main
    if(pipe(pipe_from_thread3) < 0) {
        ERR("pipe_from_thread3");
    }

    thread3_t * gracz3[n];
    for (int i=0;i<n;i++) {
        // Utwórz indywidualny potok dla każdego thread3
        if(pipe(pipes_to_thread3[i]) < 0) {
            ERR("pipes_to_thread3");
        }

        gracz3[i] = malloc(sizeof(thread3_t));
        gracz3[i]->seed = master_seed+i+200;
        gracz3[i]->mxkon = &stop_mx;
        gracz3[i]->working = &working;
        gracz3[i]->id = i;

        // W thread3: read = koniec do czytania od main, write = koniec do pisania do main
        gracz3[i]->read_fd = pipes_to_thread3[i][0];    // thread czyta z tego
        gracz3[i]->write_fd = pipe_from_thread3[1];     // thread pisze do wspólnego pipe

        // Zamknij niepotrzebne końce w main dla tego potoku
        close(pipes_to_thread3[i][1]);  // main nie będzie pisał przez ten koniec (użyje innego)

        if (pthread_create(&gracz3[i]->tid, NULL, thread3_work, gracz3[i]) != 0) {
            perror("Blad przy tworzeniu watku gracza3");
            exit(1);
        }
    }

    // Zamknij zapisowy koniec wspólnego pipe w main (main będzie czytał)
    close(pipe_from_thread3[1]);

    thread1_t * gracz1[n];
    for (int i=0;i<n;i++) {
        gracz1[i] = malloc(sizeof(thread1_t));
        gracz1[i]->seed = master_seed+i;
        gracz1[i]->mxkon = &stop_mx;
        gracz1[i]->working = &working;
        if (pthread_create(&gracz1[i]->tid, NULL, thread1_work, gracz1[i]) != 0) {
            perror("Blad przy tworzeniu watku gracza1");
            exit(1);
        }
    }

    thread2_t * gracz2[n];
    for (int i=0;i<n;i++) {
        gracz2[i] = malloc(sizeof(thread2_t));
        gracz2[i]->seed = master_seed+i+100;
        gracz2[i]->mxkon = &stop_mx;
        gracz2[i]->working = &working;
        if (pthread_create(&gracz2[i]->tid, NULL, thread2_work, gracz2[i]) != 0) {
            perror("Blad przy tworzeniu watku gracza2");
            exit(1);
        }
    }

    // Główna pętla main - wysyłanie do thread3 i odbieranie odpowiedzi
    fd_set readfds;
    char buffer[256];
    int active_threads = n;

    while(working && active_threads > 0) {
        // Wysyłaj dane do thread3
        for(int i = 0; i < n; i++) {
            if(working) {
                char message[256];
                snprintf(message, sizeof(message), "Message from main to thread3[%d]", i);

                // Użyj zapisowego końca indywidualnego pipe
                ssize_t bytes_written = write(pipes_to_thread3[i][1], message, strlen(message));

                if(bytes_written < 0) {
                    if(errno == EPIPE) {
                        printf("Main: Broken pipe to thread3[%d]\n", i);
                        active_threads--;
                    } else {
                        perror("Main write error");
                    }
                } else {
                    printf("Main sent to thread3[%d]: %s\n", i, message);
                }
            }
        }

        // Odczytuj odpowiedzi od thread3
        FD_ZERO(&readfds);
        FD_SET(pipe_from_thread3[0], &readfds);

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(pipe_from_thread3[0] + 1, &readfds, NULL, NULL, &tv);

        if(ret < 0) {
            if(errno == EINTR) continue;
            perror("Main select error");
            break;
        }

        if(ret > 0 && FD_ISSET(pipe_from_thread3[0], &readfds)) {
            // Czytaj odpowiedź
            ssize_t bytes_read = read(pipe_from_thread3[0], buffer, sizeof(buffer) - 1);

            if(bytes_read <= 0) {
                if(bytes_read == 0) {
                    printf("Main: All threads closed their write pipes\n");
                } else {
                    perror("Main read error");
                }
                break;
            }

            buffer[bytes_read] = '\0';
            printf("Main received: %s\n", buffer);
        }

        msleep(2000); // Czekaj 2 sekundy przed następną serią
    }

    // Zakończenie - czekaj na wątki
    for (int i=0;i<n;i++) {
        pthread_join(gracz1[i]->tid,NULL);
        free(gracz1[i]);
    }

    for (int i=0;i<n;i++) {
        pthread_join(gracz2[i]->tid,NULL);
        free(gracz2[i]);
    }

    // Czekaj na thread3 i zwolnij zasoby
    for (int i=0;i<n;i++) {
        pthread_join(gracz3[i]->tid,NULL);

        // Zamknij pozostałe deskryptory
        close(pipes_to_thread3[i][1]); // zapisowy koniec w main
        // czytający koniec został zamknięty przez wątek

        free(gracz3[i]);
    }

    // Zamknij czytający koniec wspólnego pipe
    close(pipe_from_thread3[0]);

    free(sigthread);

    return 0;
}
