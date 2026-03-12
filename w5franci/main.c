
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

// Stała zdefiniowana w kodzie startowym (przykładowa wartość)
#define MAX_KNIGHT_NAME_LENGTH 100
#define PATH_MAX 100

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void set_sigact(int Signal, void (*f)(int))
{
    struct sigaction a = {f, 0, 0, 0, 0};
    sigaction(Signal, &a, NULL);
}
/*void child_sigusr1(int sig) {
    czy_dzialam=1;
}
void child_sigusr2(int sig) {
    czy_dzialam=0;
}*/
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

// 3) Funkcje losowego czasu z przedziałem [min, max]
void random_time_seconds(int min_seconds, int max_seconds)
{
    if (min_seconds < 0)
        min_seconds = 0;
    if (max_seconds < 0)
        max_seconds = 0;

    if (min_seconds > max_seconds)
    {
        int temp = min_seconds;
        min_seconds = max_seconds;
        max_seconds = temp;
    }

    int seconds = random_int(min_seconds, max_seconds);
    struct timespec ts = {.tv_sec = seconds, .tv_nsec = 0};

    printf("Czekam %d sekund (zakres [%d, %d])...\n", seconds, min_seconds, max_seconds);
    nanosleep(&ts, NULL);
}

void random_time_milliseconds(int min_milliseconds, int max_milliseconds)
{
    if (min_milliseconds < 0)
        min_milliseconds = 0;
    if (max_milliseconds < 0)
        max_milliseconds = 0;

    if (min_milliseconds > max_milliseconds)
    {
        int temp = min_milliseconds;
        min_milliseconds = max_milliseconds;
        max_milliseconds = temp;
    }

    int milliseconds = random_int(min_milliseconds, max_milliseconds);
    struct timespec ts = {.tv_sec = milliseconds / 1000, .tv_nsec = (milliseconds % 1000) * 1000000L};

    printf("Czekam %d milisekund (zakres [%d, %d])...\n", milliseconds, min_milliseconds, max_milliseconds);
    nanosleep(&ts, NULL);
}

void saraceniwork(int numer, int hp, int att, char* name) {}

void frankwork(int numer, int hp, int att)
{
    /*set_sigact(SIGUSR1,child_sigusr1);
    set_sigact(SIGUSR2,child_sigusr2);
    set_sigact(SIGINT, child_sigint);*/
}

int main()
{
    FILE *franks_file, *saracens_file;
    int n, m, i;
    char buffer[256];
    signal(SIGPIPE, SIG_IGN);
    // Próba otwarcia pliku z Frankami
    franks_file = fopen("franci.txt", "r");
    if (franks_file == NULL)
    {
        printf("Franks have not arrived on the battlefield\n");
        return 1;
    }

    // Próba otwarcia pliku z Saracenami
    saracens_file = fopen("saraceni.txt", "r");
    if (saracens_file == NULL)
    {
        printf("Saracens have not arrived on the battlefield\n");
        fclose(franks_file);
        return 1;
    }

    // Odczyt danych Franków
    if (fscanf(franks_file, "%d", &n) != 1)
    {
        fprintf(stderr, "Blad odczytu liczby Frankow\n");
        fclose(franks_file);
        fclose(saracens_file);
        return 1;
    }
    char name_fr[MAX_KNIGHT_NAME_LENGTH][n];
    int hp_fr[n];
    int attack_fr[n];
    for (i = 0; i < n; i++)
    {
        if (fscanf(franks_file, "%s %d %d", name_fr[i], &hp_fr[i], &attack_fr[i]) != 3)
        {
            fprintf(stderr, "Blad odczytu danych Franka\n");
            fclose(franks_file);
            fclose(saracens_file);
            return 1;
        }
    }
    int frankish_pipes[n][2];

    // Odczyt danych Saracenów
    if (fscanf(saracens_file, "%d", &m) != 1)
    {
        fprintf(stderr, "Blad odczytu liczby Saracenow\n");
        fclose(franks_file);
        fclose(saracens_file);
        return 1;
    }
    char name_s[MAX_KNIGHT_NAME_LENGTH][m];
    int hp_s[m];
    int attack_s[m];
    for (i = 0; i < m; i++)
    {
        if (fscanf(saracens_file, "%s %d %d", name_s[i], &hp_s[i], &attack_s[i]) != 3)
        {
            fprintf(stderr, "Blad odczytu danych Saracena\n");
            fclose(franks_file);
            fclose(saracens_file);
            return 1;
        }
    }
    int saraceni_pipes[m][2];
    for (int i = 0; i < n; i++)
    {
        // Utwórz indywidualny potok dla każdego thread3
        if (pipe(frankish_pipes[i]) < 0)
        {
            ERR("frankish pipes");
        }
    }
    for (int i = 0; i < m; i++)
    {
        // Utwórz indywidualny potok dla każdego thread3
        if (pipe(saraceni_pipes[i]) < 0)
        {
            ERR("saraceni pipes");
        }
    }
    pid_t saraceni[m];
    pid_t franks[n];
    for (int i = 0; i < m; i++)
    {
        saraceni[i] = fork();
        if (saraceni[i] == 0)
        {
            for (int j = 0; j < n; j++)
            {
                close(frankish_pipes[j][0]);
            }
            close(saraceni_pipes[i][1]);
            for (int j = 0; j < m; j++)
            {
                if (i != j)
                {
                    close(saraceni_pipes[j][0]);
                    close(saraceni_pipes[j][1]);
                }
            }
            int a = count_descriptors();
            printf("I am Spanish knight %s. I will serve my king with my %d HP and %d attack.\n Descriptors : %d\n",
                   name_s[i], hp_s[i], attack_s[i], a);
            fcntl(saraceni_pipes[i][0], F_SETFL, O_NONBLOCK);
            srand(getpid());
            while (1)
            {
                ssize_t bytes_read = read(saraceni_pipes[i][0], buffer, sizeof(buffer) - 1);
                for (int l = 0; l < bytes_read; l++)
                {
                    int damage = buffer[l];
                    hp_s[i] -= damage;
                    if (hp_s[i] <= 0)
                    {
                        printf("%s dies\n", name_s[i]);
                        close(saraceni_pipes[i][0]);
                        for (int i = 0; i < n; i++)
                        {
                            close(frankish_pipes[i][1]);
                        }
                        exit(0);
                    }
                }
                int wrog = random_int(0, n - 1);
                int s = random_int(0, attack_s[i]);
                char S[1];
                S[0] = s;
                ssize_t written = write(frankish_pipes[wrog][1], S, 1);
                if (written == -1 && errno == EPIPE)
                {
                    close(frankish_pipes[wrog][1]);
                    frankish_pipes[wrog][1] = frankish_pipes[n - 1][1];
                    n--;
                    if (n < 1)
                    {
                        printf("%s returns home\n", name_s[i]);
                        close(saraceni_pipes[i][0]);
                        for (int i = 0; i < n; i++)
                        {
                            close(frankish_pipes[i][1]);
                        }
                        exit(0);
                    }
                }

                if (s == 0)
                {
                    printf("%s attacks his enemy, however he deflected\n", name_s[i]);
                }
                else if (s < 6)
                {
                    printf("%s goes to strike, he hit right and well\n", name_s[i]);
                }
                else
                {
                    printf("%s strikes powerful blow, the shield he breaks and inflicts a big wound\n", name_s[i]);
                }
                random_time_milliseconds(1, 10);
            }
            // saraceniwork(i,hp_s[i],attack_s[i]);
            exit(0);
        }
    }
    for (int i = 0; i < n; i++)
    {
        franks[i] = fork();
        if (franks[i] == 0)
        {
            for (int j = 0; j < m; j++)
            {
                close(saraceni_pipes[j][0]);
            }
            close(frankish_pipes[i][1]);
            for (int j = 0; j < n; j++)
            {
                if (i != j)
                {
                    close(frankish_pipes[j][0]);
                    close(frankish_pipes[j][1]);
                }
            }
            int a = count_descriptors();
            printf("I am Frankish knight %s. I will serve my king with my %d HP and %d attack.\n Descriptors : %d\n",
                   name_fr[i], hp_fr[i], attack_fr[i], a);
            fcntl(frankish_pipes[i][0], F_SETFL, O_NONBLOCK);
            srand(getpid());
            while (1)
            {
                ssize_t bytes_read = read(frankish_pipes[i][0], buffer, sizeof(buffer) - 1);
                if (bytes_read > 0)
                {
                    for (int l = 0; l < bytes_read; l++)
                    {
                        int damage = buffer[l];
                        hp_fr[i] -= damage;
                        printf("%s i got %d damage\n", name_fr[i], damage);
                    }
                    if (hp_fr[i] <= 0)
                    {
                        printf("%s dies\n", name_fr[i]);
                        close(frankish_pipes[i][0]);
                        for (int p = 0; p < m; p++)
                        {
                            close(saraceni_pipes[p][1]);
                        }
                        exit(0);
                    }
                }
                else if (bytes_read == 0)
                {
                    // Pipe zamknięty - koniec walki
                    printf("%s: battle ended\n", name_s[i]);
                    close(frankish_pipes[i][0]);
                    for (int p = 0; p < m; p++)
                    {
                        close(saraceni_pipes[p][1]);
                    }
                    exit(0);
                }
                else
                {  // bytes_read == -1
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // Brak danych - dodajemy opóźnienie żeby nie męczyć CPU
                        continue;
                    }
                    else
                    {
                        perror("read error");
                        exit(0);
                    }
                }
                int wrog = random_int(0, m - 1);
                int s = random_int(0, attack_fr[i]);
                char S[1];
                S[0] = s;
                ssize_t written = write(saraceni_pipes[wrog][1], S, 1);
                if (written == -1 && errno == EPIPE)
                {
                    close(saraceni_pipes[wrog][1]);
                    saraceni_pipes[wrog][1] = saraceni_pipes[m - 1][1];
                    m--;
                    if (m < 1)
                    {
                        printf("%s returns home\n", name_fr[i]);
                        close(frankish_pipes[i][0]);
                        for (int p = 0; p < m; p++)
                        {
                            close(saraceni_pipes[p][1]);
                        }
                        exit(0);
                    }
                }
                if (s == 0)
                {
                    printf("%s attacks his enemy, however he deflected\n", name_fr[i]);
                }
                else if (s < 6)
                {
                    printf("%s goes to strike, he hit right and well\n", name_fr[i]);
                }
                else
                {
                    printf("%s strikes powerful blow, the shield he breaks and inflicts a big wound\n", name_fr[i]);
                }
                random_time_milliseconds(1, 10);
            }
            // frankwork(i,hp_fr[i],attack_fr[i]);
            srand(getpid());
            exit(0);
        }
    }
    for (int i = 0; i < m; i++)
    {
        close(saraceni_pipes[i][0]);
        close(saraceni_pipes[i][1]);
    }
    for (int i = 0; i < n; i++)
    {
        close(frankish_pipes[i][0]);
        close(frankish_pipes[i][1]);
    }
    while (waitpid(-1, NULL, 0) > 0)
        ;
    printf("Dzieci sie skonczyly\n");
    // Zamknięcie plików
    fclose(franks_file);
    fclose(saracens_file);
    return 0;
}
