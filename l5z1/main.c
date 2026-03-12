
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_MSG 32
#define MAX_NAME 100
#define MAX_LINE 256
#define MAX_WARRIORS 100

typedef struct {
    char name[MAX_NAME];
    int strength;
    int hp;
} Character;

/**
 * Czyta plik i zapisuje dane do tablicy warriors (maks. max_warriors elementów).
 * Zwraca liczbę wczytanych rekordów lub -1 w przypadku błędu otwarcia pliku.
 */
int parse_file(const char *filename, Character warriors[], int max_warriors) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Błąd otwarcia pliku");
        return -1;
    }

    char line[MAX_LINE];
    int count = 0;

    /*
    char* line = NULL;
    size_t count = 0;
    while (count < max_warriors && getline(&line, &count, file)) {
    */

    while (count < max_warriors && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';  // usuwamy newline
        int parsed = sscanf(line, "%99[^;];%d;%d",
                            warriors[count].name,
                            &warriors[count].strength,
                            &warriors[count].hp);

        if (parsed == 3) {
            count++;  // tylko gdy wszystkie pola wczytane
        } else {
            fprintf(stderr, "Pominięto niepoprawną linię: %s\n", line);
        }
    }

    fclose(file);
    return count;
}

int random_int(int a, int b)
{
    if (a > b)
    {
        // Zamiana wartości jeśli a > b
        int temp = a;
        a = b;
        b = temp;
    }

    // Inicjalizacja generatora losowego tylko raz
    static int initialized = 0;
    if (!initialized)
    {
        srand(time(NULL) + getpid());
        initialized = 1;
    }

    // Generowanie liczby z zakresu [a, b]
    return rand() % (b - a + 1) + a;
}

int main(void) {
    char msg[MAX_MSG] ;
    signal(SIGPIPE, SIG_IGN);

    int pipes[3][2]; // 0 czyt 1 pisz itd...
    for (int i = 0; i < 3; i++)
    {
        // Utwórz indywidualny potok dla każdego thread3
        if (pipe(pipes[i]) < 0)
        {
            ERR("pipes");
        }
    }
    int x = 1;
    pid_t pid = fork();
    if (pid == 0) {
        close(pipes[0][1]); //swoje od pisania zamykam
        close(pipes[1][0]);
        close(pipes[2][0]);
        close(pipes[2][1]);
        while (1) {
            ssize_t red = read(pipes[0][0],msg,sizeof(msg));
            if (red == 0) {
                printf("bylo 0, %d\n",getpid());
                exit(0);
            }
            x = atoi(msg);
            if (x==0) {
                printf("mam 0, %d\n",getpid());
                close(pipes[0][0]);
                close(pipes[1][1]);
                exit(0);
            }
            printf("%d %d\n", x, getpid());
            x += random_int(-10,10);
            snprintf(msg,MAX_MSG,"%d",x);
            write(pipes[1][1],msg,sizeof(msg));
        }
        exit(0);
    }
    pid = fork();
    if (pid == 0) {
        close(pipes[0][1]);
        close(pipes[0][0]);
        close(pipes[1][1]);
        close(pipes[2][0]);
        while (1) {
            ssize_t red = read(pipes[1][0],msg,sizeof(msg));
            if (red == 0) {
                printf("bylo 0, %d\n", getpid());
                exit(0);
            }
            x = atoi(msg);
            if (x==0) {
                printf("mam 0, %d\n",getpid());
                close(pipes[1][0]);
                close(pipes[2][1]);
                exit(0);
            }
            printf("%d %d\n", x, getpid());
            x += random_int(-10,10);
            snprintf(msg,MAX_MSG,"%d",x);
            write(pipes[2][1],msg,sizeof(msg));
        }
        exit(0);
    }
    close(pipes[0][0]);
    close(pipes[1][1]);
    close(pipes[2][1]);
    close(pipes[1][0]);
    snprintf(msg,MAX_MSG,"%d",x);
    write(pipes[0][1],msg,sizeof(msg));
    while (1) {
        ssize_t red = read(pipes[2][0],msg,sizeof(msg));
        if (red == 0) {
            printf("bylo 0, %d\n", getpid());
            exit(0);
        }
        x = atoi(msg);
        if (x==0) {
            printf("mam 0, %d\n",getpid());
            close(pipes[2][0]);
            close(pipes[0][1]);
            exit(0);
        }
        printf("%d %d\n", x, getpid());
        x += random_int(-10,10);
        snprintf(msg,MAX_MSG,"%d",x);
        write(pipes[0][1],msg,sizeof(msg));
    }
    while (waitpid(-1, NULL, 0) > 0)
        ;
}