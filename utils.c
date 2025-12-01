//
// Created by jan on 30.11.2025.
//
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

ssize_t bulk_read(int fd, char *buf, size_t count) {
  ssize_t c;
  ssize_t len = 0;
  do {
    c = TEMP_FAILURE_RETRY(read(fd, buf, count));
    if (c < 0)
      return c;
    if (c == 0)
      return len; // EOF
    buf += c;
    len += c;
    count -= c;
  } while (count > 0);
  return len;
}

ssize_t bulk_write(int fd, char *buf, size_t count) {
  ssize_t c;
  ssize_t len = 0;
  do {
    c = TEMP_FAILURE_RETRY(write(fd, buf, count));
    if (c < 0)
      return c;
    buf += c;
    len += c;
    count -= c;
  } while (count > 0);
  return len;
}

int random_int(int a, int b) {
  if (a > b) {
    // Zamiana wartości jeśli a > b
    int temp = a;
    a = b;
    b = temp;
  }

  // Inicjalizacja generatora losowego tylko raz
  static int initialized = 0;
  if (!initialized) {
    srand(time(NULL) + getpid());
    initialized = 1;
  }

  // Generowanie liczby z zakresu [a, b]
  return rand() % (b - a + 1) + a;
}

// 3) Funkcje losowego czasu z przedziałem [min, max]
void random_time_seconds(int min_seconds, int max_seconds) {
  if (min_seconds < 0)
    min_seconds = 0;
  if (max_seconds < 0)
    max_seconds = 0;

  if (min_seconds > max_seconds) {
    int temp = min_seconds;
    min_seconds = max_seconds;
    max_seconds = temp;
  }

  int seconds = random_int(min_seconds, max_seconds);
  struct timespec ts = {.tv_sec = seconds, .tv_nsec = 0};

  printf("Czekam %d sekund (zakres [%d, %d])...\n", seconds, min_seconds,
         max_seconds);
  nanosleep(&ts, NULL);
}

void random_time_milliseconds(int min_milliseconds, int max_milliseconds) {
  if (min_milliseconds < 0)
    min_milliseconds = 0;
  if (max_milliseconds < 0)
    max_milliseconds = 0;

  if (min_milliseconds > max_milliseconds) {
    int temp = min_milliseconds;
    min_milliseconds = max_milliseconds;
    max_milliseconds = temp;
  }

  int milliseconds = random_int(min_milliseconds, max_milliseconds);
  struct timespec ts = {.tv_sec = milliseconds / 1000,
                        .tv_nsec = (milliseconds % 1000) * 1000000L};

  printf("Czekam %d milisekund (zakres [%d, %d])...\n", milliseconds,
         min_milliseconds, max_milliseconds);
  nanosleep(&ts, NULL);
}

// Typy dla funkcji obsługi sygnałów
typedef void (*signal_handler_t)(int);

// Struktura do konfiguracji obsługi sygnałów
typedef struct {
  int signal;
  signal_handler_t handler;
} signal_config_t;

// 1) Funkcja ustawiająca obsługę sygnałów
void set_sigact(int Signal, void (*f)(int)) {
  struct sigaction a = {f, 0, 0, 0, 0};
  sigaction(Signal, &a, NULL);
}

// 2) Funkcje dla procesów

/*pid_t* create_children(int ile, int war) {
    if (ile <= 0) {
        return NULL;
    }

    pid_t *children = malloc(ile * sizeof(pid_t));
    if (!children) {
        perror("malloc");
        return NULL;
    }

    for (int i = 0; i < ile; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            free(children);
            return NULL;
        } else if (pid == 0) {
            // Proces potomny
            free(children); // dziecko nie potrzebuje tej tablicy
            childrenwork(war);
            exit(0);
        } else {
            // Proces rodzica
            children[i] = pid;
            printf("Utworzono proces potomny o PID: %d\n", pid);
        }
    }

    return children;
}*/

// Przykład użycia
/*int main() {
    printf("Start programu (PID: %d)\n", getpid());

    // Konfiguracja obsługi sygnałów
    signal_config_t signals[] = {
        {SIGINT, default_signal_handler},
        {SIGTERM, default_signal_handler},
        {SIGCHLD, default_signal_handler},
        {SIGUSR1, default_signal_handler},
        {SIGUSR2, default_signal_handler}
    };

    setup_signal_handlers(signals, 5);

    // Tworzenie procesów potomnych
    int child_count = 3;
    pid_t *children = create_children(child_count);

    if (children) {
        // Kod rodzica
        for (int i = 0; i < 3; i++) {
            parentwork();
            random_time_seconds(5); // Losowy czas 1-5 sekund
            random_time_milliseconds(2000); // Losowy czas 1-2000 ms
        }

        // Oczekiwanie na zakończenie dzieci
        printf("Oczekiwanie na zakończenie procesów potomnych...\n");
        for (int i = 0; i < child_count; i++) {
            waitpid(children[i], NULL, 0);
        }

        free(children);
    }

    printf("Koniec programu\n");
    return 0;
}*/
