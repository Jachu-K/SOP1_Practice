#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

// Struktura do przekazania danych do wątku
typedef struct {
    int thread_id;
    unsigned int seed;  // Każdy wątek ma swój własny seed
    int count;         // Liczba losowań do wykonania
    double res;
} thread_data_t;

void* start_routine(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;

    for (int i = 0; i < data->count; i++) {
        // Losowanie liczby z przedziału [0, 1)
        double random_valuex = (double)rand_r(&data->seed) / (double)RAND_MAX;
        double random_valuey = (double)rand_r(&data->seed) / (double)RAND_MAX;
        if (random_valuex * random_valuex + random_valuey * random_valuey <= 1) {
            (data->res)++;
        }
    }
    (data->res) /= (double)(data->count);
    return 0;
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        fprintf(stderr, "Użycie: %s <liczba_wątków> <liczba_losowań_na_wątek>\n", argv[0]);
        exit(1);
    }

    int k = atoi(argv[1]);  // liczba wątków
    int n = atoi(argv[2]);  // liczba losowań na wątek

    pthread_t threads[k];
    thread_data_t thread_data[k];

    // Inicjalizacja generatora czasu dla głównego wątku
    unsigned int master_seed = (unsigned int)time(NULL);

    // Tworzenie wątków
    for (int i = 0; i < k; i++) {
        // Każdy wątek dostaje unikalny seed
        thread_data[i].thread_id = i;
        thread_data[i].seed = master_seed + i;  // Różne seedy dla każdego wątku
        thread_data[i].count = n;
        thread_data[i].res = 0.0;

        if (pthread_create(&threads[i], NULL, start_routine, &thread_data[i]) != 0) {
            perror("Błąd przy tworzeniu wątku");
            exit(1);
        }
    }

    double cum = 0.0;
    double cnt = 0.0;

    // Oczekiwanie na zakończenie wszystkich wątków
    for (int i = 0; i < k; i++) {
        pthread_join(threads[i], NULL);
        cum += thread_data[i].res;
        cnt ++;
    }
    cum /= cnt;
    cum *= 4;
    fprintf(stdout, "Obliczone pi : %10lf", cum);
    return 0;
}