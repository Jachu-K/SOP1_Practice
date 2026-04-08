#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Uruchomienie: ./a.out <liczba_dzieci>
 * Przyklad: ./a.out 4
 */

#define die(msg)                                                                                   \
    do {                                                                                           \
        perror(msg);                                                                               \
        exit(EXIT_FAILURE);                                                                        \
    } while (0)

static void die_pthread(int err, const char *what) {
    errno = err;
    die(what);
}

typedef struct {
    pthread_barrier_t barrier;
} anon_barrier_region_t;

typedef struct {
    pthread_mutex_t robust_mtx;
    int shared_counter;
} named_robust_region_t;

static void build_name(char *buf, size_t buflen, const char *prefix) {
    int n = snprintf(buf, buflen, "%s_%ld", prefix, (long)getpid());
    if (n < 0 || (size_t)n >= buflen) {
        fprintf(stderr, "snprintf nazwy zasobu\n");
        exit(EXIT_FAILURE);
    }
}

static void init_robust_mutex_pshared(pthread_mutex_t *m) {
    pthread_mutexattr_t attr;
    int rc = pthread_mutexattr_init(&attr);
    if (rc != 0) {
        die_pthread(rc, "pthread_mutexattr_init");
    }
    rc = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (rc != 0) {
        die_pthread(rc, "pthread_mutexattr_setpshared");
    }
    rc = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    if (rc != 0) {
        die_pthread(rc, "pthread_mutexattr_setrobust");
    }
    rc = pthread_mutex_init(m, &attr);
    if (rc != 0) {
        die_pthread(rc, "pthread_mutex_init");
    }
    rc = pthread_mutexattr_destroy(&attr);
    if (rc != 0) {
        die_pthread(rc, "pthread_mutexattr_destroy");
    }
}

static void lock_robust_or_recover(pthread_mutex_t *m) {
    int rc = pthread_mutex_lock(m);
    if (rc == EOWNERDEAD) {
        fprintf(stderr, "[pid %ld] EOWNERDEAD -> pthread_mutex_consistent\n", (long)getpid());
        rc = pthread_mutex_consistent(m);
        if (rc != 0) {
            die_pthread(rc, "pthread_mutex_consistent");
        }
    } else if (rc != 0) {
        die_pthread(rc, "pthread_mutex_lock");
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uzycie: %s <liczba_dzieci>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int n = atoi(argv[1]);
    if (n <= 0) {
        fprintf(stderr, "liczba_dzieci musi byc > 0\n");
        return EXIT_FAILURE;
    }

    char sem_name[64];
    char shm_name[64];
    build_name(sem_name, sizeof(sem_name), "/demo_ipc_sem");
    build_name(shm_name, sizeof(shm_name), "/demo_ipc_shm");

    /* --- Nazwany semafor: utworzenie przez rodzica (binarny, wartosc poczatkowa 1) --- */
    if (sem_unlink(sem_name) != 0 && errno != ENOENT) {
        die("sem_unlink (stary obiekt)");
    }
    sem_t *sem_parent = sem_open(sem_name, O_CREAT | O_EXCL, 0600, 1);
    if (sem_parent == SEM_FAILED) {
        die("sem_open (tworzenie)");
    }

    /* --- Anonimowa pamiec dzielona: bariera procesowa (tylko dzieci ja wywoluja) --- */
    size_t anon_size = sizeof(anon_barrier_region_t);
    anon_barrier_region_t *bar_region =
        mmap(NULL, anon_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (bar_region == MAP_FAILED) {
        if (sem_close(sem_parent) != 0) {
            perror("sem_close po mmap fail");
        }
        if (sem_unlink(sem_name) != 0 && errno != ENOENT) {
            perror("sem_unlink po mmap fail");
        }
        die("mmap anon (bariera)");
    }

    pthread_barrierattr_t battr;
    int rc = pthread_barrierattr_init(&battr);
    if (rc != 0) {
        die_pthread(rc, "pthread_barrierattr_init");
    }
    rc = pthread_barrierattr_setpshared(&battr, PTHREAD_PROCESS_SHARED);
    if (rc != 0) {
        die_pthread(rc, "pthread_barrierattr_setpshared");
    }
    rc = pthread_barrier_init(&bar_region->barrier, &battr, (unsigned int)n);
    if (rc != 0) {
        die_pthread(rc, "pthread_barrier_init");
    }
    rc = pthread_barrierattr_destroy(&battr);
    if (rc != 0) {
        die_pthread(rc, "pthread_barrierattr_destroy");
    }

    /* --- Nazwana pamiec dzielona: robust mutex + wspolny licznik --- */
    int shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shm_fd < 0) {
        die("shm_open");
    }
    if (ftruncate(shm_fd, (off_t)sizeof(named_robust_region_t)) != 0) {
        close(shm_fd);
        die("ftruncate");
    }
    named_robust_region_t *robust_region =
        mmap(NULL, sizeof(named_robust_region_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (robust_region == MAP_FAILED) {
        close(shm_fd);
        die("mmap named");
    }
    memset(robust_region, 0, sizeof(*robust_region));
    init_robust_mutex_pshared(&robust_region->robust_mtx);
    robust_region->shared_counter = 0;

    if (close(shm_fd) != 0) {
        perror("close shm_fd");
    }

    /* --- Dzieci: sem_open (podlaczenie), bariera, sekcja krytyczna pod robust mutexem --- */
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            die("fork");
        }
        if (pid == 0) {
            /* Dziecko: otwiera nazwany semafor (bez O_CREAT) */
            sem_t *sem_child = sem_open(sem_name, 0);
            if (sem_child == SEM_FAILED) {
                die("sem_open (dziecko)");
            }

            int brc = pthread_barrier_wait(&bar_region->barrier);
            if (brc != 0 && brc != PTHREAD_BARRIER_SERIAL_THREAD) {
                fprintf(stderr, "pthread_barrier_wait: %d\n", brc);
                _exit(EXIT_FAILURE);
            }

            if (sem_wait(sem_child) != 0) {
                perror("sem_wait");
                _exit(EXIT_FAILURE);
            }

            lock_robust_or_recover(&robust_region->robust_mtx);
            robust_region->shared_counter++;
            if (pthread_mutex_unlock(&robust_region->robust_mtx) != 0) {
                perror("pthread_mutex_unlock");
                sem_post(sem_child);
                sem_close(sem_child);
                _exit(EXIT_FAILURE);
            }

            if (sem_post(sem_child) != 0) {
                perror("sem_post");
                sem_close(sem_child);
                _exit(EXIT_FAILURE);
            }

            if (sem_close(sem_child) != 0) {
                perror("sem_close dziecka");
                _exit(EXIT_FAILURE);
            }
            _exit(EXIT_SUCCESS);
        }
    }

    /* Rodzic czeka na dzieci */
    for (int i = 0; i < n; i++) {
        int st = 0;
        if (wait(&st) < 0) {
            die("wait");
        }
    }

    printf("shared_counter (pod robust mutex w named shm): %d (oczekiwane %d)\n",
           robust_region->shared_counter, n);

    /* Zwalnianie: najpierw niszczenie obiektow pthread w pamieci dzielonej, potem munmap/unlink */
    rc = pthread_barrier_destroy(&bar_region->barrier);
    if (rc != 0) {
        die_pthread(rc, "pthread_barrier_destroy");
    }

    rc = pthread_mutex_destroy(&robust_region->robust_mtx);
    if (rc != 0) {
        die_pthread(rc, "pthread_mutex_destroy");
    }

    if (munmap(bar_region, anon_size) != 0) {
        die("munmap anon");
    }
    if (munmap(robust_region, sizeof(*robust_region)) != 0) {
        die("munmap named");
    }

    if (sem_close(sem_parent) != 0) {
        die("sem_close rodzica");
    }
    if (sem_unlink(sem_name) != 0) {
        die("sem_unlink");
    }

    if (shm_unlink(shm_name) != 0) {
        die("shm_unlink");
    }

    return 0;
}
