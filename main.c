#include "my_unistd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Error, missing argument. Usage %s <filename>\n", argv[0]);
        return 1;
    }
	init(argv[1]);
    
    char path[256] = "/";
    char input[256];
    
    while (1) {
        printf("%s > ", path);
        if (!fgets(input, sizeof(input), stdin)) {
            printf("\n");
            break;
        }
        if (!strcmp(input, "ls\n")) {
            if (!fork()) {
                execl("ls/ls", "ls/ls", argv[1], path, 0);
            } else {
                wait(0);
            }
        } else if (!strcmp(input, "exit\n")) {
            break;
        }
    }

    deinit();
    return 0;
}
