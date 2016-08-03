#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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

struct Drive {
	char	manufacturerDescription[9];
	int		bytesPerBlock;
	int		blocksPerAllocationUnit;
	int		numberOfFat;
	int		numberOfRootEntries;
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

struct DirectoryEntry {
	char	filename[9];
	char	filenameExtension[4];
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
	int		size;

	uint8_t	*data;

	unsigned int position;

	struct PhysicalDirectoryEntry *physicalDirectoryEntry;
};

FILE *hdd = 0;

void readData(uint32_t position, void *ptr, uint32_t size) {
	fseek(hdd, position, SEEK_SET);
	fread(ptr, size, 1, hdd);
}

void writeData(uint32_t position, void *ptr, uint32_t size) {
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

struct DirectoryEntry *readEntry(struct Drive *drive, uint32_t position) {
	struct PhysicalDirectoryEntry *tmp = malloc(sizeof(*tmp));
	readData(position, tmp, sizeof(*tmp));
	struct DirectoryEntry *entry = malloc(sizeof(*entry));
	bzero(entry, sizeof(*entry));
	entry->position = position;
	entry->physicalDirectoryEntry = tmp;
	entry->filename[7] = '\0';
	entry->filenameExtension[3] = '\0';
	switch (tmp->filename[0]) {
		case 0x00:
		case 0xe5:
			copyStringFromDisk(entry->filename, tmp->filename + 1, sizeof(tmp->filename) - 1);
			break;
		case 0x05:
			copyStringFromDisk(entry->filename, tmp->filename, sizeof(tmp->filename));
			entry->filename[0] = 0xe5;
			break;
		case 0x2e:
			entry->isDirectory = 1;
			if (tmp->filename[1] == 0x2e)
				entry->isParentDirectory = 1;
			copyStringFromDisk(entry->filename, tmp->filename + 1, sizeof(tmp->filename) - 1);
			break;
		default:
			copyStringFromDisk(entry->filename, tmp->filename, sizeof(tmp->filename));
			break;
	}
	copyStringFromDisk(entry->filenameExtension, tmp->filenameExtension,
			sizeof(tmp->filenameExtension));
	if (tmp->fileAttributes & 0x1)
		entry->isReadOnly = 1;
	if (tmp->fileAttributes & 0x2)
		entry->isHidden = 1;
	if (tmp->fileAttributes & 0x4)
		entry->isSystemFile = 1;
	if (tmp->fileAttributes & 0x8)
		entry->isSpecialEntry = 1;
	if (tmp->fileAttributes & 0x10)
		entry->isDirectory = 1;
	if (tmp->fileAttributes & 0x20)
		entry->archive = 1;
	entry->hours = (tmp->timeCreatedUpdated & 0xF000) >> 11;
	entry->minutes = (tmp->timeCreatedUpdated & 0x7E0) >> 5;
	entry->seconds = (tmp->timeCreatedUpdated & 0x1F) * 2;
	entry->year = (tmp->dateCreatedUpdated & 0xFE00) >> 9;
	entry->month = (tmp->dateCreatedUpdated & 0x1E0) >> 5;
	entry->day = tmp->dateCreatedUpdated & 0x1F;
	entry->size = tmp->fileSize;
	if (entry->size <= 0)
		return entry;
	entry->data = malloc(entry->size);
	int currentCluster = tmp->startingCluster;
	int totalBytesRead = 0;
	do {
		int allocationUnitNumber = currentCluster + drive->dataStartBlock + 1;
		int bytesToRead = totalBytesRead + drive->bytesPerBlock >= entry->size ?
				entry->size - totalBytesRead : drive->bytesPerBlock;
		printf("reading from: %x\n", allocationUnitNumber * drive->bytesPerBlock);
		readData(allocationUnitNumber * drive->bytesPerBlock, &(entry->data[totalBytesRead]),
				bytesToRead);
		totalBytesRead += bytesToRead;
		currentCluster = drive->fat[currentCluster];
	} while (totalBytesRead < entry->size && currentCluster != 0xFFFF);
	return entry;
}

void closeEntry(struct DirectoryEntry *entry) {
	struct PhysicalDirectoryEntry *tmp = entry->physicalDirectoryEntry;
	entry->filename[0] = tmp->filename[0];
	if (strlen(entry->filename))
		copyStringToDisk(tmp->filename, entry->filename, sizeof(tmp->filename));
	if (strlen(entry->filename))
		copyStringToDisk(tmp->filenameExtension, entry->filenameExtension, sizeof(tmp->filenameExtension));
	if (entry->data)
		free(entry->data);
	writeData(entry->position, tmp, sizeof(*tmp));
	free(tmp);
	free(entry);
}

struct Drive *openDrive() {
	struct PhysicalBootBlock *tmp = malloc(sizeof(*tmp));
	readData(0, tmp, sizeof(*tmp));
	struct Drive *drive = malloc(sizeof(*drive));
	drive->physicalBootBlock = tmp;
	drive->manufacturerDescription[8] = '\0';
	drive->volumeLabel[11] = '\0';
	copyStringFromDisk(drive->manufacturerDescription, tmp->manufacturerDescription,
			sizeof(tmp->manufacturerDescription));
	copyStringFromDisk(drive->volumeLabel, tmp->volumeLabel, sizeof(tmp->volumeLabel));
	drive->bytesPerBlock = tmp->bytesPerBlock;
	drive->blocksPerAllocationUnit = tmp->blocksPerAllocationUnit;
	drive->numberOfFat = tmp->fatNumber;
	drive->numberOfRootEntries = tmp->rootDirectoryEntries;
	if (tmp->totalNumberOfBlocks == 0)
		drive->totalNumberOfBlocks = tmp->totalNumberOfBlocksBig;
	else
		drive->totalNumberOfBlocks = tmp->totalNumberOfBlocks;
	drive->sizeOfFat = tmp->blocksPerFat * drive->bytesPerBlock;
	drive->volumeSerialNumber = tmp->volumeSerialNumber;
	drive->rootDirectoryStartBlock = tmp->blocksPerFat * drive->numberOfFat + 1;
	drive->dataStartBlock = drive->rootDirectoryStartBlock +
		(drive->numberOfRootEntries * sizeof(struct PhysicalDirectoryEntry)) / drive->bytesPerBlock;
	printf("root: %x\n", drive->rootDirectoryStartBlock * drive->bytesPerBlock);
	printf("data: %x\n", drive->dataStartBlock * drive->bytesPerBlock);
	drive->fat = malloc(drive->sizeOfFat);
	readData(drive->bytesPerBlock, drive->fat, drive->sizeOfFat);

	return drive;
}

void closeDrive(struct Drive *drive) {
	struct PhysicalBootBlock *tmp = drive->physicalBootBlock;
	copyStringToDisk(tmp->volumeLabel, drive->volumeLabel, sizeof(tmp->volumeLabel));
	writeData(0, drive->physicalBootBlock, sizeof(*tmp));
	int i;
	for (i = 0; i < drive->numberOfFat; ++i)
		writeData(drive->bytesPerBlock + i * drive->sizeOfFat, drive->fat,
				drive->sizeOfFat);
	free(drive->physicalBootBlock);
	free(drive);
}

int main() {
	hdd = fopen("c.img", "rb+");

	struct Drive *drive = openDrive();

	int i;
	for (i = 0; i < drive->numberOfRootEntries; ++i) {
		struct DirectoryEntry *entry =
			readEntry(drive,
				i * sizeof(struct PhysicalDirectoryEntry) +
				drive->rootDirectoryStartBlock * drive->bytesPerBlock);
		if (strlen(entry->filename)) {
			printf("%s.%s\n", entry->filename, entry->filenameExtension);
			if (entry->size) {
				int j;
				for (j = 0; j < entry->size; ++j)
					printf("%c", entry->data[j]);
			}
		}
		closeEntry(entry);
	}

	closeDrive(drive);

	fclose(hdd);
	return 0;
}
