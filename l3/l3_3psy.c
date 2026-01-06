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

/* ============================================
   STRUKTURY DANYCH I ZMIENNE GLOBALNE
   ============================================ */
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

// Flaga zatrzymania programu

// Struktura danych dla wątku obsługi sygnałów
typedef struct {
    int *stop_flag_ptr;
} signal_handler_data_t;

// Struktura danych dla dodatkowego wątku (do wypełnienia)
typedef struct {
    pthread_mutex_t * stop_mutex;
    pthread_mutex_t * meta_mtx;
    int * ileMeta;
    int numer;
    int * dlugToru;
    unsigned int seed;
    pthread_t tid;
    int * stop_cond;
    pthread_mutex_t** mtx;
    int * tab;
} worker_thread_data_t;


/* ============================================
   WĄTEK OBSŁUGI SYGNAŁÓW
   ============================================ */

void* signal_handler_thread(void* arg) {
    signal_handler_data_t* args = (signal_handler_data_t*)arg;

    // Ustaw maskę sygnałów do obsługi
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);


    printf("Watek obslugi sygnalow gotowy\n");

    while (1) {
        int signal_received;
        int rc = sigwait(&signal_set, &signal_received);

        if (rc != 0) {
            fprintf(stderr, "Blad sigwait: %d\n", rc);
            continue;
        }

        if (signal_received == SIGINT) {
            *args->stop_flag_ptr = 1;
            break;
        }
    }

    return NULL;
}

/* ============================================
   DODATKOWY WĄTEK ROBOCZY
   ============================================ */

void* worker_thread(void* arg) {
    worker_thread_data_t* data = (worker_thread_data_t*)arg;

    printf("Worker thread started with tid: %lu\n", data->tid);
    pthread_mutex_lock(data->mtx[0]);
    data->tab[0]++;
    pthread_mutex_unlock(data->mtx[0]);
    int pos = 0;
    // TODO: Implementuj swoją logikę tutaj
    // Przykład: Prosty cykl pracy
    while (1) {
        // Sprawdź flagę stop
        /*pthread_mutex_lock(data->stop_mutex);
        int should_stop = *data->stop_cond;
        pthread_mutex_unlock(data->stop_mutex);*/

        /*if (should_stop) {
            break;
        }*/

        // TODO: Wykonaj pracę wątku
        int ile = rand_r(&data->seed)%1321 + 200;
        msleep(ile);
        int ruch = rand_r(&data->seed)%5 + 1;
        if (ruch + pos < *data->dlugToru) {
            pthread_mutex_lock(data->mtx[pos]);
            data->tab[pos]--;
            pthread_mutex_unlock(data->mtx[pos]);
            pos += ruch;
            pthread_mutex_lock(data->mtx[pos]);
            ruch = data->tab[pos];
            data->tab[pos]++;
            pthread_mutex_unlock(data->mtx[pos]);
        }else {
            int odl = *data->dlugToru - pos - 1;
            int npos = *data->dlugToru - (ruch - odl + 1);
            pthread_mutex_lock(data->mtx[pos]);
            data->tab[pos]--;
            pthread_mutex_unlock(data->mtx[pos]);
            pos = npos;
            pthread_mutex_lock(data->mtx[pos]);
            ruch = data->tab[pos];
            data->tab[pos]++;
            pthread_mutex_unlock(data->mtx[pos]);
        }
        if (ruch) {
            printf("waf waf waf\n");
        }
        printf("Pies numer %d nowa pozycja %d\n", data->numer, pos);
        if (pos == *data->dlugToru - 1) {
            pthread_mutex_lock(data->meta_mtx);
            (*(data->ileMeta))++;
            int ktory = *data->ileMeta;
            pthread_mutex_unlock(data->meta_mtx);
            printf("Pies numer %d na mecie jako %d\n", data->numer, ktory);
            break;
        }

    }

    printf("Worker thread finishing\n");
    return NULL;
}


/* ============================================
   FUNKCJA GŁÓWNA
   ============================================ */

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Uzycie: %s <n> <m>\n", argv[0]);
        fprintf(stderr, "n - dlugosc toru\n");
        fprintf(stderr, "m - liczba psów\n");
        exit(1);
    }

    int n = atoi(argv[1]);
    if (n <= 20) {
        fprintf(stderr, "Musi zachodzic n > 20\n");
        exit(1);
    }
    int m = atoi(argv[2]);
    if (m<=2) {
        fprintf(stderr, "Musi zachodzić m > 2");
        exit(1);
    }

    printf("Program startuje z n = %d, m = %d\n", n, m);
    int tab[n];
    for (int i=0;i<n;i++) {
        tab[i]=0;
    }
    // Alokacja tablicy mutexów - każdy mutex musi być oddzielnie alokowany!
    pthread_mutex_t **muteksy = malloc(n * sizeof(pthread_mutex_t*));
    if (muteksy == NULL) {
        ERR("malloc failed for muteksy");
    }

    for (int i = 0; i < n; i++) {
        muteksy[i] = malloc(sizeof(pthread_mutex_t));
        if (muteksy[i] == NULL) {
            ERR("malloc failed for muteksy[i]");
        }
        pthread_mutex_init(muteksy[i], NULL);
    }
    int meta = 0;
    pthread_mutex_t metaMtx = PTHREAD_MUTEX_INITIALIZER;

    unsigned int master_seed = time(NULL);
    int stop = 0;
    pthread_mutex_t stop_mtx = PTHREAD_MUTEX_INITIALIZER;

    worker_thread_data_t *worker_data_array = malloc(m * sizeof(worker_thread_data_t));
    if (worker_data_array == NULL) {
        ERR("malloc failed for worker_data_array");
    }

    pthread_t *psy = malloc(m * sizeof(pthread_t));
    if (psy == NULL) {
        ERR("malloc failed for psy");
    }

    // Zablokuj sygnały w głównym wątku
    sigset_t signal_set, old_signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &signal_set, &old_signal_set) != 0) {
        perror("pthread_sigmask");
        exit(1);
    }

    for (int i = 0; i < m; i++) {
        // Inicjalizacja struktury dla wątku (już alokowanej!)
        worker_data_array[i].dlugToru = &n;
        worker_data_array[i].ileMeta = &meta;
        worker_data_array[i].meta_mtx = &metaMtx;
        worker_data_array[i].mtx = muteksy;
        worker_data_array[i].numer = i + 1;
        worker_data_array[i].seed = master_seed + i + 1; // Różne seedy dla każdego psa
        worker_data_array[i].stop_cond = &stop;
        worker_data_array[i].tab = tab;
        worker_data_array[i].stop_mutex = &stop_mtx;

        // Utwórz wątek roboczy
        if (pthread_create(&psy[i], NULL, worker_thread, &worker_data_array[i]) != 0) {
            perror("Blad przy tworzeniu watku roboczego");
            exit(1);
        }
        worker_data_array[i].tid = psy[i];
    }



    signal_handler_data_t dane;
    dane.stop_flag_ptr = &stop;
    pthread_t watekSig;
    if (pthread_create(&watekSig, NULL, signal_handler_thread, &dane) != 0) {
        perror("Blad przy tworzeniu watku sygnalow");
        exit(1);
    }
    int przerwano = 1;
    while (stop == 0) {
        msleep(2000);
        pthread_mutex_lock(&metaMtx);
        int ileSkonczylo = meta;
        pthread_mutex_unlock(&metaMtx);
        if (ileSkonczylo == m) {
            printf("Wszystkie psy skonczyly\n");
            przerwano = 0;
            break;
        }
        for (int i=0;i<n;i++) {
            pthread_mutex_lock(muteksy[i]);
        }
        for (int i=0;i<n;i++) {
            printf("Pole %d : %d psów\n", i, tab[i]);
        }
        for (int i=0;i<n;i++) {
            pthread_mutex_unlock(muteksy[i]);
        }
    }
    if (przerwano) {
        printf("Przerwano wyścig!\n");
    }
    // Czekaj na zakończenie wątków
    printf("Oczekiwanie na zakonczenie watkow...\n");
    for (int i=0;i<m;i++) {
        pthread_cancel(psy[i]);
        pthread_join(psy[i], NULL);
    }
    // TODO: Odkomentuj gdy zaimplementujesz worker_thread
    // pthread_join(worker_data.tid, NULL);

    // Sprzątanie
    printf("Czyszczenie zasobow...\n");
    // Zwolnienie pamięci
    free(psy);
    free(worker_data_array);

    pthread_mutex_destroy(&stop_mtx);
    pthread_mutex_destroy(&metaMtx);

    for (int i = 0; i < n; i++) {
        pthread_mutex_destroy(muteksy[i]);
        free(muteksy[i]);
    }
    free(muteksy);

    printf("Program zakonczony poprawnie.\n");
    return 0;
}