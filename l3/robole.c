#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAXLINE 4096
#define DEFAULT_STUDENT_COUNT 100
#define ELAPSED(start, end) ((end).tv_sec - (start).tv_sec) + (((end).tv_nsec - (start).tv_nsec) * 1.0e-9)
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define NEXT_DOUBLE(seedptr) ((double)rand_r(seedptr) / (double)RAND_MAX)
#include <signal.h>

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
}signal_data_t;

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
    int iledzialek;
    int * dzialki;
    int * posypane;
    pthread_mutex_t ** mxdzialki;
}trag_data_t;

void* trag_work(void * arg) {
    trag_data_t* args = (trag_data_t*)arg;
    printf("Tragarz %lu zaczyna pracę\n", args->tid);
    while (1) {
        pthread_mutex_lock(args->mxkon);
        int kon = *args->working;
        pthread_mutex_unlock(args->mxkon);
        if (!kon) {
            break;
        }
        int nr_dzialki = rand_r(&args->seed) % args->iledzialek;
        msleep(5 + nr_dzialki);
        pthread_mutex_lock(args->mxdzialki[nr_dzialki]);
        args->dzialki[nr_dzialki] += 5;
        pthread_mutex_unlock(args->mxdzialki[nr_dzialki]);

    }
    printf("Tragarz %lu kończy pracę\n", args->tid);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        perror("za mało argumentów wejściowych");
        exit(1);
    }
    int n = atoi(argv[1]);
    int q = atoi(argv[2]);
    int do_work = 1;
    pthread_mutex_t work_mtx = PTHREAD_MUTEX_INITIALIZER;


    signal_data_t dane;
    dane.mxkon = &work_mtx;
    dane.working = &do_work;

    // Zablokuj sygnały w głównym wątku
    sigset_t set, oldset;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &set, &oldset) != 0) {
        perror("pthread_sigmask");
        exit(1);
    }

    // Stwórz wątek
    if (pthread_create(&dane.tid, NULL, signal_routine, &dane) != 0) {
        perror("Blad przy tworzeniu watku obslugi sygnalow");
        exit(1);
    }
    pthread_mutex_t * dzialki_mx[n];
    int dzialki[n];
    int posypane[n];
    for (int i=0;i<n;i++) {
        dzialki_mx[i] = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(dzialki_mx[i],NULL);
        dzialki[i]=0;
        posypane[i]=0;
    }

    unsigned int master_seed = time(NULL);
    trag_data_t* tragarze[q];
    for (int i=0;i<q;i++) {
        tragarze[i] = malloc(sizeof(trag_data_t));
        tragarze[i]->mxdzialki = dzialki_mx;
        tragarze[i]->mxkon = &work_mtx;
        tragarze[i]->working = &do_work;
        tragarze[i]->dzialki = dzialki;
        tragarze[i]->iledzialek = n;
        tragarze[i]->posypane = posypane;
        tragarze[i]->seed = master_seed+i;
        if (pthread_create(&tragarze[i]->tid, NULL, trag_work, tragarze[i]) != 0) {
            perror("Blad przy tworzeniu watku tragarze");
            exit(1);
        }
    }
    while (1) {
        pthread_mutex_lock(&work_mtx);
        int a = do_work;
        pthread_mutex_unlock(&work_mtx);
        if (!a) {
            break;
        }
    }
    printf("Czekam na workerów...\n");
    for (int i=0;i<q;i++) {
        pthread_join(tragarze[i]->tid,NULL);
        free(tragarze[i]);
    }
    printf("Tragarze zanokczyli\n");

    for (int i=0;i<n;i++) {
        printf("Działka %d : ile_soli = %d, ile_posypanych = %d\n", i, dzialki[i], posypane[i]);
    }

    for (int i=0;i<n;i++) {
        pthread_mutex_destroy(dzialki_mx[i]);
        free(dzialki_mx[i]);
    }
    pthread_mutex_destroy(&work_mtx);
    return 0;

}