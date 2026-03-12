#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Użycie: %s <nazwa_pliku>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("Błąd otwarcia pliku");
        return 1;
    }

    int n;
    if (fscanf(file, "%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Nieprawidłowa liczba wierzchołków\n");
        fclose(file);
        return 1;
    }

    // Alokacja macierzy n x n (indeksy 0..n-1)
    int** matrix = (int**)malloc(n * sizeof(int*));
    if (!matrix) {
        fprintf(stderr, "Błąd alokacji pamięci\n");
        fclose(file);
        return 1;
    }
    for (int i = 0; i < n; i++) {
        matrix[i] = (int*)calloc(n, sizeof(int));  // zerowanie
        if (!matrix[i]) {
            fprintf(stderr, "Błąd alokacji wiersza %d\n", i);
            for (int j = 0; j < i; j++) free(matrix[j]);
            free(matrix);
            fclose(file);
            return 1;
        }
    }

    // Pomijamy resztę pierwszej linii (znak nowej linii)
    char line[1024];
    fgets(line, sizeof(line), file);

    // Wczytujemy dokładnie n linii
    for (int i = 0; i < n; i++) {
        if (!fgets(line, sizeof(line), file)) {
            fprintf(stderr, "Nieoczekiwany koniec pliku\n");
            for (int j = 0; j < n; j++) free(matrix[j]);
            free(matrix);
            fclose(file);
            return 1;
        }

        // Usuwamy znak nowej linii
        line[strcspn(line, "\n")] = 0;

        // Dzielimy linię na tokeny
        char* token = strtok(line, " \t");
        if (!token) {
            fprintf(stderr, "Ostrzeżenie: pusta linia %d\n", i + 2);
            continue;
        }

        int vertex = atoi(token);
        if (vertex < 1 || vertex > n) {
            fprintf(stderr, "Nieprawidłowy numer wierzchołka: %d (linia %d)\n", vertex, i + 2);
            continue;  // pomijamy całą linię
        }

        int row = vertex - 1;  // indeks w macierzy

        // Przetwarzamy sąsiadów
        token = strtok(NULL, " \t");
        while (token) {
            int neighbor = atoi(token);
            if (neighbor >= 1 && neighbor <= n) {
                int col = neighbor - 1;
                matrix[row][col] = 1;  // ustawiamy krawędź
            } else {
                fprintf(stderr, "Nieprawidłowy sąsiad: %d dla wierzchołka %d\n", neighbor, vertex);
            }
            token = strtok(NULL, " \t");
        }
    }

    fclose(file);

    // Wyświetlamy macierz
    printf("Macierz sąsiedztwa (%d x %d):\n", n, n);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }

    // Zwalniamy pamięć
    for (int i = 0; i < n; i++) free(matrix[i]);
    free(matrix);

    return 0;
}