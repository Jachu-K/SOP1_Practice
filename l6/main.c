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
#include <math.h>
#include <stdint.h>


#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define SHARED_NAME "/lab_shared_calcs2"
#define LAB_NAMED_SEM "/lab_named_seml6_2"
#define LAB_NAMED_SHM_CTRL "/lab_shm_controll62"

typedef struct {
    pthread_mutex_t robust_mutex;
    int processes_count;
} ctrl_block_t;

static void init_robust_mutex_pshared(pthread_mutex_t *m) {
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) ERR("pthread_mutexattr_init");
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) ERR("pthread_mutexattr_setpshared");
    if (pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) != 0) ERR("pthread_mutexattr_setrobust");
    if (pthread_mutex_init(m, &attr) != 0) ERR("pthread_mutex_init");
    if (pthread_mutexattr_destroy(&attr) != 0) ERR("pthread_mutexattr_destroy");
}
static void robust_lock_or_recover(pthread_mutex_t *m) {
    int rc = pthread_mutex_lock(m);
    if (rc == EOWNERDEAD) {
        fprintf(stderr, "[RECOVERY] Wlasciciel mutexa umarl. Przywracam spojny stan.\n");
        if (pthread_mutex_consistent(m) != 0) ERR("pthread_mutex_consistent");
    } else if (rc != 0) {
        errno = rc;
        ERR("pthread_mutex_lock");
    }
}

// Values of this function are in range (0,1]
double func(double x)
{
    usleep(2000);
    return exp(-x * x);
}

/**
 * It counts hit points by Monte Carlo method.
 * Use it to process one batch of computation.
 * @param N Number of points to randomize
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return Number of points which was hit.
 */
int randomize_points(int N, float a, float b)
{
    int result = 0;
    for (int i = 0; i < N; ++i)
    {
        double rand_x = ((double)rand() / RAND_MAX) * (b - a) + a;
        double rand_y = ((double)rand() / RAND_MAX);
        double real_y = func(rand_x);

        if (rand_y <= real_y)
            result++;
    }
    return result;
}

/**
 * This function calculates approximation of integral from counters of hit and total points.
 * @param total_randomized_points Number of total randomized points.
 * @param hit_points Number of hit points.
 * @param a Lower bound of integration
 * @param b Upper bound of integration
 * @return The approximation of integral
 */
double summarize_calculations(uint64_t total_randomized_points, uint64_t hit_points, float a, float b)
{
    return (b - a) * ((double)hit_points / (double)total_randomized_points);
}

/**
 * This function locks mutex and can sometime die (it has 2% chance to die).
 * It cannot die if lock would return an error.
 * It doesn't handle any errors. It's users responsibility.
 * Use it only in STAGE 4.
 *
 * @param mtx Mutex to lock
 * @return Value returned from pthread_mutex_lock.
 */
int random_death_lock(pthread_mutex_t* mtx)
{
    int ret = pthread_mutex_lock(mtx);
    if (ret)
        return ret;

    // 2% chance to die
    if (rand() % 50 == 0)
        abort();
    return ret;
}

void usage(char* argv[])
{
    printf("%s a b N - calculating integral with multiple processes\n", argv[0]);
    printf("a - Start of segment for integral (default: -1)\n");
    printf("b - End of segment for integral (default: 1)\n");
    printf("N - Size of batch to calculate before reporting to shared memory (default: 1000)\n");
}

int main(int argc, char* argv[])
{
    usage(argv);
    if (argc < 4) {
        ERR("za male wejscie");
        exit(1);
    }
    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    int N = atoi(argv[3]);

    int ctrl_fd = shm_open(LAB_NAMED_SHM_CTRL, O_CREAT | O_RDWR, 0600);
    if (ctrl_fd < 0 && errno == EEXIST) {
        if (shm_unlink(LAB_NAMED_SHM_CTRL) != 0) ERR("shm_unlink old ctrl");
        ctrl_fd = shm_open(LAB_NAMED_SHM_CTRL, O_CREAT | O_RDWR, 0600);
    }
    if (ctrl_fd < 0) ERR("shm_open ctrl");
    if (ftruncate(ctrl_fd, (off_t)sizeof(ctrl_block_t)) != 0) ERR("ftruncate ctrl");
    ctrl_block_t *ctrl = mmap(NULL, sizeof(ctrl_block_t), PROT_READ | PROT_WRITE, MAP_SHARED, ctrl_fd, 0);
    if (ctrl == MAP_FAILED) ERR("mmap ctrl");
    sem_t *named_sem = sem_open(LAB_NAMED_SEM, O_CREAT, 0600, 1);
    if (named_sem == SEM_FAILED) ERR("sem_open named");
    int czyz=0;
    errno = 0;

    int ret = sem_trywait(named_sem);
    if (ret==0 || errno != EAGAIN) {
        czyz=1;
        init_robust_mutex_pshared(&ctrl->robust_mutex);
    }

    robust_lock_or_recover(&ctrl->robust_mutex);
    ctrl->processes_count++;
    printf("pracujace procesy : %d\n", ctrl->processes_count);
    pthread_mutex_unlock(&ctrl->robust_mutex);


    sleep(2);
    robust_lock_or_recover(&ctrl->robust_mutex);
    ctrl->processes_count--;
    if (ctrl->processes_count == 0) {
        printf("czyszcze\n");
        pthread_mutex_unlock(&ctrl->robust_mutex);
        if (pthread_mutex_destroy(&ctrl->robust_mutex) != 0) ERR("pthread_mutex_destroy");
        if (sem_close(named_sem) != 0) ERR("sem_close parent named");
        close(ctrl_fd);
        if (shm_unlink(LAB_NAMED_SHM_CTRL) != 0) ERR("shm_unlink ctrl final");
    }else {
        pthread_mutex_unlock(&ctrl->robust_mutex);
    }

    munmap(ctrl,sizeof(ctrl_block_t));
    return EXIT_SUCCESS;
}