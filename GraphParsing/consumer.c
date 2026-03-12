#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define FIFO_PATH "/tmp/graf_fifo"

// Struktura listy sąsiedztwa (jak w pierwszym zadaniu)
typedef struct Node {
    int vertex;
    struct Node* next;
} Node;

void addEdge(Node** head, int v) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->vertex = v;
    newNode->next = *head;
    *head = newNode;
}

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

int main() {
    printf("Konsument: oczekiwanie na dane z FIFO %s...\n", FIFO_PATH);

    // Otwórz FIFO do odczytu (blokujące)
    int fd = open(FIFO_PATH, O_RDONLY);
    if (fd == -1) {
        perror("open FIFO do odczytu");
        return 1;
    }

    FILE *fifo_in = fdopen(fd, "r");
    if (!fifo_in) {
        perror("fdopen");
        close(fd);
        return 1;
    }

    // Wczytaj liczbę wierzchołków
    int n;
    if (fscanf(fifo_in, "%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Konsument: błąd odczytu liczby wierzchołków\n");
        fclose(fifo_in);
        return 1;
    }

    // Alokacja list sąsiedztwa (indeksy 1..n)
    Node** adjList = (Node**)calloc(n + 1, sizeof(Node*));
    if (!adjList) {
        fprintf(stderr, "Konsument: błąd alokacji pamięci\n");
        fclose(fifo_in);
        return 1;
    }

    // Odczytujemy dane linia po linii
    char line[1024];
    fgets(line, sizeof(line), fifo_in); // wczytuje resztę pierwszej linii (powinna być pusta)

    for (int i = 0; i < n; i++) {
        if (!fgets(line, sizeof(line), fifo_in)) {
            fprintf(stderr, "Konsument: nieoczekiwany koniec danych\n");
            freeGraph(adjList, n);
            fclose(fifo_in);
            return 1;
        }

        // Usuń znak nowej linii
        line[strcspn(line, "\n")] = 0;

        char* token = strtok(line, " \t");
        if (!token) {
            fprintf(stderr, "Konsument: pusta linia, ignoruję\n");
            continue;
        }

        int vertex = atoi(token);
        if (vertex < 1 || vertex > n) {
            fprintf(stderr, "Konsument: nieprawidłowy wierzchołek %d\n", vertex);
            continue;
        }

        token = strtok(NULL, " \t");
        while (token) {
            int neighbor = atoi(token);
            if (neighbor >= 1 && neighbor <= n) {
                addEdge(&adjList[vertex], neighbor);
            } else {
                fprintf(stderr, "Konsument: nieprawidłowy sąsiad %d dla wierzchołka %d\n", neighbor, vertex);
            }
            token = strtok(NULL, " \t");
        }
    }

    fclose(fifo_in);

    // Wyświetlenie odebranego grafu
    printf("Konsument: odebrano graf – lista sąsiedztwa:\n");
    printGraph(adjList, n);

    freeGraph(adjList, n);
    return 0;
}