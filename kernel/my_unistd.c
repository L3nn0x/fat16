#include "my_unistd.h"
#include "file.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>

const char* no_file = "No such file or directory";
const char* no_dir = "Not a directory";
const char* incorrect_fd = "The file descriptor isn't attached to any file";
const char* before_file_start = "The offset is set before the start of the file";
const char* file_not_open_for_read = "The file is not open for read";
const char* file_not_open_for_write = "The file is not open for write";

const char* my_errno = 0;

struct OpenedFile {
    int nb;
    struct File* file;
    int perms;
};

static struct Deque* deque = 0;
static struct Device* device = 0;
static struct File* root = 0;

static void free_openedFile(void* fd) {
    struct OpenedFile* tmp = (struct OpenedFile*)fd;
    closeFile(tmp->file, tmp->perms & MY_O_WRITE);
    free(fd);
}

void init(const char* filename) {
    FILE* hdd = fopen(filename, "rb+");
    device = openDevice(hdd);
    root = openRoot(device);
}

void deinit() {
    FILE* hdd = device->hdd;
    closeFile(root, 1);
	closeDevice(device);
    fclose(hdd);
}

int my_open(const char* pathname, int flags) {
    if (!deque) {
        deque = create_deque();
    }
    struct File* file = pathToFile(root, pathname);
    if (!file && !(flags & MY_O_CREAT)) {
        my_errno = no_file;
        return -1;
    }
    if (!file->isDirectory && flags & MY_O_DIRECTORY) {
        closeFile(file, 0);
        my_errno = no_dir;
        return -1;
    }
    for (void* const* it = cbegin_deque(deque); it != cend_deque(deque); ++it) {
        if (*it == file) {
            return (int)(it - cbegin_deque(deque));
        }
    }
    struct OpenedFile* fd = malloc(sizeof(*fd));
    fd->nb = 1;
    openFile(file);
    if (flags & MY_O_APPEND) {
        file->offset = file->size;
    }
    fd->perms = flags;
    fd->file = file;
    push_back_deque(deque, fd);
    return size_deque(deque) - 1;
}

int my_close(int fd) {
    if (fd < 0 || fd >= size_deque(deque)) {
        my_errno = incorrect_fd;
        return -1;
    }
    struct OpenedFile *file = get_elem_deque(deque, fd);
    --file->nb;
    if (!file->nb) {
        delete_elem_deque(deque, fd, &free_openedFile);
    }
}

ssize_t my_read(int fd, void *buff, size_t count) {
    if (fd < 0 || fd >= size_deque(deque)) {
        my_errno = incorrect_fd;
        return -1;
    }
    struct OpenedFile* file = get_elem_deque(deque, fd);
    if (!file->perms & MY_O_READ) {
        my_errno = file_not_open_for_read;
        return -1;
    }
    size_t offset = file->file->offset;
    size_t bytesRead = file->file->size > offset + count ? count : file->file->size - offset;
    memcpy(buff, &(file->file->mmap[offset]), bytesRead);
    file->file->offset += bytesRead;
    return bytesRead;
}

ssize_t my_write(int fd, const void* buff, size_t count) {
    if (fd < 0 || fd >= size_deque(deque)) {
        my_errno = incorrect_fd;
        return -1;
    }
    struct OpenedFile* file = get_elem_deque(deque, fd);
    if (!file->perms & MY_O_WRITE) {
        my_errno = file_not_open_for_write;
        return -1;
    }
    size_t offset = file->file->offset;
    if (offset + count > file->file->size) {
        size_t size = file->file->size;
        file->file->size = offset + count;
        file->file->mmap = realloc(file->file->mmap, file->file->size);
        if (offset > size) {
            bzero(&(file->file->mmap[size]), offset - size);
        }
    }
    memcpy(&(file->file->mmap[offset]), buff, count);
    return count;
}

ssize_t my_lseek(int fd, ssize_t offset, int whence) {
    if (fd < 0 || fd >= size_deque(deque)) {
        my_errno = incorrect_fd;
        return -1;
    }
    struct OpenedFile* file = get_elem_deque(deque, fd);
    ssize_t start = 0;
    if (whence == MY_SEEK_CUR) {
        start = file->file->offset;
    } else if (whence == MY_SEEK_END) {
        start = file->file->size;
    }
    start += offset;
    if (start < 0) {
        my_errno = before_file_start;
        return -1;
    }
    file->file->offset = start;
    return start;
}

struct MY_DIR {
    int fd;
    int index;
};

MY_DIR *my_opendir(const char* name) {
    int fd = my_open(name, MY_O_READ & MY_O_DIRECTORY);
    if (fd == -1) {
        return 0;
    }
    struct MY_DIR *dir = malloc(sizeof(*dir));
    dir->fd = fd;
    dir->index = 0;
    return dir;
}

int my_closedir(MY_DIR* dir) {
    if (my_close(dir->fd) == -1) {
        return -1;
    }
    free(dir);
    return 0;
}

struct my_dirent* my_readdir(MY_DIR* dir) {
    static struct my_dirent my_direct;
    if (!dir) {
        my_errno = no_file;
    }
    struct OpenedFile* file = get_elem_deque(deque, dir->fd);
    if (!file->perms & MY_O_READ) {
        my_errno = file_not_open_for_read;
        return 0;
    }
    struct File* leaf;
    int i = 0;
    int done = 0;
    list_for_each_entry(leaf, &file->file->leaf, sibling) {
        done = 0;
        if (i == dir->index) break;
        ++i;
        done = 1;
    }
    if (done) {
        return 0;
    }
    ++dir->index;
    my_direct.d_name = leaf->name;
    my_direct.d_type = leaf->isDirectory ? MY_DT_DIR : MY_DT_REG;
    return &my_direct;
}
