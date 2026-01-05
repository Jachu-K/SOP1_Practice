#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#define NEXT_DOUBLE(seedptr) ((double)rand_r(seedptr) / (double)RAND_MAX)
// Struktura do przekazania danych do wątku
typedef struct {
    pthread_t tid;
    unsigned int seed;
    int *pBallsThrown;
    int *pBallsWaiting;
    int *bins;
    pthread_mutex_t *mxBins;
    pthread_mutex_t *pmxBallsThrown;
    pthread_mutex_t *pmxBallsWaiting;

} thread_data_t;


int throwBall(unsigned int *seedptr)
{
    int result = 0;
    for (int i = 0; i < 11 - 1; i++)
        if (NEXT_DOUBLE(seedptr) > 0.5)
            result++;
    return result;
}
void* start_routine(void* arg) {
    thread_data_t* args = (thread_data_t*)arg;
    while (1)
    {
        pthread_mutex_lock(args->pmxBallsWaiting);
        if (*args->pBallsWaiting > 0)
        {
            (*args->pBallsWaiting) -= 1;
            pthread_mutex_unlock(args->pmxBallsWaiting);
        }
        else
        {
            pthread_mutex_unlock(args->pmxBallsWaiting);
            break;
        }
        int binno = throwBall(&args->seed);
        pthread_mutex_lock(&args->mxBins[binno]);
        args->bins[binno] += 1;
        pthread_mutex_unlock(&args->mxBins[binno]);
        pthread_mutex_lock(args->pmxBallsThrown);
        (*args->pBallsThrown) += 1;
        pthread_mutex_unlock(args->pmxBallsThrown);
    }
    return NULL;

}


int main(int argc, char ** argv) {
    if (argc < 3) {
        fprintf(stderr, "Użycie: %s <liczba_wątków> <calkowita_liczba_kulek>\n", argv[0]);
        exit(1);
    }

    int k = atoi(argv[1]);  // liczba wątków
    int n = atoi(argv[2]);  // liczba kulek

    pthread_t threads[k];
    thread_data_t thread_data[k];

    // Inicjalizacja generatora czasu dla głównego wątku
    unsigned int master_seed = (unsigned int)time(NULL);

    int ballsT = 0;
    int ballsW = n;
    int bin[11] = {0,0,0,0,0,0,0,0,0,0,0};
    pthread_mutex_t mxBallsThrown = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mxBallsWaiting = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mx[11];
    for (int i=0;i<10;i++) {
        pthread_mutex_init(&mx[i],NULL);
    }

    // Tworzenie wątków
    for (int i = 0; i < k; i++) {
        // Każdy wątek dostaje unikalny seed
        thread_data[i].tid = i;
        thread_data[i].seed = master_seed + i;  // Różne seedy dla każdego wątku
        thread_data[i].pBallsThrown = &ballsT;
        thread_data[i].pBallsWaiting = &ballsW;
        thread_data[i].bins = bin;
        thread_data[i].pmxBallsThrown = &mxBallsThrown;
        thread_data[i].pmxBallsWaiting = &mxBallsWaiting;
        thread_data[i].mxBins = mx;

        if (pthread_create(&threads[i], NULL, start_routine, &thread_data[i]) != 0) {
            perror("Błąd przy tworzeniu wątku");
            exit(1);
        }
    }

    // Oczekiwanie na zakończenie wszystkich wątków
    while (1) {
        sleep(1);
        pthread_mutex_lock(&mxBallsThrown);
        fprintf(stdout, "kul %d \n", ballsT);
        if (ballsT == n) {
            break;
        }
        pthread_mutex_unlock(&mxBallsThrown);
    }
    for (int i=0;i<11;i++) {
        fprintf(stdout, "bin %d: %d ballz\n", i+1, bin[i]);
    }
    return 0;
}