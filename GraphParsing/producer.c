#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define FIFO_PATH "/tmp/graf_fifo"

// Funkcja generuje losową permutację liczb 0..n-1 (bez powtórzeń) – wykorzystywana do losowania sąsiadów
void random_permutation(int *array, int n) {
    for (int i = 0; i < n; i++) array[i] = i;
    for (int i = n-1; i > 0; i--) {
        int j = rand() % (i+1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Użycie: %s <liczba_wierzchołków>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    if (n <= 0) {
        fprintf(stderr, "Liczba wierzchołków musi być dodatnia.\n");
        return 1;
    }

    srand(time(NULL));

    // Utwórz FIFO (jeśli już istnieje, to zignoruj błąd)
    unlink(FIFO_PATH);  // usunięcie ewentualnego starego FIFO
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("mkfifo");
        return 1;
    }

    printf("Producent: utworzono FIFO %s\n", FIFO_PATH);
    printf("Producent: oczekiwanie na konsumenta...\n");

    // Otwórz FIFO do zapisu (blokujące)
    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open FIFO do zapisu");
        unlink(FIFO_PATH);
        return 1;
    }

    printf("Producent: konsument gotowy, wysyłanie danych...\n");

    // Strumień dla wygody (printf)
    FILE *fifo_out = fdopen(fd, "w");
    if (!fifo_out) {
        perror("fdopen");
        close(fd);
        unlink(FIFO_PATH);
        return 1;
    }

    // Wyślij liczbę wierzchołków
    fprintf(fifo_out, "%d\n", n);
    fflush(fifo_out);

    // Dla każdego wierzchołka generuj losowych sąsiadów
    int *neighbors = malloc(n * sizeof(int));
    for (int v = 1; v <= n; v++) {
        // Losujemy stopień (od 0 do n-1)
        int deg = rand() % n;

        // Losujemy deg unikalnych sąsiadów (mogą być z zakresu 1..n)
        random_permutation(neighbors, n);
        fprintf(fifo_out, "%d", v);
        for (int i = 0; i < deg; i++) {
            fprintf(fifo_out, " %d", neighbors[i] + 1); // +1 bo wierzchołki są numerowane od 1
        }
        fprintf(fifo_out, "\n");
        fflush(fifo_out);
    }

    free(neighbors);
    fclose(fifo_out); // zamknięcie strumienia i deskryptora
    unlink(FIFO_PATH);

    printf("Producent: dane wysłane, kończę pracę.\n");
    return 0;
}