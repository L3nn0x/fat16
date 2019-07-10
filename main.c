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

    if (fork()) {
        execl("ls/ls", argv[1]);
    } else {
        deinit();
    }
	return 0;
}
