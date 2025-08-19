#include "stdio.h"
#include "signal.h"
#include "unistd.h"
#include <stdlib.h>

void handle_SIGHUP() {
    // Function to handle HUP signal.
    // Whenever it receive HUP, the program will prinout "Ouch!" and continue
    write(STDOUT_FILENO, "Ouch!", 6);
}
void handle_SIGINT() {
    // Function to handle INT signal.
    // Whenever it receive INT, the program will prinout "Yeah!" and continue
    write(STDOUT_FILENO, "Yeah!\n", 6);
}
int main(int argc, char *argv[]) {
    // Init the default signal handler for SIGHUP and SIGINT by our customise function 
    signal(SIGHUP, handle_SIGHUP);
    signal(SIGINT, handle_SIGINT);

    // Validate input 
    if (argc != 2) {
        printf("Usage: %s <integer>\n", argv[0]);
        return 1;
    }

    // Turn argument into INT type
    int n = atoi(argv[1]);

    // Check if the argument is negative 
    if (n < 0) return 1;

    // Runing the loop to print out the first n even numbers
    // Each loop, the program is forced to sleep in 5 seconds
    for(int i = 0; i < n; i++) {
        printf("%d \n", i*2);
        
        // Force program to print before sleep 
        fflush(stdout);
        sleep(5);
    }
    return 0;
}