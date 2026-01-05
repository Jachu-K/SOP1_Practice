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

// Struktura węzła listy
struct node {
    int val;
    struct node *next;
};

// Globalna głowa listy
struct node *head = NULL;

// Flaga zatrzymania programu
volatile sig_atomic_t stop_flag = 0;

// Mutexy dla synchronizacji
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stop_mutex = PTHREAD_MUTEX_INITIALIZER;

// Struktura danych dla wątku obsługi sygnałów
typedef struct {
    pthread_t tid;
    unsigned int seed;
    int *element_count;
    pthread_mutex_t *list_mutex_ptr;
    pthread_mutex_t *stop_mutex_ptr;
    struct node **list_head;
    volatile sig_atomic_t *stop_flag_ptr;
} signal_handler_data_t;

// Struktura danych dla dodatkowego wątku (do wypełnienia)
typedef struct {
    pthread_t tid;
    // TODO: Dodaj potrzebne pola
    int some_parameter;
    // ... inne pola specyficzne dla zadania
} worker_thread_data_t;

/* ============================================
   FUNKCJE POMOCNICZE DLA LISTY
   ============================================ */

// Funkcja inicjalizująca listę od 1 do k
void initialize_list(int k) {
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
}

// Funkcja wyświetlająca listę
void print_list() {
    struct node* current = head;
    while (current != NULL) {
        printf("%d ", current->val);
        current = current->next;
    }
    printf("\n");
}

// Funkcja czyszcząca listę
void cleanup_list() {
    struct node* current = head;
    while (current != NULL) {
        struct node* next = current->next;
        free(current);
        current = next;
    }
    head = NULL;
}

/* ============================================
   WĄTEK OBSŁUGI SYGNAŁÓW
   ============================================ */

void* signal_handler_thread(void* arg) {
    signal_handler_data_t* args = (signal_handler_data_t*)arg;

    // Ustaw maskę sygnałów do obsługi
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGQUIT);

    // Inicjalizacja generatora liczb losowych
    args->seed = time(NULL) ^ pthread_self();

    printf("Watek obslugi sygnalow gotowy\n");

    while (1) {
        int signal_received;
        int rc = sigwait(&signal_set, &signal_received);

        if (rc != 0) {
            fprintf(stderr, "Blad sigwait: %d\n", rc);
            continue;
        }

        if (signal_received == SIGINT) {
            pthread_mutex_lock(args->list_mutex_ptr);

            // Sprawdź czy lista nie jest pusta
            if (*(args->element_count) <= 0 || *(args->list_head) == NULL) {
                pthread_mutex_unlock(args->list_mutex_ptr);
                printf("Lista jest pusta, brak elementow do usuniecia\n");
                continue;
            }

            int list_size = *(args->element_count);
            int random_index = rand_r(&(args->seed)) % list_size;

            // Znajdź element do usunięcia
            struct node** current = args->list_head;
            struct node* previous = NULL;

            for (int i = 0; i < random_index; i++) {
                previous = *current;
                current = &((*current)->next);
            }

            // Usuń element
            struct node* to_remove = *current;
            if (previous == NULL) {
                // Usuwamy pierwszy element
                *(args->list_head) = to_remove->next;
            } else {
                previous->next = to_remove->next;
            }

            printf("Usunieto %d-ty element (wartosc: %d)\n",
                   random_index, to_remove->val);
            free(to_remove);
            (*(args->element_count))--;

            pthread_mutex_unlock(args->list_mutex_ptr);

        } else if (signal_received == SIGQUIT) {
            printf("Otrzymano SIGQUIT, konczenie pracy...\n");
            pthread_mutex_lock(args->stop_mutex_ptr);
            *(args->stop_flag_ptr) = 1;
            pthread_mutex_unlock(args->stop_mutex_ptr);
            pthread_exit(NULL);
        }
    }

    return NULL;
}

/* ============================================
   DODATKOWY WĄTEK ROBOCZY
   ============================================ */

void* worker_thread(void* arg) {
    worker_thread_data_t* data = (worker_thread_data_t*)arg;

    printf("Worker thread started with parameter: %d\n", data->some_parameter);

    // TODO: Implementuj swoją logikę tutaj
    // Przykład: Prosty cykl pracy
    while (1) {
        // Sprawdź flagę stop
        pthread_mutex_lock(&stop_mutex);
        int should_stop = stop_flag;
        pthread_mutex_unlock(&stop_mutex);

        if (should_stop) {
            break;
        }

        // TODO: Wykonaj pracę wątku
        // Przykład: wypisz coś co sekundę
        printf("Worker thread working...\n");
        sleep(2);
    }

    printf("Worker thread finishing\n");
    return NULL;
}

/* ============================================
   INICJALIZACJA I KONFIGURACJA
   ============================================ */

void initialize_signal_handling(signal_handler_data_t* sh_data,
                                int* element_count,
                                volatile sig_atomic_t* stop_flag) {
    // Zablokuj sygnały w głównym wątku
    sigset_t signal_set, old_signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGQUIT);

    if (pthread_sigmask(SIG_BLOCK, &signal_set, &old_signal_set) != 0) {
        perror("pthread_sigmask");
        exit(1);
    }

    // Skonfiguruj dane wątku obsługi sygnałów
    sh_data->element_count = element_count;
    sh_data->list_head = &head;
    sh_data->list_mutex_ptr = &list_mutex;
    sh_data->stop_mutex_ptr = &stop_mutex;
    sh_data->stop_flag_ptr = stop_flag;

    // Utwórz wątek obsługi sygnałów
    if (pthread_create(&sh_data->tid, NULL, signal_handler_thread, sh_data) != 0) {
        perror("Blad przy tworzeniu watku obslugi sygnalow");
        exit(1);
    }
}

void initialize_worker_thread(worker_thread_data_t* worker_data) {
    // TODO: Skonfiguruj dane dla wątku roboczego
    worker_data->some_parameter = 42; // Przykładowa wartość

    // Utwórz wątek roboczy
    if (pthread_create(&worker_data->tid, NULL, worker_thread, worker_data) != 0) {
        perror("Blad przy tworzeniu watku roboczego");
        exit(1);
    }
}

/* ============================================
   GŁÓWNA PĘTLA PROGRAMU
   ============================================ */

void main_loop(int element_count, volatile sig_atomic_t* stop_flag) {
    printf("Rozpoczynam glowna petle programu\n");

    while (1) {
        // Sprawdź flagę stop
        pthread_mutex_lock(&stop_mutex);
        int should_stop = *stop_flag;
        pthread_mutex_unlock(&stop_mutex);

        if (should_stop) {
            break;
        }

        // TODO: Implementuj główną logikę programu
        // Przykład: wyświetl listę co sekundę
        pthread_mutex_lock(&list_mutex);
        printf("Glowny watek - Lista (%d elementow): ", element_count);
        print_list();
        pthread_mutex_unlock(&list_mutex);

        sleep(1);
    }
}

/* ============================================
   FUNKCJA GŁÓWNA
   ============================================ */

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Uzycie: %s <k>\n", argv[0]);
        fprintf(stderr, "k - liczba elementow do inicjalizacji listy\n");
        exit(1);
    }

    int k = atoi(argv[1]);
    if (k <= 0) {
        fprintf(stderr, "Parametr k musi byc liczba dodatnia\n");
        exit(1);
    }

    printf("Program startuje z k = %d\n", k);

    // Inicjalizacja listy
    initialize_list(k);
    int element_count = k;

    // Struktury danych dla wątków
    signal_handler_data_t signal_handler_data;
    worker_thread_data_t worker_data;

    // Inicjalizacja obsługi sygnałów
    initialize_signal_handling(&signal_handler_data, &element_count, &stop_flag);

    // Inicjalizacja wątku roboczego
    // TODO: Odkomentuj gdy zaimplementujesz worker_thread
    // initialize_worker_thread(&worker_data);

    // Uruchom główną pętlę
    main_loop(element_count, &stop_flag);

    // Czekaj na zakończenie wątków
    printf("Oczekiwanie na zakonczenie watkow...\n");
    pthread_join(signal_handler_data.tid, NULL);

    // TODO: Odkomentuj gdy zaimplementujesz worker_thread
    // pthread_join(worker_data.tid, NULL);

    // Sprzątanie
    printf("Czyszczenie zasobow...\n");
    cleanup_list();

    pthread_mutex_destroy(&list_mutex);
    pthread_mutex_destroy(&stop_mutex);

    printf("Program zakonczony poprawnie.\n");
    return 0;
}