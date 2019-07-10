#ifndef FILE_H
#define FILE_H

#include "list.h"
#include "fat16.h"
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

struct File {
    struct PhysicalDirectoryEntry *entry;
    struct Device *device;
    char *name;
	short	isReadOnly;
	short	isHidden;
	short	isSystemFile;
	short	isSpecialEntry;
	short	isDirectory;
	short	isParentDirectory;
	short	archive;
	int		hours;
	int		minutes;
	int		seconds;
	int		year;
	int		month;
	int		day;

    int size;
    int entryPosition;
    int isRoot;

    char *mmap;
    size_t offset;

    struct File* parent;
    struct list_head leaf;
    struct list_head sibling;
};

void closeFile(struct File*, int flush);
void deleteFile(struct File* file);
void openFile(struct File* file);
void flushFile(struct File* file);
struct File* isCachedFile(struct File* dir, const char *filename);
void getDirEntries(struct File* dir);
struct File* openRoot(struct Device* device);
struct File* pathToFile(struct File* root, const char* path);
char *fileToPath(struct File* file);
struct File* addFile(struct File* root, const char *path, int isDir);

#endif
