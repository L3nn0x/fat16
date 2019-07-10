#ifndef MY_UNISTD
#define MY_UNISTD

#include <string.h>
#include <unistd.h>

#define MY_O_APPEND 0x1
#define MY_O_CREAT  0x2
#define MY_O_DIRECTORY 0x4
#define MY_O_READ 0x8
#define MY_O_WRITE 0x10

#define MY_SEEK_SET 0x1
#define MY_SEEK_CUR 0x2
#define MY_SEEK_END 0x4


// only here because the device is a file in the host
void init(const char* filename);
void deinit();

int my_open(const char* pathname, int flags);
int my_close(int fd);
ssize_t my_read(int fd, void *buff, size_t count);
ssize_t my_write(int fd, const void* buff, size_t count);
ssize_t my_lseek(int fd, ssize_t offset, int whence);

extern const char* my_errno;

#define MY_DT_DIR 0x1
#define MY_DT_REG 0x2

struct MY_DIR;
typedef struct MY_DIR MY_DIR;

struct my_dirent {
    char* d_name;
    unsigned char d_type;
};

MY_DIR *my_opendir(const char* name);
int my_closedir(MY_DIR* dir);
struct my_dirent* my_readdir(MY_DIR* dir);

#endif
