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

typedef struct{
    unsigned int seed;
    int numer;
    pthread_t tid;
    int ile_polek;
    int* polki;
    pthread_mutex_t** mx_polki;
    int* stop;
    pthread_mutex_t* mx_stop;
}worker_data;

void* worker_work(void* arg) {
    worker_data* args = (worker_data*)arg;
    fprintf(stdout, "Worker %d: Reporting for the night shift!\n",args->numer);
    while (1) {
        pthread_mutex_lock(args->mx_stop);
        int s = *args->stop;
        pthread_mutex_unlock(args->mx_stop);
        if (s) {
            //printf("Worker %d: SIGINT\n", args->numer);
            break;
        }
        int a = rand_r(&args->seed)%(args->ile_polek) + 1;
        int b = rand_r(&args->seed)%(args->ile_polek) + 1;
        if (a==b)continue;
        if (a>b) {
            int temp = a;
            a = b;
            b = temp;
        }
        pthread_mutex_lock(args->mx_polki[a]);
        pthread_mutex_lock(args->mx_polki[b]);
        int vala = args->polki[a];
        int valb = args->polki[b];
        if (vala > valb) {
            args->polki[a] = valb;
            args->polki[b] = vala;
        }
        pthread_mutex_unlock(args->mx_polki[a]);
        pthread_mutex_unlock(args->mx_polki[b]);
        msleep(100);
    }
    //printf("Worker %d: konczy\n", args->numer);
    return 0;
}

void shuffle(int n, int* tab) {
    for (int i=1;i<=n;i++) {
        int j = rand()%i + 1;
        int temp = tab[i];
        tab[i] = tab[j];
        tab[j]=temp;
    }
}
void print_tab(int n, int* tab) {
    printf("Polki:\n");
    for (int i=1;i<=n;i++) {
        printf("%d ", tab[i]);
    }
    printf("\n");
}

typedef struct {
    pthread_t tid;
    int * working;
    pthread_mutex_t * mxkon;
    int ile_polek;
    int* polki;
    pthread_mutex_t** mx_polki;
}signal_data_t;

void* signal_routine(void* arg) {
    signal_data_t* args = (signal_data_t*)arg;

    int ile = args->ile_polek;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGUSR1);

    alarm(1);

    while (1) {
        int sig;
        int rc = sigwait(&set, &sig);
        if (rc != 0) {
            continue;
        }

        if (sig == SIGINT) {
            printf("Handluje SIGINT\n");
            pthread_mutex_lock(args->mxkon);
            *args->working = 1;
            pthread_mutex_unlock(args->mxkon);
            pthread_exit(NULL);
        }else if (sig == SIGALRM) {
            printf("Handluje SIGALRM\n");
            for (int i=1;i<=ile;i++) {
                pthread_mutex_lock(args->mx_polki[i]);
            }
            print_tab(args->ile_polek,args->polki);
            for (int i=1;i<=ile;i++) {
                pthread_mutex_unlock(args->mx_polki[i]);
            }
            alarm(1);
        }else if (sig == SIGUSR1) {
            printf("Handluje SIGUSR1\n");
            for (int i=1;i<=args->ile_polek;i++) {
                pthread_mutex_lock(args->mx_polki[i]);
            }
            shuffle(args->ile_polek,args->polki);
            for (int i=1;i<=args->ile_polek;i++) {
                pthread_mutex_unlock(args->mx_polki[i]);
            }
        }
    }
    return NULL;
}

/*  Działanie po uruchomieniu:
 *  ./bitronka n m
 *  ps -C bitronka -o pid=
 *  (zwroci pid)
 *  kill -SIGUSR1 pid
 *  (zeby przetasowac)
 *  kill -SIGINT pid
 *  (zeby zakonczyc)
 */

int main(int argc, char** argv) {
    if (argc < 3) {
        ERR("Usage: <n> <m>");
    }
    int n = atoi(argv[1]); // produkty
    int m = atoi(argv[2]); // pracownicy

    int stop=0;
    pthread_mutex_t mx_stop = PTHREAD_MUTEX_INITIALIZER;

    int polki[n+1];
    for (int i=1;i<=n;i++) {
        polki[i]=i;
    }
    shuffle(n,polki);

    print_tab(n,polki);

    pthread_mutex_t* polki_mx[n+1];
    for (int i=1;i<=n;i++) {
        polki_mx[i] = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(polki_mx[i],NULL);
    }

    sigset_t set, oldset;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGUSR1);

    if (pthread_sigmask(SIG_BLOCK, &set, &oldset) != 0) {
        perror("pthread_sigmask");
        exit(1);
    }

    signal_data_t* sigthread = malloc(sizeof(signal_data_t));
    sigthread->mx_polki = polki_mx;
    sigthread->polki = polki;
    sigthread->working = &stop;
    sigthread->mxkon = &mx_stop;
    sigthread->ile_polek = n;
    if (pthread_create(&sigthread->tid, NULL, signal_routine, sigthread) != 0) {
        perror("Blad przy tworzeniu watku sygnalow");
        exit(1);
    }

    worker_data* pracownicy[m];
    unsigned int master_seed = time(NULL);
    for (int i=0;i<m;i++) {
        pracownicy[i] = malloc(sizeof(worker_data));
        pracownicy[i]->numer = i+1;
        pracownicy[i]->mx_polki = polki_mx;
        pracownicy[i]->polki = polki;
        pracownicy[i]->seed = master_seed+i;
        pracownicy[i]->ile_polek = n;
        pracownicy[i]->stop = &stop;
        pracownicy[i]->mx_stop = &mx_stop;
        if (pthread_create(&pracownicy[i]->tid, NULL, worker_work, pracownicy[i]) != 0) {
            perror("Blad przy tworzeniu watku pracownika");
            exit(1);
        }
    }
    while (1) {
        pthread_mutex_lock(&mx_stop);
        int s = stop;
        pthread_mutex_unlock(&mx_stop);
        if (s) {
            break;
        }
        msleep(100);
    }
    printf("Czekam na roboli\n");
    for (int i=0;i<m;i++) {
        pthread_join(pracownicy[i]->tid,NULL);
    }

    print_tab(n,polki);
    free(sigthread);

    printf("Czyszczę\n");
    for (int i=0;i<m;i++) {
        free(pracownicy[i]);

    }
    pthread_mutex_destroy(&mx_stop);
    for (int i=1;i<=n;i++) {
        pthread_mutex_destroy(polki_mx[i]);
        free(polki_mx[i]);
    }
    printf("Bye\n");
    return 0;
}