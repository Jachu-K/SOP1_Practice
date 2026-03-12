#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void sigint_handler(int sig) {
    // Pusty handler
    printf("Otrzymano SIGINT\n");
}

int main() {
    // Prostsza rejestracja handlera (ale mniej przenośna niż sigaction)
    signal(SIGINT, sigint_handler);
    
    printf("Program uruchomiony. Naciśnij Ctrl+C...\n");
    
    while(1) {
        printf("Działam...\n");
        sleep(1);
    }
    
    return 0;
}