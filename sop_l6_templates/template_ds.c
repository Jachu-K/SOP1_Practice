#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#define die(msg) do { perror(msg); exit(EXIT_FAILURE); } while(0)

static void die_pthread(int err, const char *what) {
    errno = err;
    die(what);
}

/* Struktura przechowująca liczniki i mechanizmy synchronizacji */
struct shared_sync {
    int counter_mutex;          // licznik chroniony mutexem
    pthread_mutex_t mutex;      // robust mutex
    int counter_sem;            // licznik chroniony semaforem
    sem_t sem;                  // semafor binarny (działa jak mutex)
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Użycie: %s <liczba_dzieci> <ścieżka_do_pliku.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int num_children = atoi(argv[1]);
    const char *file_path = argv[2];
    if (num_children <= 0) die("Nieprawidłowa liczba dzieci");

    /* ------------------------------------------------------------
     * 1. Otwarcie pliku i mapowanie go przez mmap (współdzielone)
     * ------------------------------------------------------------ */
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) die("open pliku");
    struct stat st;
    if (fstat(fd, &st) == -1) die("fstat");
    off_t file_size = st.st_size;
    char *mapped_file = NULL;
    if (file_size > 0) {
        mapped_file = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
        if (mapped_file == MAP_FAILED) die("mmap pliku");
    }
    close(fd);  // deskryptor nie jest już potrzebny

    /* ------------------------------------------------------------
     * 2. Pamięć dzielona NIENAZWANA (anonimowa) – tablica znaków
     *    Rozmiar: sizeof(char) * liczba_dzieci
     * ------------------------------------------------------------ */
    char *anon_shared = mmap(NULL, num_children * sizeof(char),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (anon_shared == MAP_FAILED) die("mmap anonimowy");

    /* ------------------------------------------------------------
     * 3. Pamięć dzielona NAZWANA (shm_open) – tablica znaków
     * ------------------------------------------------------------ */
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/my_named_shm_%ld", (long)getpid());
    int shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shm_fd == -1) die("shm_open");
    if (ftruncate(shm_fd, num_children * sizeof(char)) == -1)
        die("ftruncate");
    char *named_shared = mmap(NULL, num_children * sizeof(char),
                              PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (named_shared == MAP_FAILED) die("mmap nazwany");
    close(shm_fd);

    /* ------------------------------------------------------------
     * 4. Pamięć dzielona dla synchronizacji (robust mutex + semafor)
     * ------------------------------------------------------------ */
    struct shared_sync *sync = mmap(NULL, sizeof(struct shared_sync),
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sync == MAP_FAILED) die("mmap sync");

    // Inicjalizacja robust mutexa
    pthread_mutexattr_t attr;
    int pret = pthread_mutexattr_init(&attr);
    if (pret != 0) die_pthread(pret, "pthread_mutexattr_init");
    pret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (pret != 0) die_pthread(pret, "pthread_mutexattr_setpshared");
    pret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    if (pret != 0) die_pthread(pret, "pthread_mutexattr_setrobust");
    pret = pthread_mutex_init(&sync->mutex, &attr);
    if (pret != 0) die_pthread(pret, "pthread_mutex_init");
    pret = pthread_mutexattr_destroy(&attr);
    if (pret != 0) die_pthread(pret, "pthread_mutexattr_destroy");
    sync->counter_mutex = 0;

    // Inicjalizacja semafora binarnego (pshared = 1)
    if (sem_init(&sync->sem, 1, 1) == -1) die("sem_init");
    sync->counter_sem = 0;

    /* ------------------------------------------------------------
     * 5. Tworzenie procesów dzieci
     * ------------------------------------------------------------ */
    pid_t pids[num_children];
    srand(time(NULL));  // ziarno dla rodzica (nie jest wykorzystywane przez dzieci)

    for (int i = 0; i < num_children; i++) {
        pid_t pid = fork();
        if (pid == -1) die("fork");

        if (pid == 0) {  // ** PROCES DZIECKA **
            int index = i;
            // własne ziarno dla generatora liczb losowych
            srand(getpid() ^ time(NULL));

            // ---- a) Praca na własnej części pamięci dzielonej ----
            anon_shared[index] = 'a' + (index % 26);
            named_shared[index] = 'A' + (index % 26);

            // ---- b) Przetwarzanie fragmentu pliku .txt ----
            off_t block_size = (file_size + num_children - 1) / num_children;
            off_t start = index * block_size;
            off_t end = (index + 1) * block_size;
            if (end > file_size) end = file_size;
            int letter_count = 0;
            for (off_t j = start; j < end; j++) {
                if (file_size > 0 && mapped_file[j] == 'e') letter_count++;  // zliczamy literę 'e'
            }
            printf("Dziecko %d: znaleziono %d liter 'e' w segmencie [%ld, %ld)\n",
                   index, letter_count, start, end);

            // ---- c) Synchronizacja przez ROBUST MUTEX (z możliwością losowej śmierci) ----
            int ret = pthread_mutex_lock(&sync->mutex);
            if (ret == EOWNERDEAD) {
                printf("Dziecko %d: poprzedni właściciel mutexa zmarł – odzyskiwanie\n", index);
                int crec = pthread_mutex_consistent(&sync->mutex);
                if (crec != 0) {
                    fprintf(stderr, "Dziecko %d: błąd pthread_mutex_consistent: %d\n", index, crec);
                    exit(EXIT_FAILURE);
                }
                // kontynuujemy – licznik może być już zwiększony, ale dla uproszczenia zwiększamy go ponownie
            } else if (ret != 0) {
                fprintf(stderr, "Dziecko %d: błąd pthread_mutex_lock: %d\n", index, ret);
                exit(EXIT_FAILURE);
            }
            // Sekcja krytyczna
            sync->counter_mutex++;
            printf("Dziecko %d: zwiększyło counter_mutex do %d\n", index, sync->counter_mutex);

            // Losowe zakończenie (20% szans) – test robust mutexa
            if (rand() % 5 == 0) {
                printf("Dziecko %d: UMIERA TRZYMAJĄC MUTEKS! (test robust mutex)\n", index);
                exit(0);  // mutex pozostanie w stanie EOWNERDEAD
            }
            ret = pthread_mutex_unlock(&sync->mutex);
            if (ret != 0) {
                fprintf(stderr, "Dziecko %d: błąd pthread_mutex_unlock: %d\n", index, ret);
                exit(EXIT_FAILURE);
            }

            // ---- d) Synchronizacja przez SEMAFOR (bez losowego kończenia) ----
            if (sem_wait(&sync->sem) == -1) die("sem_wait");
            sync->counter_sem++;
            printf("Dziecko %d: zwiększyło counter_sem do %d\n", index, sync->counter_sem);
            if (sem_post(&sync->sem) == -1) die("sem_post");

            usleep(10000);  // małe opóźnienie dla czytelności
            exit(0);
        } else {
            pids[i] = pid;   // rodzic zapamiętuje PID dziecka
        }
    }

    /* ------------------------------------------------------------
     * 6. Rodzic czeka na zakończenie wszystkich dzieci
     * ------------------------------------------------------------ */
    for (int i = 0; i < num_children; i++) {
        waitpid(pids[i], NULL, 0);
    }

    /* ------------------------------------------------------------
     * 7. Prezentacja wyników i czyszczenie zasobów
     * ------------------------------------------------------------ */
    printf("\n=== ZAWARTOŚĆ PAMIĘCI DZIELONYCH ===\n");
    printf("anon_shared (nienazwana):\n");
    for (int i = 0; i < num_children; i++)
        printf("  [%d] = '%c'\n", i, anon_shared[i]);
    printf("named_shared (nazwana):\n");
    for (int i = 0; i < num_children; i++)
        printf("  [%d] = '%c'\n", i, named_shared[i]);
    printf("\nLicznik chroniony robust mutexem: counter_mutex = %d\n", sync->counter_mutex);
    printf("Licznik chroniony semaforem:    counter_sem = %d\n", sync->counter_sem);
    printf("(Uwaga: część dzieci mogła umrzeć przed inkrementacją, dlatego wartości mogą być niższe niż %d)\n", num_children);

    // Zwalnianie pamięci i usuwanie obiektów
    munmap(anon_shared, num_children * sizeof(char));
    munmap(named_shared, num_children * sizeof(char));
    shm_unlink(shm_name);
    if (file_size > 0) {
        munmap(mapped_file, file_size);
    }
    int dret = pthread_mutex_destroy(&sync->mutex);
    if (dret != 0) die_pthread(dret, "pthread_mutex_destroy");
    sem_destroy(&sync->sem);
    munmap(sync, sizeof(struct shared_sync));

    return 0;
}