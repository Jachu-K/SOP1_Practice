#define _GNU_SOURCE
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// Zamiast printf w handlerze:
/*char buf[100];
snprintf(buf, sizeof(buf), "Sygnał od %d\n", info->si_pid);
write(STDOUT_FILENO, buf, strlen(buf));*/

// Jeden handler dla wszystkich sygnałów
void signal_handler(int sig, siginfo_t *info, void *context) {
  switch(sig) {
    case SIGUSR1:
      printf("Child[%d]: %d has coughed at me!\n", getpid(), info->si_pid);
      // Tutaj dodaj logikę dla SIGUSR1
      break;

    case SIGTERM:
      printf("Child[%d] exits with 0\n", getpid());
      exit(0);
      break;

    default:
      exit(0);
      break;
  }
}


void childrenwork(int k, int p) {
  char isIll[3] = "no";
  printf("Child[%d] starts day in the kindergarten, ill: %s\n", getpid(),
         isIll);
  random_time_milliseconds(300, 1000);

  sigset_t x;
  sigemptyset(&x);
  sigaddset(&x, SIGUSR1);
  sigaddset(&x, SIGTERM);
  sigprocmask(SIG_BLOCK, &x, NULL);
  struct sigaction sa;
  sigaction(SIGUSR1, &sa, NULL);

  // Konfiguracja sigaction
  sa.sa_sigaction = sigusr1_handler;  // Użyj sa_sigaction zamiast sa_handler
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;

  int sig = 0;
  while (1) {
    int info = sigwait(&x, &sig);
    if (sig == SIGUSR1) {

        printf("Child[{PID}]: {PID} has coughed at me!", getpid(), )
        int a = random_int(1,100);
        if (p>=a) {
          //choruje
        }
    }
    if (sig == SIGTERM) {
      printf("Child[%d] exits with 0\n", getpid());
      exit(0);
    }
  }
}

void parentwork(int t) {
  while (waitpid(-1, NULL, 0) > 0)
    ;

}

pid_t *create_children(int ile, int k, int p) {
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
      childrenwork(k, p);
      exit(0);
    } else {
      // Proces rodzica
      children[i] = pid;
      printf("Utworzono proces potomny o PID: %d\n", pid);
    }
  }

  return children;
}

int main(int argc, char **argv) {
  int t, k, n, p;
  if (argc < 5) {
    printf("Program musi być wywołany z 4 argumentami\n");
    exit(1);
  }
  t = strtol(argv[1], NULL, 10);
  k = strtol(argv[2], NULL, 10);
  n = strtol(argv[3], NULL, 10);
  p = strtol(argv[4], NULL, 10);

  set_sigact(SIGUSR1,SIG_IGN);
  set_sigact(SIGTERM,SIG_IGN);
  pid_t* dzieci = create_children(n, k, p);

  sigset_t x;
  sigemptyset(&x);
  sigaddset(&x, SIGALRM);
  sigprocmask(SIG_BLOCK, &x, NULL);
  int a;
  alarm(t);
  printf("KG[%d]: Alarm has been set for %d sec\n", getpid(), t);

  sigwait(&x, &a);
  printf("KG[%d]: Simulation has ended!\n", getpid());
  for (int i=0;i<n;i++) {
    kill(dzieci[i],SIGTERM);
  }
  return 0;
}
