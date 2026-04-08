#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LAB_NAMED_SHM_DATA "/lab_named_data_l6"
#define LAB_NAMED_SHM_CTRL "/lab_named_ctrl_l6"

typedef struct {
    pthread_mutex_t robust_mutex;
    int children_left_mutex;
    sem_t semaphore;
    int children_left_semaphore;
} ctrl_block_t;

static void fail(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void init_robust_mutex_pshared(pthread_mutex_t *m) {
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) fail("pthread_mutexattr_init");
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) fail("pthread_mutexattr_setpshared");
    if (pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) != 0) fail("pthread_mutexattr_setrobust");
    if (pthread_mutex_init(m, &attr) != 0) fail("pthread_mutex_init");
    if (pthread_mutexattr_destroy(&attr) != 0) fail("pthread_mutexattr_destroy");
}

static void robust_lock_or_recover(pthread_mutex_t *m) {
    int rc = pthread_mutex_lock(m);
    if (rc == EOWNERDEAD) {
        fprintf(stderr, "[RECOVERY] Wlasciciel mutexa umarl. Przywracam spojny stan.\n");
        if (pthread_mutex_consistent(m) != 0) fail("pthread_mutex_consistent");
    } else if (rc != 0) {
        errno = rc;
        fail("pthread_mutex_lock");
    }
}

static void child_task(
    int child_idx,
    int child_count,
    const char *txt_path,
    off_t txt_size,
    char *unnamed_shared,
    char *named_shared,
    ctrl_block_t *ctrl
) {
    srand((unsigned int)(time(NULL) ^ (getpid() << 8)));

    int fd = open(txt_path, O_RDONLY);
    if (fd < 0) fail("open txt");

    off_t start = (txt_size * child_idx) / child_count;
    off_t end = (txt_size * (child_idx + 1)) / child_count;
    size_t len = (size_t)(end - start);

    char *tmp = NULL;
    if (len > 0) {
        tmp = malloc(len);
        if (!tmp) fail("malloc");
        ssize_t rd = pread(fd, tmp, len, start);
        if (rd < 0) fail("pread");
        len = (size_t)rd;
    }
    close(fd);

    /*
     * TODO (LAB):
     * Tu wstaw wlasne przetwarzanie fragmentu pliku.
     * Aktualnie: liczymy cyfry i kodujemy wynik do jednego znaku.
     */
    int digits = 0;
    for (size_t i = 0; i < len; i++) {
        if (tmp[i] >= '0' && tmp[i] <= '9') digits++;
    }
    free(tmp);

    char result = (char)('a' + (digits % 26));
    unnamed_shared[child_idx] = result;
    named_shared[child_idx] = (char)(result - 32); /* uppercase */

    /*
     * Losowa awaria dziecka podczas trzymania robust mutex.
     * Inne procesy musza obsluzyc EOWNERDEAD.
     */
    robust_lock_or_recover(&ctrl->robust_mutex);
    if ((rand() % 6) == 0) {
        fprintf(stderr, "[CHILD %d] Losowe przerwanie podczas sekcji krytycznej.\n", child_idx);
        _exit(120 + child_idx);
    }
    ctrl->children_left_mutex--;
    if (pthread_mutex_unlock(&ctrl->robust_mutex) != 0) fail("pthread_mutex_unlock");

    if (sem_wait(&ctrl->semaphore) != 0) fail("sem_wait");
    ctrl->children_left_semaphore--;
    if (sem_post(&ctrl->semaphore) != 0) fail("sem_post");

    _exit(0);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Uzycie: %s <liczba_dzieci> <plik.txt>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int child_count = atoi(argv[1]);
    if (child_count <= 0) {
        fprintf(stderr, "liczba_dzieci musi byc > 0\n");
        return EXIT_FAILURE;
    }

    const char *txt_path = argv[2];
    struct stat st;
    if (stat(txt_path, &st) != 0) fail("stat");
    off_t txt_size = st.st_size;

    size_t data_size = sizeof(char) * (size_t)child_count;

    /* 1) Pamiec dzielona nienazwana: mmap + MAP_ANONYMOUS. */
    char *unnamed_shared = mmap(NULL, data_size, PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (unnamed_shared == MAP_FAILED) fail("mmap unnamed");
    memset(unnamed_shared, '.', data_size);

    /* 2) Pamiec dzielona nazwana dla danych dzieci: shm_open + mmap. */
    int data_fd = shm_open(LAB_NAMED_SHM_DATA, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (data_fd < 0 && errno == EEXIST) {
        if (shm_unlink(LAB_NAMED_SHM_DATA) != 0) fail("shm_unlink old data");
        data_fd = shm_open(LAB_NAMED_SHM_DATA, O_CREAT | O_EXCL | O_RDWR, 0600);
    }
    if (data_fd < 0) fail("shm_open data");
    if (ftruncate(data_fd, (off_t)data_size) != 0) fail("ftruncate data");
    char *named_shared = mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, data_fd, 0);
    if (named_shared == MAP_FAILED) fail("mmap named");
    memset(named_shared, '.', data_size);

    /* 3) Osobna pamiec dzielona na licznik dzieci + mutex + semafor. */
    int ctrl_fd = shm_open(LAB_NAMED_SHM_CTRL, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (ctrl_fd < 0 && errno == EEXIST) {
        if (shm_unlink(LAB_NAMED_SHM_CTRL) != 0) fail("shm_unlink old ctrl");
        ctrl_fd = shm_open(LAB_NAMED_SHM_CTRL, O_CREAT | O_EXCL | O_RDWR, 0600);
    }
    if (ctrl_fd < 0) fail("shm_open ctrl");
    if (ftruncate(ctrl_fd, (off_t)sizeof(ctrl_block_t)) != 0) fail("ftruncate ctrl");
    ctrl_block_t *ctrl = mmap(NULL, sizeof(ctrl_block_t), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0);
    if (ctrl == MAP_FAILED) fail("mmap ctrl");
    memset(ctrl, 0, sizeof(*ctrl));

    init_robust_mutex_pshared(&ctrl->robust_mutex);
    ctrl->children_left_mutex = child_count;
    if (sem_init(&ctrl->semaphore, 1, 1) != 0) fail("sem_init");
    ctrl->children_left_semaphore = child_count;

    for (int i = 0; i < child_count; i++) {
        pid_t pid = fork();
        if (pid < 0) fail("fork");
        if (pid == 0) {
            child_task(i, child_count, txt_path, txt_size, unnamed_shared, named_shared, ctrl);
        }
    }

    for (int i = 0; i < child_count; i++) {
        int status = 0;
        pid_t p = wait(&status);
        if (p < 0) fail("wait");
        if (WIFEXITED(status)) {
            printf("[PARENT] pid=%d exit=%d\n", (int)p, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[PARENT] pid=%d signal=%d\n", (int)p, WTERMSIG(status));
        }
    }

    printf("\nWynik dzieci (nienazwana): ");
    for (int i = 0; i < child_count; i++) printf("%c ", unnamed_shared[i]);
    printf("\nWynik dzieci (nazwana):   ");
    for (int i = 0; i < child_count; i++) printf("%c ", named_shared[i]);
    printf("\n");

    robust_lock_or_recover(&ctrl->robust_mutex);
    int by_mutex = ctrl->children_left_mutex;
    if (pthread_mutex_unlock(&ctrl->robust_mutex) != 0) fail("pthread_mutex_unlock final");

    if (sem_wait(&ctrl->semaphore) != 0) fail("sem_wait final");
    int by_semaphore = ctrl->children_left_semaphore;
    if (sem_post(&ctrl->semaphore) != 0) fail("sem_post final");

    printf("children_left (mutex robust): %d\n", by_mutex);
    printf("children_left (semafor):      %d\n", by_semaphore);

    if (sem_destroy(&ctrl->semaphore) != 0) fail("sem_destroy");
    if (pthread_mutex_destroy(&ctrl->robust_mutex) != 0) fail("pthread_mutex_destroy");
    if (munmap(unnamed_shared, data_size) != 0) fail("munmap unnamed");
    if (munmap(named_shared, data_size) != 0) fail("munmap named");
    if (munmap(ctrl, sizeof(*ctrl)) != 0) fail("munmap ctrl");
    close(data_fd);
    close(ctrl_fd);
    if (shm_unlink(LAB_NAMED_SHM_DATA) != 0) fail("shm_unlink data final");
    if (shm_unlink(LAB_NAMED_SHM_CTRL) != 0) fail("shm_unlink ctrl final");

    return 0;
}
