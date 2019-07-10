#include "fat16.h"
#include "file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void driverRead(struct Device* device, uint32_t position, void *ptr, uint32_t size) {
	fseek(device->hdd, position, SEEK_SET);
	fread(ptr, size, 1, device->hdd);
}

static void driverWrite(struct Device* device, uint32_t position, void *ptr, uint32_t size) {
	fseek(device->hdd, position, SEEK_SET);
	fwrite(ptr, size, 1, device->hdd);
}

static void copyStringFromDisk(char dest[], uint8_t src[], uint32_t size) {
	memcpy(dest, src, size);
	int i = size - 1;
	while (i >= 0 && dest[i] == ' ')
		dest[i--] = '\0';
}

static void copyStringToDisk(uint8_t dest[], char src[], uint32_t size) {
	memcpy(dest, src, size);
	int i = size - 1;
	while (i >= 0 && dest[i] == '\0')
		dest[i--] = ' ';
}

struct Device* openDevice(FILE* hdd) {
    struct PhysicalBootBlock* tmp = malloc(sizeof(*tmp));
    struct Device* device = malloc(sizeof(*device));
    device->hdd = hdd;
    driverRead(device, 0, tmp, sizeof(*tmp));
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
    driverRead(device, device->bytesPerBlock, device->fat, device->sizeOfFat);
    return device;
}

static void flushDevice(struct Device* device) {
    struct PhysicalBootBlock *tmp = device->physicalBootBlock;
	copyStringToDisk(tmp->volumeLabel, device->volumeLabel, sizeof(tmp->volumeLabel));
	driverWrite(device, 0, tmp, sizeof(*tmp));
	for (int i = 0; i < device->numberOfFat; ++i) {
		driverWrite(device, device->bytesPerBlock + i * device->sizeOfFat, device->fat,
				device->sizeOfFat);
    }
}

void closeDevice(struct Device* device) {
    flushDevice(device);
    free(device->physicalBootBlock);
    free(device->fat);
    free(device);
}

char* readDirectoryEntry(struct Device* device, struct PhysicalDirectoryEntry* entry) {
    char* mmap = malloc(entry->fileSize);
    int currentCluster = entry->startingCluster;
    int totalBytesRead = 0;
    do {
        int blocks = (currentCluster - 2) * device->blocksPerAllocationUnit + device->dataStartBlock;
        int bytesToRead = (totalBytesRead + device->blocksPerAllocationUnit * device->bytesPerBlock)
            >= entry->fileSize ?
                entry->fileSize - totalBytesRead :
                device->blocksPerAllocationUnit * device->bytesPerBlock;
        driverRead(device, blocks * device->bytesPerBlock, &(mmap[totalBytesRead]), bytesToRead);
        totalBytesRead += bytesToRead;
        currentCluster = device->fat[currentCluster];
    } while (totalBytesRead < entry->fileSize && currentCluster != 0xFFFF);
    return mmap;
}

struct File *physicalToFile(struct Device* device, struct PhysicalDirectoryEntry *entry) {
    struct File* file = malloc(sizeof(*file));
    file->entry = entry;
    file->device = device;
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

    file->parent = 0;
    INIT_LIST_HEAD(&file->leaf);
    INIT_LIST_HEAD(&file->sibling);
    return file;
}

struct PhysicalDirectoryEntry *fileToPhysical(struct File* file) {
    struct PhysicalDirectoryEntry* entry = malloc(sizeof(*entry));
    memcpy(entry, file->entry, sizeof(*entry));
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
    return entry;
}

void updateSizeDirectoryEntry(struct Device* device, struct PhysicalDirectoryEntry* entry, size_t fileSize) {
    int oldAllocationUnits = entry->fileSize / (device->blocksPerAllocationUnit * device->bytesPerBlock)
        + (entry->fileSize % (device->blocksPerAllocationUnit * device->bytesPerBlock) > 0);
    int allocationUnits = fileSize / (device->blocksPerAllocationUnit * device->bytesPerBlock)
        + (fileSize % (device->blocksPerAllocationUnit * device->bytesPerBlock) > 0);

    entry->fileSize = fileSize;

    uint16_t *fat = device->fat;
    if (allocationUnits > oldAllocationUnits) {
        int diff = allocationUnits - oldAllocationUnits;
        int current = entry->startingCluster;
        if (current) {
            while (fat[current] != 0xFFFF) current = fat[current];
        } else {
            --diff;
            int nextFree = 3;
            while (fat[nextFree++] != 0);
            current = nextFree - 1;
            fat[current] = 0xFFFF;
            entry->startingCluster = current;
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
        flushDevice(device);
    } else if (allocationUnits < oldAllocationUnits) {
        int diff = oldAllocationUnits - allocationUnits;
        while (diff--) {
            int current = entry->startingCluster;
            int tmp = --allocationUnits;
            while (tmp-- > 0) current = fat[current];
            fat[current] = 0;
        }
        flushDevice(device);
    }
}

void updateDirectoryEntryData(struct Device* device, struct PhysicalDirectoryEntry *entry, char* mmap, size_t fileSize) {
    if (entry->fileSize != fileSize) {
        updateSizeDirectoryEntry(device, entry, fileSize);
    }
    int currentCluster = entry->startingCluster;
    int totalBytesWritten = 0;
    do {
        int blocks = (currentCluster - 2) * device->blocksPerAllocationUnit + device->dataStartBlock;
        int bytesToWrite = totalBytesWritten + device->blocksPerAllocationUnit * device->bytesPerBlock >= fileSize ?
            fileSize - totalBytesWritten : device->blocksPerAllocationUnit * device->bytesPerBlock;
        driverWrite(device, blocks * device->bytesPerBlock, &(mmap[totalBytesWritten]), bytesToWrite);
        totalBytesWritten += bytesToWrite;
        currentCluster = device->fat[currentCluster];
    } while (totalBytesWritten < fileSize && currentCluster != 0xFFFF);
}

char* readRootDirectory(struct Device* device) {
    const int size = device->totalNumberOfRootEntries * sizeof(struct PhysicalDirectoryEntry);
    char* mmap = malloc(size);
    driverRead(device, device->rootDirectoryStartBlock * device->bytesPerBlock, mmap, size);
    return mmap;
}

void writeRootDirectory(struct Device* device, char* mmap) {
    const int size = device->totalNumberOfRootEntries * sizeof(struct PhysicalDirectoryEntry);
    driverWrite(device, device->rootDirectoryStartBlock * device->bytesPerBlock, mmap, size);
}
