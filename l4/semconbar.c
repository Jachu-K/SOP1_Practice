#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

pthread_barrier_t barrier;
    int n;
    pthread_barrier_init(&barrier, NULL, n);
    int result = pthread_barrier_wait(&barrier);
    if (result == PTHREAD_BARRIER_SERIAL_THREAD) {
    }
    pthread_barrier_destroy(&barrier);

    pthread_cond_t cond;
    pthread_mutex_t mutex;

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);

    pthread_mutex_lock(&mutex);
    while (*data->cond_signaled == 0) {
        pthread_cond_wait(data->cond, data->cond_mx);
    }
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);
    pthread_cond_broadcast(&cond);

    sem_t semaphore;
    int initial_value;
    int res = sem_init(&semaphore, 0, initial_value);
    if (res != 0) {
        // Błąd inicjalizacji
    }
    sem_wait(&semaphore);
    sem_trywait(&semaphore);

    sem_post(&semaphore);

    int value;
    sem_getvalue(&semaphore, &value);
    sem_destroy(&semaphore);
    
    barier()
    if(serial && exit_flag){
      next_flag = 1;
    }
    barier()
    if(next_flag)end
