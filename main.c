#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "list.h"

struct PhysicalBootBlock {
	uint8_t		bootstrap[3];
	uint8_t		manufacturerDescription[8];
	uint16_t	bytesPerBlock;
	uint8_t		blocksPerAllocationUnit;
	uint16_t	reservedBlocks;
	uint8_t		fatNumber;
	uint16_t	rootDirectoryEntries;
	uint16_t	totalNumberOfBlocks;
	uint8_t		mediaDescriptor;
	uint16_t	blocksPerFat;
	uint16_t	blocksPerTrack;
	uint16_t	numberOfHeads;
	uint32_t	hiddenBlocks;
	uint32_t	totalNumberOfBlocksBig;
	uint16_t	physicalDriveNumber;
	uint8_t		extendedBootRecordSignature;
	uint32_t	volumeSerialNumber;
	uint8_t		volumeLabel[11];
	uint8_t		fileSystemIdentifier[8];
} __attribute__((packed));

struct Device {
	char	manufacturerDescription[9];
	int		bytesPerBlock;
	int		blocksPerAllocationUnit;
	int		numberOfFat;
	int		totalNumberOfRootEntries;
	int		totalNumberOfBlocks;
	int		sizeOfFat;
	int		volumeSerialNumber;
	char	volumeLabel[12];
	int		rootDirectoryStartBlock;
	int		dataStartBlock;

	uint16_t *fat;

	struct PhysicalBootBlock *physicalBootBlock;
};

struct PhysicalDirectoryEntry {
	uint8_t		filename[8];
	uint8_t		filenameExtension[3];
	uint8_t		fileAttributes;
	uint8_t		reserved[10];
	uint16_t	timeCreatedUpdated;
	uint16_t	dateCreatedUpdated;
	uint16_t	startingCluster;
	uint32_t	fileSize;
} __attribute__((packed));

FILE *hdd = 0;

void driverRead(uint32_t position, void *ptr, uint32_t size) {
	fseek(hdd, position, SEEK_SET);
	fread(ptr, size, 1, hdd);
}

void driverWrite(uint32_t position, void *ptr, uint32_t size) {
	fseek(hdd, position, SEEK_SET);
	fwrite(ptr, size, 1, hdd);
}

void copyStringFromDisk(char dest[], uint8_t src[], uint32_t size) {
	memcpy(dest, src, size);
	int i = size - 1;
	while (i >= 0 && dest[i] == ' ')
		dest[i--] = '\0';
}

void copyStringToDisk(uint8_t dest[], char src[], uint32_t size) {
	memcpy(dest, src, size);
	int i = size - 1;
	while (i >= 0 && dest[i] == '\0')
		dest[i--] = ' ';
}

struct Device* openDevice() {
    struct PhysicalBootBlock* tmp = malloc(sizeof(*tmp));
    driverRead(0, tmp, sizeof(*tmp));
    struct Device* device = malloc(sizeof(*device));
    device->physicalBootBlock = tmp;
    copyStringFromDisk(device->manufacturerDescription, tmp->manufacturerDescription, sizeof(tmp->manufacturerDescription));
    copyStringFromDisk(device->volumeLabel, tmp->volumeLabel, sizeof(tmp->volumeLabel));
    device->bytesPerBlock = tmp->bytesPerBlock;
    device->blocksPerAllocationUnit = tmp->blocksPerAllocationUnit;
    device->numberOfFat = tmp->fatNumber;
    device->totalNumberOfRootEntries = tmp->rootDirectoryEntries;
    if (tmp->totalNumberOfBlocks == 0) {
        device->totalNumberOfBlocks = tmp->totalNumberOfBlocksBig;
    } else {
        device->totalNumberOfBlocks = tmp->totalNumberOfBlocks;
    }
    device->sizeOfFat = tmp->blocksPerFat * device->bytesPerBlock;
    device->volumeSerialNumber = tmp->volumeSerialNumber;
    device->rootDirectoryStartBlock = tmp->blocksPerFat * device->numberOfFat + 1;
    device->dataStartBlock = device->rootDirectoryStartBlock +
        (device->totalNumberOfRootEntries * sizeof(struct PhysicalDirectoryEntry)) / device->bytesPerBlock;
    device->fat = malloc(device->sizeOfFat);
    driverRead(device->bytesPerBlock, device->fat, device->sizeOfFat);
    return device;
}

void flushDevice(struct Device* device) {
    struct PhysicalBootBlock *tmp = device->physicalBootBlock;
	copyStringToDisk(tmp->volumeLabel, device->volumeLabel, sizeof(tmp->volumeLabel));
	driverWrite(0, tmp, sizeof(*tmp));
	for (int i = 0; i < device->numberOfFat; ++i) {
		driverWrite(device->bytesPerBlock + i * device->sizeOfFat, device->fat,
				device->sizeOfFat);
    }
}

void closeDevice(struct Device* device) {
    free(device->physicalBootBlock);
    free(device->fat);
    free(device);
}

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
    char *mmap;
    int size;
    int entryPosition;
    int startingDataCluster;
    int isRoot;

    struct File* parent;
    struct list_head leaf;
    struct list_head sibling;
};

struct File *physicalToFile(struct PhysicalDirectoryEntry *entry, struct Device* device, uint32_t position) {
    struct File* file = malloc(sizeof(*file));
    file->entry = entry;
    file->device = device;
    file->entryPosition = position;
    file->isRoot = 0;
    char filename[9] = {0};
    char ext[4] = {0};
	switch (entry->filename[0]) {
		case 0x00:
		case 0xe5:
            free(file);
            return 0;
		case 0x05:
			copyStringFromDisk(filename, entry->filename, sizeof(entry->filename));
			filename[0] = 0xe5;
			break;
		case 0x2e:
			file->isDirectory = 1;
			if (entry->filename[1] == 0x2e)
				file->isParentDirectory = 1;
			copyStringFromDisk(filename, entry->filename + 1, sizeof(entry->filename) - 1);
			break;
		default:
			copyStringFromDisk(filename, entry->filename, sizeof(entry->filename));
			break;
	}
    copyStringFromDisk(ext, entry->filenameExtension, sizeof(entry->filenameExtension));
    file->name = malloc(strlen(filename) + strlen(ext) + 1);
    if (strlen(ext)) {
        sprintf(file->name, "%s.%s", filename, ext);
    } else {
        strcpy(file->name, filename);
    }
	if (entry->fileAttributes & 0x1)
		file->isReadOnly = 1;
	if (entry->fileAttributes & 0x2)
		file->isHidden = 1;
	if (entry->fileAttributes & 0x4)
		file->isSystemFile = 1;
	if (entry->fileAttributes & 0x8)
		file->isSpecialEntry = 1;
	if (entry->fileAttributes & 0x10)
		file->isDirectory = 1;
	if (entry->fileAttributes & 0x20)
		file->archive = 1;
	file->hours = (entry->timeCreatedUpdated & 0xF000) >> 11;
	file->minutes = (entry->timeCreatedUpdated & 0x7E0) >> 5;
	file->seconds = (entry->timeCreatedUpdated & 0x1F) * 2;
	file->year = (entry->dateCreatedUpdated & 0xFE00) >> 9;
	file->month = (entry->dateCreatedUpdated & 0x1E0) >> 5;
	file->day = entry->dateCreatedUpdated & 0x1F;
	file->size = entry->fileSize;
    file->mmap = 0;
    file->startingDataCluster = entry->startingCluster;

    file->parent = 0;
    INIT_LIST_HEAD(&file->leaf);
    INIT_LIST_HEAD(&file->sibling);
    return file;
}

void readFile(struct File* file) {
    if (file->mmap != 0 || !file->size) {
        return;
    }
    file->mmap = malloc(file->size);
    int currentCluster = file->startingDataCluster;
    int totalBytesRead = 0;
    do {
        int blocks = (currentCluster - 2) * file->device->blocksPerAllocationUnit + file->device->dataStartBlock;
        int bytesToRead = totalBytesRead + file->device->blocksPerAllocationUnit * file->device->bytesPerBlock >= file->size ?
            file->size - totalBytesRead : file->device->blocksPerAllocationUnit * file->device->bytesPerBlock;
        driverRead(blocks * file->device->bytesPerBlock, &(file->mmap[totalBytesRead]),
                bytesToRead);
        totalBytesRead += bytesToRead;
        currentCluster = file->device->fat[currentCluster];
    } while (totalBytesRead < file->size && currentCluster != 0xFFFF);
}

void cleanUpFile(struct File* file) {
    free(file->entry);
    free(file->name);
    if (file->mmap) {
        free(file->mmap);
        file->mmap = 0;
    }
    list_del(&file->leaf);
    list_del(&file->sibling);
    free(file);
}

void openFile(struct File* file) {
    readFile(file);
}

void flushFile(struct File* file);

void closeFile(struct File* file) {
    if (file->mmap) {
	flushFile(file);
        free(file->mmap);
        file->mmap = 0;
    }
}

void deleteFile(struct File* file) {
    if (file->isRoot) {
        return;
    }
    struct PhysicalDirectoryEntry entry;
    bzero(&entry, sizeof(entry));

    uint16_t *fat = file->device->fat;

    int allocationUnits = file->size / (file->device->blocksPerAllocationUnit * file->device->bytesPerBlock)
        + (file->size % (file->device->blocksPerAllocationUnit * file->device->bytesPerBlock) > 0);
    while (allocationUnits--) {
        int current = file->startingDataCluster;
        int tmp = allocationUnits;
        while (tmp-- > 0) current = fat[current];
        fat[current] = 0;
    }

    int close = 0;
    if (!file->parent->mmap) {
        openFile(file->parent);
        close = 1;
    }
    file->parent->size -= sizeof(entry);
    memmove(&(file->parent->mmap[file->entryPosition * sizeof(entry)]),
            &(file->parent->mmap[(1 + file->entryPosition) * sizeof(entry)]),
            file->parent->size - file->entryPosition * sizeof(entry));
    flushFile(file->parent);
    if (close) {
        closeFile(file->parent);
    }

    flushDevice(file->device);
    cleanUpFile(file);
}

void flushFile(struct File* file) {
    if (file->isRoot) {
        // only copy mmap
        driverWrite(file->device->rootDirectoryStartBlock * file->device->bytesPerBlock, file->mmap, file->size);
        return;
    }
    struct PhysicalDirectoryEntry *entry = file->entry;

    // if there is a mmap, update fat + data blocks
    if (file->mmap) {
        if (!file->size) {
            return;
        }
        int oldAllocationUnits = entry->fileSize / (file->device->blocksPerAllocationUnit * file->device->bytesPerBlock)
            + (entry->fileSize % (file->device->blocksPerAllocationUnit * file->device->bytesPerBlock) > 0);
        entry->fileSize = file->size;
        int allocationUnits = file->size / (file->device->blocksPerAllocationUnit * file->device->bytesPerBlock)
            + (file->size % (file->device->blocksPerAllocationUnit * file->device->bytesPerBlock) > 0);
        uint16_t *fat = file->device->fat;
        if (allocationUnits > oldAllocationUnits) {
            int diff = allocationUnits - oldAllocationUnits;
            int current = file->startingDataCluster;
		    if (current) {
                while (fat[current] != 0xFFFF) current = fat[current];
            } else {
                --diff;
                int nextFree = 3;
                while (fat[nextFree++] != 0);
                current = nextFree - 1;
                fat[current] = 0xFFFF;
                file->startingDataCluster = current;
                file->startingCluster = current;
            }
            int nextFree = 3;
            while (diff) {
                if (fat[nextFree] == 0) {
                    fat[nextFree] = 0xFFFF;
                    fat[current] = nextFree;
                    current = nextFree;
                    --diff;
                }
                nextFree++;
            }
        } else if (allocationUnits < oldAllocationUnits) {
            int diff = oldAllocationUnits - allocationUnits;
            while (diff--) {
                int current = file->startingDataCluster;
                int tmp = --allocationUnits;
                while (tmp-- > 0) current = fat[current];
                fat[current] = 0;
            }
        }
        int currentCluster = file->startingDataCluster;
        int totalBytesWritten = 0;
        do {
            int blocks = (currentCluster - 2) * file->device->blocksPerAllocationUnit + file->device->dataStartBlock;
            int bytesToWrite = totalBytesWritten + file->device->blocksPerAllocationUnit * file->device->bytesPerBlock >= file->size ?
                file->size - totalBytesWritten : file->device->blocksPerAllocationUnit * file->device->bytesPerBlock;
            driverWrite(blocks * file->device->bytesPerBlock, &(file->mmap[totalBytesWritten]), bytesToWrite);
            totalBytesWritten += bytesToWrite;
            currentCluster = fat[currentCluster];
        } while (totalBytesWritten < file->size && currentCluster != 0xFFFF);
    }

    char filename[8];
    memset(filename, ' ', sizeof(filename));
    char ext[3];
    memset(ext, ' ', sizeof(ext));
    char* tmp = strchr(file->name, '.');
    if (tmp) {
        memcpy(ext, tmp + 1, strlen(tmp+1) > sizeof(ext) ? sizeof(ext) : strlen(tmp + 1));
        memcpy(filename, file->name, tmp - file->name);
    } else {
        memcpy(filename, file->name, strlen(file->name) > sizeof(filename) ? sizeof(filename) : strlen(file->name));
    }

    if (filename[0] == 0xe5) {
        filename[0] = 0x05;
    }
    if (file->isDirectory) {
        int m = 1;
        if (file->isParentDirectory) {
            m = 2;
        }
        memmove(filename + m, filename, sizeof(filename) - m);
        filename[0] = 0x2e;
        if (m == 2) {
            filename[1] = 0x2e;
        }
    }
    uint8_t attrs = 0;
    if (file->isReadOnly) attrs |= 0x1;
    if (file->isHidden) attrs |= 0x2;
    if (file->isSystemFile) attrs |= 0x4;
    if (file->isSpecialEntry) attrs |= 0x8;
    if (file->isDirectory) attrs |= 0x10;
    if (file->archive) attrs |= 0x20;
    uint16_t time = file->hours << 11 | file->minutes << 5 | file->seconds / 2;
    uint16_t date = file->year << 9 | file->month << 5 | file->day;

    memcpy(entry->filename, filename, sizeof(filename));
    memcpy(entry->filenameExtension, ext, sizeof(ext));
    entry->fileAttributes = attrs;
    entry->timeCreatedUpdated = time;
    entry->dateCreatedUpdated = date;

    int close = 0;
    if (!file->parent->mmap) {
        openFile(file->parent);
        close = 1;
    }
    memcpy(&(file->parent->mmap[file->entryPosition * sizeof(*entry)]), entry, sizeof(*entry));
    flushFile(file->parent);
    if (close) {
        closeFile(file->parent);
    }

    flushDevice(file->device);
}

struct File* isCachedFile(struct File* dir, char *filename) {
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
        readFile(dir);
    }

    for (int i = 0; i < dir->size / sizeof(struct PhysicalDirectoryEntry); ++i) {
        struct PhysicalDirectoryEntry *entry = malloc(sizeof(*entry));
        memcpy(entry, &(dir->mmap[i * sizeof(*entry)]), sizeof(*entry));
        struct File * file = physicalToFile(entry, dir->device, i);
        if (!file) {
            break;
        }
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
    file->mmap = malloc(file->size);
    driverRead(device->rootDirectoryStartBlock * device->bytesPerBlock, file->mmap, file->size);
    INIT_LIST_HEAD(&file->leaf);
    getDirEntries(file);
    INIT_LIST_HEAD(&file->sibling);
    return file;
}

struct File* pathToFile(struct File* root, char* path) {
    char* tmp = strdup(path);
    char* next = strtok(tmp, "/");
    while (next) {
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
        next = strtok(0, "/");
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

struct File* addFile(struct File* root, char *path, int isDir) {
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

    file->startingDataCluster = entry->startingCluster = 0;
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
        closeFile(root);
    }

    return file;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Error, missing argument. Usage %s <filename>\n", argv[0]);
        return 1;
    }
    hdd = fopen(argv[1], "rb+");

    struct Device *device = openDevice();

    struct File* root = openRoot(device);
    struct File* current = root;

    while (1) {
        char *path = fileToPath(current);

        printf("%s > ", path);
        char input[256];
        if (!fgets(input, sizeof(input), stdin)) {
            printf("\n");
            break;
        }
        input[strlen(input) - 1] = '\0';
        for (int i = 0; input[i]; ++i) {
            input[i] = toupper(input[i]);
        }
        char* split = strtok(input, " ");
        if (!split) {
            continue;
        }
        if (!strcmp(split, "LS")) {
            struct File* leaf;
            struct File* c = current;
            char *filename = strtok(0, " ");
            if (filename) {
                c = pathToFile(current, filename);
                if (!c) {
                    printf("ls: %s: no such file or directory\n", filename);
                    continue;
                }
                getDirEntries(c);
            }
            list_for_each_entry(leaf, &c->leaf, sibling) {
                printf("%s (%c) (%d)\n", leaf->name, leaf->isDirectory ? 'd' : 'f', leaf->size);
            }
        } else if (!strcmp(split, "EXIT")) {
            break;
        } else if (!strcmp(split, "CAT")) {
            char *filename = strtok(0, " ");
            if (!filename) {
                printf("cat: usage cat <filename>\n");
                continue;
            }
            struct File* leaf = pathToFile(current, filename);
            if (!leaf) {
                printf("cat: %s: not such file or directory\n", filename);
            } else {
                openFile(leaf);
                for (int i = 0; i < leaf->size; ++i) {
                    printf("%c", leaf->mmap[i]);
                }
                closeFile(leaf);
                printf("$\n");
            }
        } else if (!strcmp(split, "EDIT")) {
            char *filename = strtok(0, " ");
            if (!filename) {
                printf("edit: usage edit <filename> <text>\n");
                continue;
            }
            char *text = strtok(0, " ");
            if (!text) {
                printf("edit: usage edit <filename> <text>\n");
                continue;
            }
            struct File* leaf = pathToFile(current, filename);
            if (!leaf) {
                printf("edit: %s: not such file or directory\n", filename);
            } else {
                openFile(leaf);
                int oldSize = leaf->size;
                leaf->size += strlen(text);
                leaf->mmap = realloc(leaf->mmap, leaf->size);
                strcpy(&(leaf->mmap[oldSize]), text);
                closeFile(leaf);
            }
        } else if (!strcmp(split, "FILL")) {
            char *filename = strtok(0, " ");
            if (!filename) {
                printf("fill: usage fill <filename> <size> <char>\n");
                continue;
            }
            char *text = strtok(0, " ");
            if (!text) {
                printf("fill: usage fill <filename> <size> <char>\n");
                continue;
            }
            char *c = strtok(0, " ");
            if (!c) {
                printf("fill: usage fill <filename> <size> <char>\n");
                continue;
            }
            struct File* leaf = pathToFile(current, filename);
            if (!leaf) {
                printf("fill: %s: not such file or directory\n", filename);
            } else {
                int size = atoi(text);
                openFile(leaf);
                int oldSize = leaf->size;
                leaf->size += size;
                leaf->mmap = realloc(leaf->mmap, leaf->size);
                memset(&(leaf->mmap[oldSize]), c[0], size);
                closeFile(leaf);
            }
        } else if (!strcmp(split, "RM")) {
            char *filename = strtok(0, " ");
            if (!filename) {
                printf("rm: usage rm <filename>\n");
                continue;
            }
            struct File* leaf = pathToFile(current, filename);
            if (!leaf) {
                printf("delete: %s: not such file or directory\n", filename);
            } else {
                openFile(leaf);
                deleteFile(leaf);
            }
        } else if (!strcmp(split, "TOUCH")) {
            char *filename = strtok(0, " ");
            if (!filename) {
                printf("touch: usage touch <filename>\n");
                continue;
            }
            addFile(current, filename, 0);
        } else if (!strcmp(split, "MKDIR")) {
            char *filename = strtok(0, " ");
            if (!filename) {
                printf("mkdir: usage mkdir <filename>\n");
                continue;
            }
            addFile(current, filename, 1);
        } else if (!strcmp(split, "CD")) {
            char *filename = strtok(0, " ");
            if (!filename) {
                printf("cd: usage cd <filename>\n");
                continue;
            }
            current = pathToFile(current, filename);
            getDirEntries(current);
        } else {
            printf("Unrecognized command\n");
        }
        free(path);
    }

    closeFile(root);

    flushDevice(device);
	closeDevice(device);

	fclose(hdd);
	return 0;
}
