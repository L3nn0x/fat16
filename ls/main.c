#include "my_unistd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Error, missing argument. Usage %s <filename>\n", argv[0]);
        return 1;
    }

    init(argv[1]);

    MY_DIR* dir = my_opendir(argv[2]);
    if (!dir) {
        printf("Error: %s\n", my_errno);
        return -1;
    }
    struct my_dirent* entry;
    while ((entry = my_readdir(dir)) != 0) {
        printf("%s (%c)\n", entry->d_name, entry->d_type == MY_DT_DIR ? 'd' : 'f');
    }
    my_closedir(dir);

    /*int fd = my_open("FILE.TXT", MY_O_READ);
    if (fd == -1) {
        printf("Error: %s\n", my_errno);
        return -1;
    }
    char data[256] = {0};
    if (my_read(fd, data, sizeof(data)) == -1) {
        printf("Error: %s\n", my_errno);
        return -1;
    }
    printf("data: %s", data);
    my_close(fd);

    MY_DIR* dir = my_opendir("/");
    if (!dir) {
        printf("Error: %s\n", my_errno);
        return -1;
    }
    struct my_dirent* entry;
    while ((entry = my_readdir(dir)) != 0) {
        printf("%s (%c)\n", entry->d_name, entry->d_type == MY_DT_DIR ? 'd' : 'f');
    }
    my_closedir(dir);*/

    deinit();
	return 0;
}
