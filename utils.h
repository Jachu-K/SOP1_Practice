//
// Created by jan on 30.11.2025.


#include <signal.h>
#include <sys/types.h>

typedef void (*signal_handler_t)(int);

typedef struct {
    int signal;
    signal_handler_t handler;
} signal_config_t;

// Deklaracje funkcji
ssize_t bulk_read(int fd, char *buf, size_t count);
ssize_t bulk_write(int fd, char *buf, size_t count);
int random_int(int a, int b);
void set_sigact(int Signal, void (*f)(int));
void parentwork();
void childrenwork(int numer);
pid_t *create_children(int ile);
void random_time_seconds(int min_seconds, int max_seconds);
void random_time_milliseconds(int min_miliseconds, int max_milliseconds);
