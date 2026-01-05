#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    pthread_t tid;
    unsigned int seed;
    int *ile;
    pthread_mutex_t *mxlista;
    pthread_mutex_t *mxkon;
    struct node **lista;
    int *czykon;
} thread_data_t;

struct node {
    int val;
    struct node *next;
};

struct node *head = NULL;

void* start_routine(void* arg) {
    thread_data_t* args = (thread_data_t*)arg;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);

    args->seed = time(NULL) ^ pthread_self();

    while (1) {
        int sig;
        int rc = sigwait(&set, &sig);
        if (rc != 0) {
            continue;
        }

        if (sig == SIGINT) {
            pthread_mutex_lock(args->mxlista);

            if (*args->ile <= 0 || *(args->lista) == NULL) {
                pthread_mutex_unlock(args->mxlista);
                continue;
            }

            int lim = *args->ile;
            int los = rand_r(&(args->seed)) % lim;

            struct node** cur = args->lista;
            struct node* prev = NULL;

            for (int i = 0; i < los; i++) {
                prev = *cur;
                cur = &((*cur)->next);
            }

            struct node* to_remove = *cur;
            if (prev == NULL) {
                // Usuwamy pierwszy element
                *(args->lista) = to_remove->next;
            } else {
                prev->next = to_remove->next;
            }

            printf("usunalem %d-ty element (wartosc: %d)\n", los, to_remove->val);
            free(to_remove);
            (*args->ile)--;

            pthread_mutex_unlock(args->mxlista);

        } else if (sig == SIGQUIT) {
            pthread_mutex_lock(args->mxkon);
            *args->czykon = 1;
            pthread_mutex_unlock(args->mxkon);
            pthread_exit(NULL);
        }
    }
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Program musi zostac wywolany z parametrem k\n");
        exit(1);
    }

    int k = atoi(argv[1]);
    if (k <= 0) {
        fprintf(stderr, "Parametr k musi byc liczba dodatnia\n");
        exit(1);
    }

    // Inicjalizacja listy od 1 do k
    for (int i = k; i > 0; i--) {
        struct node* new = malloc(sizeof(struct node));
        if (!new) {
            perror("malloc");
            exit(1);
        }
        new->val = i;
        new->next = head;
        head = new;
    }

    int stop = 0;

    pthread_mutex_t mlista = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mkon = PTHREAD_MUTEX_INITIALIZER;

    thread_data_t dane;
    dane.ile = &k;
    dane.lista = &head;  // Przekazujemy wskaźnik do wskaźnika
    dane.mxlista = &mlista;
    dane.czykon = &stop;
    dane.mxkon = &mkon;

    // Zablokuj sygnały w głównym wątku
    sigset_t set, oldset;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);

    if (pthread_sigmask(SIG_BLOCK, &set, &oldset) != 0) {
        perror("pthread_sigmask");
        exit(1);
    }

    // Stwórz wątek
    if (pthread_create(&dane.tid, NULL, start_routine, &dane) != 0) {
        perror("Blad przy tworzeniu watku");
        exit(1);
    }

    // Główna pętla
    while (1) {
        pthread_mutex_lock(&mkon);
        int local_stop = stop;
        pthread_mutex_unlock(&mkon);

        if (local_stop) {
            break;
        }

        pthread_mutex_lock(&mlista);
        struct node* current = head;
        printf("Lista (%d elementow): ", k);
        while (current != NULL) {
            printf("%d ", current->val);
            current = current->next;
        }
        printf("\n");
        pthread_mutex_unlock(&mlista);

        sleep(1);
    }

    // Oczekiwanie na zakończenie wątku obsługującego sygnały
    pthread_join(dane.tid, NULL);

    // Czyszczenie pamięci
    printf("\nCzyszczenie pamieci...\n");
    pthread_mutex_lock(&mlista);
    struct node* current = head;
    while (current != NULL) {
        struct node* next = current->next;
        free(current);
        current = next;
    }
    head = NULL;
    pthread_mutex_unlock(&mlista);

    pthread_mutex_destroy(&mlista);
    pthread_mutex_destroy(&mkon);

    printf("Koniec programu.\n");
    return 0;
}