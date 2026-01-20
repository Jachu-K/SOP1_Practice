#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
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

    for (int i=0;i<n;i++) {
        pthread_join(gracz1[i]->tid,NULL);
        free(gracz1[i]);
    }
    for (int i=0;i<n;i++) {
        pthread_join(gracz2[i]->tid,NULL);
        free(gracz2[i]);
    }
    free(sigthread);
}