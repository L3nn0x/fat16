#include "file.h"
#include <stdlib.h>
#include <string.h>

static void cleanUpFile(struct File* file) {
    if (file->mmap) {
        free(file->mmap);
        file->mmap = 0;
    }
    free(file->entry);
    free(file->name);
    list_del(&file->leaf);
    list_del(&file->sibling);
    free(file);
}

void openFile(struct File* file) {
    if (file->mmap != 0 || !file->size) {
        return;
    }
    file->mmap = readDirectoryEntry(file->device, file->entry);
    file->offset = 0;
}

void closeFile(struct File* file, int flush) {
    if (file->mmap) {
        if (flush) {
            flushFile(file);
        }
        free(file->mmap);
        file->mmap = 0;
    }
}

void deleteFile(struct File* file) {
    if (file->isRoot) {
        return;
    }

    updateSizeDirectoryEntry(file->device, file->entry, 0);

    int close = 0;
    if (!file->parent->mmap) {
        openFile(file->parent);
        close = 1;
    }
    file->parent->size -= sizeof(struct PhysicalDirectoryEntry);
    memmove(&(file->parent->mmap[file->entryPosition * sizeof(struct PhysicalDirectoryEntry)]),
            &(file->parent->mmap[(1 + file->entryPosition) * sizeof(struct PhysicalDirectoryEntry)]),
            file->parent->size - file->entryPosition * sizeof(struct PhysicalDirectoryEntry));
    flushFile(file->parent);
    if (close) {
        closeFile(file->parent, 1);
    }

    cleanUpFile(file);
}

void flushFile(struct File* file) {
    if (file->isRoot) {
        writeRootDirectory(file->device, file->mmap);
        return;
    }
    if (file->mmap) {
        if (!file->size) {
            // error
            return;
        }
        updateDirectoryEntryData(file->device, file->entry, file->mmap, file->size);
    }

    struct PhysicalDirectoryEntry* entry = fileToPhysical(file);
    free(file->entry);
    file->entry = entry;

    int close = 0;
    if (!file->parent->mmap) {
        openFile(file->parent);
        close = 1;
    }
    memcpy(&(file->parent->mmap[file->entryPosition * sizeof(*entry)]), entry, sizeof(*entry));
    flushFile(file->parent);
    if (close) {
        closeFile(file->parent, 1);
    }
}

struct File* isCachedFile(struct File* dir, const char *filename) {
    struct File *leaf;
    list_for_each_entry(leaf, &dir->leaf, sibling) {
        if (!strcmp(leaf->name, filename)) {
            return leaf;
        }
    }
    return 0;
}

void getDirEntries(struct File* dir) {
    if (!dir->isDirectory) {
        return;
    }
    int offload = 0;
    if (!dir->mmap) {
        offload = 1;
        openFile(dir);
    }

    for (int i = 0; i < dir->size / sizeof(struct PhysicalDirectoryEntry); ++i) {
        struct PhysicalDirectoryEntry *entry = malloc(sizeof(*entry));
        memcpy(entry, &(dir->mmap[i * sizeof(*entry)]), sizeof(*entry));
        struct File * file = physicalToFile(dir->device, entry);
        if (!file) {
            break;
        }
        file->entryPosition = i;
        if (!isCachedFile(dir, file->name)) {
            file->parent = dir;
            list_add(&file->sibling, &dir->leaf);
        } else {
            cleanUpFile(file);
        }
    }

    if (offload) {
        free(dir->mmap);
    }
}

struct File* openRoot(struct Device* device) {
    struct File* file = malloc(sizeof(*file));
    bzero(file, sizeof(*file));
    file->device = device;
    file->size = device->totalNumberOfRootEntries * sizeof(struct PhysicalDirectoryEntry);
    file->name = strdup("/");
    file->isRoot = 1;
    file->isDirectory = 1;
    file->parent = file;
    file->mmap = readRootDirectory(device);
    INIT_LIST_HEAD(&file->leaf);
    getDirEntries(file);
    INIT_LIST_HEAD(&file->sibling);
    return file;
}

struct File* pathToFile(struct File* root, const char* path) {
    char* tmp = strdup(path);
    for (char *next = strtok(tmp, "/"); next; next = strtok(0, "/")) {
        if (!root->isDirectory) {
            free(tmp);
            return 0;
        }
        if (!strcmp("..", next)) {
            root = root->parent;
        } else if (!strcmp(".", next)) {
            // nop
        } else {
            getDirEntries(root);
            if (!(root = isCachedFile(root, next))) {
                free(tmp);
                return 0;
            }
        }
    }
    return root;
}

char *fileToPath(struct File* file) {
    char res[512] = {0};

    char *tmp;
    if (!file->isRoot) {
       tmp = fileToPath(file->parent); 
       strcpy(res, tmp);
       strcpy(res + strlen(tmp), file->name);
       res[strlen(tmp) + strlen(file->name)] = '/';
    } else {
        return strdup(file->name);
    }

    return strdup(res);
}

struct File* addFile(struct File* root, const char *path, int isDir) {
    char* res = strrchr(path, '/');
    char parent[512];
    char name[512];
    if (!res) {
        parent[0] = '.';
        parent[1] = '\0';
        strcpy(name, path);
    } else {
        memcpy(parent, path, res - path);
        parent[res - path] = '\0';
        strncpy(name, path, (int)(res - path));
    }
    root = pathToFile(root, parent);
    struct PhysicalDirectoryEntry* entry = malloc(sizeof(*entry));
    bzero(entry, sizeof(*entry));
    struct File* file = malloc(sizeof(*file));
    bzero(file, sizeof(*file));
    file->entry = entry;
    file->device = root->device;
    file->name = strdup(name);
    file->isDirectory = isDir;

    getDirEntries(root);

    struct list_head* leaf;
    int index = 0;
    list_for_each(leaf, &root->leaf) {
        ++index;
    }
    file->entryPosition = index;

    entry->startingCluster = 0;
    file->parent = root;
    INIT_LIST_HEAD(&file->leaf);
    list_add(&file->sibling, &root->leaf);

    // update parent mmap
    int close = 0;
    if (!root->mmap) {
        openFile(root);
        close = 1;
    }
    root->size += sizeof(*entry);
    root->mmap = realloc(root->mmap, root->size);
    flushFile(file);
    if (close) {
        closeFile(root, 1);
    }

    return file;
}
