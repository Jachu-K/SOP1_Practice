#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NAMED_DATA_SHM "/demo_named_data_shm_l6"
#define CTRL_SHM "/demo_ctrl_shm_l6"
#define NAMED_SEM_L6 "/demo_named_sem_l6"

typedef struct {
    pthread_mutex_t robust_count_mutex;
    int alive_count_mutex;
    sem_t count_sem;
    int alive_count_sem;
    int alive_count_named_sem;
} shared_ctrl_t;

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void setup_robust_process_shared_mutex(pthread_mutex_t *m) {
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        die("pthread_mutexattr_init");
    }
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        die("pthread_mutexattr_setpshared");
    }
    if (pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) != 0) {
        die("pthread_mutexattr_setrobust");
    }
    if (pthread_mutex_init(m, &attr) != 0) {
        die("pthread_mutex_init");
    }
    if (pthread_mutexattr_destroy(&attr) != 0) {
        die("pthread_mutexattr_destroy");
    }
}

static void lock_robust_or_recover(pthread_mutex_t *m) {
    int rc = pthread_mutex_lock(m);
    if (rc == EOWNERDEAD) {
        fprintf(stderr, "[INFO] Robust mutex: poprzedni wlasciciel umarl, odzyskuje stan.\n");
        if (pthread_mutex_consistent(m) != 0) {
            die("pthread_mutex_consistent");
        }
    } else if (rc != 0) {
        errno = rc;
        die("pthread_mutex_lock");
    }
}

static void child_job(int idx, int children, const char *txt_path, char *unnamed_map,
                      char *named_map, shared_ctrl_t *ctrl, sem_t *named_sem, off_t file_size) {
    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));

    int fd = open(txt_path, O_RDONLY);
    if (fd < 0) {
        die("child open txt");
    }

    off_t start = (file_size * idx) / children;
    off_t end = (file_size * (idx + 1)) / children;
    size_t part_len = (size_t)(end - start);

    char *buf = NULL;
    if (part_len > 0) {
        buf = (char *)malloc(part_len);
        if (!buf) {
            die("malloc");
        }
        ssize_t rd = pread(fd, buf, part_len, start);
        if (rd < 0) {
            die("pread");
        }
        if ((size_t)rd < part_len) {
            part_len = (size_t)rd;
        }
    }
    close(fd);

    char result_char = 'A' + (idx % 26);
    if (part_len > 0) {
        int letters = 0;
        for (size_t i = 0; i < part_len; i++) {
            if ((buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= 'A' && buf[i] <= 'Z')) {
                letters++;
            }
        }
        result_char = (char)('a' + (letters % 26));
    }
    free(buf);

    unnamed_map[idx] = result_char;
    named_map[idx] = (char)(result_char - 32 >= 'A' ? result_char - 32 : result_char);

    bool crash_now = (rand() % 5 == 0);
    lock_robust_or_recover(&ctrl->robust_count_mutex);
    if (crash_now) {
        fprintf(stderr, "[CHILD %d] Losowa awaria podczas trzymania robust mutex.\n", idx);
        _exit(100 + idx);
    }
    ctrl->alive_count_mutex--;
    if (pthread_mutex_unlock(&ctrl->robust_count_mutex) != 0) {
        die("pthread_mutex_unlock");
    }

    if (sem_wait(&ctrl->count_sem) != 0) {
        die("sem_wait");
    }
    ctrl->alive_count_sem--;
    if (sem_post(&ctrl->count_sem) != 0) {
        die("sem_post");
    }

    if (sem_wait(named_sem) != 0) {
        die("sem_wait named");
    }
    ctrl->alive_count_named_sem--;
    if (sem_post(named_sem) != 0) {
        die("sem_post named");
    }
    if (sem_close(named_sem) != 0) {
        die("sem_close child named");
    }

    _exit(0);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Uzycie: %s <liczba_dzieci> <sciezka_do_pliku.txt>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int children = atoi(argv[1]);
    if (children <= 0) {
        fprintf(stderr, "liczba_dzieci musi byc > 0\n");
        return EXIT_FAILURE;
    }

    const char *txt_path = argv[2];
    struct stat st;
    if (stat(txt_path, &st) != 0) {
        die("stat txt");
    }
    off_t file_size = st.st_size;

    size_t per_child_mem_size = sizeof(char) * (size_t)children;

    char *unnamed_map = mmap(NULL, per_child_mem_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (unnamed_map == MAP_FAILED) {
        die("mmap unnamed");
    }
    memset(unnamed_map, '?', per_child_mem_size);

    int named_fd = shm_open(NAMED_DATA_SHM, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (named_fd < 0) {
        if (errno == EEXIST) {
            if (shm_unlink(NAMED_DATA_SHM) != 0) {
                die("shm_unlink stale named data");
            }
            named_fd = shm_open(NAMED_DATA_SHM, O_CREAT | O_EXCL | O_RDWR, 0600);
        }
    }
    if (named_fd < 0) {
        die("shm_open named");
    }
    if (ftruncate(named_fd, (off_t)per_child_mem_size) != 0) {
        die("ftruncate named");
    }
    char *named_map = mmap(NULL, per_child_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, named_fd, 0);
    if (named_map == MAP_FAILED) {
        die("mmap named");
    }
    memset(named_map, '?', per_child_mem_size);

    int ctrl_fd = shm_open(CTRL_SHM, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (ctrl_fd < 0) {
        if (errno == EEXIST) {
            if (shm_unlink(CTRL_SHM) != 0) {
                die("shm_unlink stale ctrl");
            }
            ctrl_fd = shm_open(CTRL_SHM, O_CREAT | O_EXCL | O_RDWR, 0600);
        }
    }
    if (ctrl_fd < 0) {
        die("shm_open ctrl");
    }
    if (ftruncate(ctrl_fd, (off_t)sizeof(shared_ctrl_t)) != 0) {
        die("ftruncate ctrl");
    }
    shared_ctrl_t *ctrl = mmap(NULL, sizeof(shared_ctrl_t), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0);
    if (ctrl == MAP_FAILED) {
        die("mmap ctrl");
    }
    memset(ctrl, 0, sizeof(*ctrl));

    setup_robust_process_shared_mutex(&ctrl->robust_count_mutex);
    ctrl->alive_count_mutex = children;
    if (sem_init(&ctrl->count_sem, 1, 1) != 0) {
        die("sem_init");
    }
    ctrl->alive_count_sem = children;
    ctrl->alive_count_named_sem = children;

    if (sem_unlink(NAMED_SEM_L6) != 0 && errno != ENOENT) {
        die("sem_unlink stale named");
    }
    sem_t *named_sem = sem_open(NAMED_SEM_L6, O_CREAT | O_EXCL, 0600, 1);
    if (named_sem == SEM_FAILED) {
        die("sem_open named");
    }

    for (int i = 0; i < children; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            die("fork");
        }
        if (pid == 0) {
            child_job(i, children, txt_path, unnamed_map, named_map, ctrl, named_sem, file_size);
        }
    }

    int exited = 0;
    while (exited < children) {
        int status = 0;
        pid_t w = wait(&status);
        if (w < 0) {
            die("wait");
        }
        exited++;
        if (WIFEXITED(status)) {
            printf("[PARENT] child pid=%d exit=%d\n", (int)w, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[PARENT] child pid=%d signal=%d\n", (int)w, WTERMSIG(status));
        }
    }

    printf("\n=== WYNIKI DZIECI (1 bajt na dziecko) ===\n");
    printf("unnamed mmap: ");
    for (int i = 0; i < children; i++) {
        printf("%c ", unnamed_map[i]);
    }
    printf("\n");

    printf("named shm+mmap: ");
    for (int i = 0; i < children; i++) {
        printf("%c ", named_map[i]);
    }
    printf("\n");

    lock_robust_or_recover(&ctrl->robust_count_mutex);
    int final_mutex_count = ctrl->alive_count_mutex;
    if (pthread_mutex_unlock(&ctrl->robust_count_mutex) != 0) {
        die("pthread_mutex_unlock final");
    }

    if (sem_wait(&ctrl->count_sem) != 0) {
        die("sem_wait final");
    }
    int final_sem_count = ctrl->alive_count_sem;
    if (sem_post(&ctrl->count_sem) != 0) {
        die("sem_post final");
    }

    if (sem_wait(named_sem) != 0) {
        die("sem_wait named final");
    }
    int final_named_sem_count = ctrl->alive_count_named_sem;
    if (sem_post(named_sem) != 0) {
        die("sem_post named final");
    }

    printf("alive_count (chronione robust mutex): %d\n", final_mutex_count);
    printf("alive_count (chronione semaforem):    %d\n", final_sem_count);
    printf("alive_count (chronione nazw. sem.):   %d\n", final_named_sem_count);

    if (sem_destroy(&ctrl->count_sem) != 0) {
        die("sem_destroy");
    }
    if (pthread_mutex_destroy(&ctrl->robust_count_mutex) != 0) {
        die("pthread_mutex_destroy");
    }
    if (munmap(unnamed_map, per_child_mem_size) != 0) {
        die("munmap unnamed");
    }
    if (munmap(named_map, per_child_mem_size) != 0) {
        die("munmap named");
    }
    if (munmap(ctrl, sizeof(shared_ctrl_t)) != 0) {
        die("munmap ctrl");
    }
    close(named_fd);
    close(ctrl_fd);
    if (sem_close(named_sem) != 0) {
        die("sem_close parent named");
    }
    if (sem_unlink(NAMED_SEM_L6) != 0) {
        die("sem_unlink named sem final");
    }
    if (shm_unlink(NAMED_DATA_SHM) != 0) {
        die("shm_unlink named final");
    }
    if (shm_unlink(CTRL_SHM) != 0) {
        die("shm_unlink ctrl final");
    }

    return 0;
}
