#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Struktura elementu listy sąsiedztwa
typedef struct Node {
    int vertex;
    struct Node* next;
} Node;

// Funkcja dodaje nowy wierzchołek na początek listy (dla prostoty)
void addEdge(Node** head, int v) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->vertex = v;
    newNode->next = *head;
    *head = newNode;
}

// Funkcja wyświetla listę sąsiedztwa (opcjonalnie)
void printGraph(Node** adjList, int n) {
    for (int i = 1; i <= n; i++) {
        printf("%d: ", i);
        Node* temp = adjList[i];
        while (temp) {
            printf("%d ", temp->vertex);
            temp = temp->next;
        }
        printf("\n");
    }
}

// Funkcja zwalnia pamięć zaalokowaną dla listy sąsiedztwa
void freeGraph(Node** adjList, int n) {
    for (int i = 1; i <= n; i++) {
        Node* temp = adjList[i];
        while (temp) {
            Node* toFree = temp;
            temp = temp->next;
            free(toFree);
        }
    }
    free(adjList);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    int n;
    // Wczytaj liczbę wierzchołków
    if (fscanf(file, "%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Invalid number of vertices\n");
        fclose(file);
        return 1;
    }

    // Alokacja tablicy list (indeksy 1..n)
    Node** adjList = (Node**)calloc(n + 1, sizeof(Node*));
    if (!adjList) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return 1;
    }

    // Usuń pozostałość pierwszej linii (znak nowej linii po liczbie wierzchołków)
    char line[1024];
    fgets(line, sizeof(line), file);  // powinna zawierać tylko '\n' lub być pusta

    // Wczytaj dokładnie n linii z danymi
    for (int i = 0; i < n; i++) {
        if (!fgets(line, sizeof(line), file)) {
            fprintf(stderr, "Unexpected end of file\n");
            freeGraph(adjList, n);
            fclose(file);
            return 1;
        }

        // Usuń znak nowej linii z końca
        line[strcspn(line, "\n")] = 0;

        // Podziel linię na tokeny
        char* token = strtok(line, " \t");
        if (!token) {
            fprintf(stderr, "Warning: empty line %d\n", i + 2);
            continue;
        }

        int vertex = atoi(token);
        if (vertex < 1 || vertex > n) {
            fprintf(stderr, "Invalid vertex number: %d (line %d)\n", vertex, i + 2);
            // Mimo błędu próbujemy czytać dalej (linia może zawierać sąsiadów)
        }

        // Pozostałe tokeny to sąsiedzi
        token = strtok(NULL, " \t");
        while (token) {
            int neighbor = atoi(token);
            if (neighbor >= 1 && neighbor <= n) {
                // Dodaj krawędź (jednokierunkowo, zgodnie z plikiem)
                addEdge(&adjList[vertex], neighbor);
            } else {
                fprintf(stderr, "Invalid neighbor: %d for vertex %d\n", neighbor, vertex);
            }
            token = strtok(NULL, " \t");
        }
    }

    fclose(file);

    // Wyświetlenie listy sąsiedztwa (można pominąć)
    printf("Lista sąsiedztwa:\n");
    printGraph(adjList, n);

    // Zwolnienie pamięci
    freeGraph(adjList, n);

    return 0;
}