#ifndef FAT16
#define FAT16

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

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
    FILE* hdd;

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

struct File;

struct Device* openDevice(FILE* hdd);
void closeDevice(struct Device* device);
char* readDirectoryEntry(struct Device* device, struct PhysicalDirectoryEntry* entry);
struct File *physicalToFile(struct Device*, struct PhysicalDirectoryEntry *entry);
struct PhysicalDirectoryEntry *fileToPhysical(struct File*);
void updateSizeDirectoryEntry(struct Device* device, struct PhysicalDirectoryEntry* entry, size_t fileSize);
void updateDirectoryEntryData(struct Device* device, struct PhysicalDirectoryEntry *entry, char* mmap, size_t fileSize);
char* readRootDirectory(struct Device*);
void writeRootDirectory(struct Device*, char* mmap);

#endif
