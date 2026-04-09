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

#define FILENAME "tekst.txt"
#define MAX_LEN 1024

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

void child_work(char* ptr, int* res) {
    srand(getpid());
    int ile[26];
    for (int j=0;j<26;j++) {
        ile[j]=0;
    }
    int ilec[10];
    for (int j=0;j<10;j++) {
        ilec[j]=0;
    }
    int ilesp=0;
    int ileen=0;
    int i=0;
    while (ptr[i]!=EOF && (ptr[i]!=0)) {
        if (ptr[i]>='a' && ptr[i]<='z') {
            ile[ptr[i]-'a']++;
        }else if (ptr[i]>='0' && ptr[i]<='9') {
            ilec[ptr[i]-'0']++;
        }else if (ptr[i]=='\n') {
            ileen++;
        }else if (ptr[i]==' ') {
            ilesp++;
        }
        i++;
    }
    if (rand()%100 <= 3) {
        abort();
    }
    res[0]=ilesp;
    res[1]=ileen;
    for (i=0;i<26;i++) {
        res[i+2]=ile[i];
    }
    for (i=0;i<10;i++) {
        res[i+28]=ilec[i];
    }
}

int main(void) {
    int n = 4;
    pid_t pids[n];
    printf("Hello, World!\n");
    int fd = open(FILENAME,O_RDONLY);
    char * ptr = mmap(NULL,MAX_LEN,PROT_READ,MAP_SHARED,fd,0);
    int * wyn = mmap(NULL,MAX_LEN,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);


    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid==0) {
            child_work(ptr,wyn+(sizeof(int)*38*i));
            exit(0);
        }else {
            pids[i]=pid;
        }
    }
    int er=0;
    for (int i = 0; i < n; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (WIFSIGNALED(status)) {
            er=1;
        }
    }
    if (er) {
        printf("obliczenia się nie powiodły\n");
        exit(0);
    }
    printf("spacje : %d\n entery : %d\n",wyn[0],wyn[1]);
    for (int i=0;i<26;i++) {
        if (wyn[i+2]>0)printf("%c : %d\n",i+'a',wyn[i+2]);
    }
    for (int i=0;i<10;i++) {
        if (wyn[i+28]>0)printf("%c : %d\n",i+'0',wyn[i+28]);
    }

    return 0;
}