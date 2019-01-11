#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include "SubParser.h"

#define MAX_FILE_SIZE        4096               /* File Size */
#define DATA_BLOCK_SIZE      100                /* Size of Single Block */
#define INDEX_BLOCK_SIZE     50                 /* Size of Single Index (Must Be Greater Then METADATA) */
#define FILENAME             "File System.txt"  /* Name of The File System */
#define NULLCHAR             '\0'               /* I Fill The File System With '\0' */
#define FILE_DATA_NULLCHARS  "\0\0\0\0\0\0\0\0" /* Must be Same Size/Length of FILE_DATA */
#define TYPE_FILE            0                  /* To Indicate It's A File */
#define TYPE_DIRECTORY       1                  /* To Indicate It's A Directory */
#define METADATA             36                 /* The Struct MetaData Holds A Total of 36 Bits */
#define FILE_INDEX_SIZE      12                 /* The Struct FileIndex Holds A Total of 12 Bits */
#define FILE_DATA            8                  /* The Struct FileData Holds A Total of 8 Bits */
#define DIRECTORY_INDEX_SIZE 12                 /* The Struct DirectoryIndex Holds A Total of 12 Bits */
#define EMPTY                -1

// Globel Variables
char*  UNUSED_INDEX                     = NULL; /* Used To Find and Allocate Unused Indices */
struct IndexBlocks* indexBlocksInMemory = NULL; /* MMAP'ed All Index Blocks */
struct DataBlocks* dataBlocksInMemory   = NULL; /* MMAP'ed All Data Blocks */
int    totalBlocks                      = 0   ;

struct MetaData
{
    int   type;
    char* name;
    char* timeCreated;
    int   pointer;
    int   offset;
    int   index;
};

struct FileData
{
    char* data;
};

struct FileIndex
{
    char* name;
    int   indexNumber;
};

struct DirectoryIndex
{
    char* name;
    int   indexNumber;
};

struct IndexBlocks /* To Load MMAP */
{
    char index[INDEX_BLOCK_SIZE];
};

struct DataBlocks /* To Load MMAP */
{
    char block[DATA_BLOCK_SIZE];
};

int findAndSetIndex(char* directoryName, int type)
{
    for (int i = 0; i < totalBlocks; ++i)
    {
        if (memcmp(UNUSED_INDEX, indexBlocksInMemory[i].index, INDEX_BLOCK_SIZE) == 0)
        {
            // Time
            char   timeCreated[10];
            time_t rawtime;
            struct tm* timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            sprintf(timeCreated, "%02d:%02d:%02d", timeinfo->tm_hour + 3, timeinfo->tm_min, timeinfo->tm_sec);

            struct MetaData* dir = (struct MetaData *)malloc(sizeof(struct MetaData));
            dir->type            = type;
            dir->name            = directoryName;
            dir->timeCreated     = timeCreated;
            dir->pointer         = EMPTY;
            dir->offset          = 0;
            dir->index           = i;
            memcpy(indexBlocksInMemory[i].index, dir, METADATA);

            return i;
        }
    }

    // Index is Full
    fprintf(stderr, "Error: The Filesystem Has Reached its Capacity\n");
    exit(EXIT_FAILURE);
    //return -1;
}

void createFile(char* filePath)
{
    int    j             = 0;
    int    count         = 0;
    int    dependent     = 1;
    char** parsed        = parseString(filePath, &count, '/');
    struct MetaData* dir = NULL;

    for (int i = 1; i < count; ++i)
    {
        for ( ; j < totalBlocks; ++j)
        {
            if (dependent == totalBlocks)
            {
                break;
            }

            dir = (struct MetaData *)malloc(sizeof(struct MetaData));
            memcpy(dir, indexBlocksInMemory[j].index, METADATA);

            if (dir->type == TYPE_DIRECTORY && !strcmp(dir->name, parsed[i]))
            {
                j++;
                break;
            }
            else
            {
                dependent++;
            }
        }
        if (dependent == totalBlocks)
        {
            fprintf(stderr, "Error: Directory Does Not Exist\n");
            break;
        }
    }

    if (dependent != totalBlocks)
    {
        struct FileIndex* file    = (struct FileIndex *)malloc(sizeof(struct FileIndex));
        file->name                = parsed[count];
        file->indexNumber         = findAndSetIndex(parsed[count], TYPE_FILE);

        struct FileData* fileData = (struct FileData *)malloc(sizeof(struct FileData));
        fileData->data = "NULL";

        memcpy(dataBlocksInMemory[file->indexNumber].block, fileData, FILE_DATA);

        memcpy(dataBlocksInMemory[j - 1].block + dir->offset, file, FILE_INDEX_SIZE);

        // Update Offset
        dir->offset += FILE_INDEX_SIZE;
        memcpy(indexBlocksInMemory[j - 1].index, dir, METADATA);
    }
}

void createDir(char* directoryIndex)
{
    int    count  = 0;
    char** parsed = parseString(directoryIndex, &count, '/');

    char timeCreated[10];

    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    sprintf(timeCreated, "%02d:%02d:%02d", timeinfo->tm_hour + 3, timeinfo->tm_min, timeinfo->tm_sec);
    

    if (count == 1)
    {
        findAndSetIndex(parsed[count], TYPE_DIRECTORY);
    }
    else
    {
        int    i                   = 0;
        int    j                   = 1;
        int    done                = 0;
        struct MetaData* metaData  = NULL;
        struct DirectoryIndex* dir = NULL; 
        while (1)
        {
            metaData = (struct MetaData *)malloc(sizeof(struct MetaData));
            memcpy(metaData, indexBlocksInMemory[i].index, METADATA);

            if (metaData->type == TYPE_DIRECTORY && !strcmp(metaData->name, parsed[j]))
            {
                while (1)
                {
                    if (parsed[j + 2] == NULL)
                    {
                        dir              = (struct DirectoryIndex *)malloc(sizeof(struct DirectoryIndex));
                        dir->name        = parsed[count];
                        dir->indexNumber = findAndSetIndex(parsed[count], TYPE_DIRECTORY);
                        
                        int calc = DATA_BLOCK_SIZE - ((DATA_BLOCK_SIZE / DIRECTORY_INDEX_SIZE) * DIRECTORY_INDEX_SIZE);
                        if (metaData->offset == DATA_BLOCK_SIZE - calc)
                        {
                            // Update Pointer
                            if (metaData->pointer == EMPTY)
                                metaData->pointer = findAndSetIndex(metaData->name, TYPE_DIRECTORY);

                            struct MetaData* newMeta = (struct MetaData *)malloc(sizeof(struct MetaData));
                            memcpy(newMeta, indexBlocksInMemory[metaData->pointer].index, METADATA);

                            memcpy(dataBlocksInMemory[newMeta->index].block + newMeta->offset, dir, DIRECTORY_INDEX_SIZE);
                                
                            newMeta->offset += DIRECTORY_INDEX_SIZE;
                            memcpy(indexBlocksInMemory[newMeta->index].index, newMeta, METADATA);
                            memcpy(indexBlocksInMemory[metaData->index].index, metaData, METADATA);
                        }
                        else
                        {
                            memcpy(dataBlocksInMemory[i].block + metaData->offset, dir, DIRECTORY_INDEX_SIZE);
                            // Update Offset
                            metaData->offset += DIRECTORY_INDEX_SIZE;
                            memcpy(indexBlocksInMemory[i].index, metaData, METADATA);
                        }


                        done = 1;
                        break;
                    }
                    else
                    {
                        j++;
                        int offsetCalculation = metaData->offset / DIRECTORY_INDEX_SIZE;
                        for (int k = 0; k < offsetCalculation; ++k)
                        {
                            dir = (struct DirectoryIndex *)malloc(sizeof(struct DirectoryIndex));
                            memcpy(dir, dataBlocksInMemory[i].block + (k * DIRECTORY_INDEX_SIZE), DIRECTORY_INDEX_SIZE);
                            if (!strcmp(dir->name, parsed[j]))
                            {
                                i = dir->indexNumber;
                                break;
                            }
                        }
                        break;
                    }
                }
                if (done)
                    break;
            }
            else if (i != totalBlocks)
                i++;
            else
                break;
        }

        if (!done)
        {
            fprintf(stderr, "Error: Directory \"%s\" Does Not Exist\n", parsed[j]);
            exit(EXIT_FAILURE);
        }
    }
}

void* openFile(char* filePath)
{
    int    i                   = 0;
    int    j                   = 1;
    int    count               = 0;
    char** parsed              = parseString(filePath, &count, '/');
    struct MetaData* meta      = NULL;
    struct DirectoryIndex* dir = NULL;

    while (1)
    {
        meta = (struct MetaData *)malloc(sizeof(struct MetaData));
        memcpy(meta, indexBlocksInMemory[i].index, METADATA);

        if (meta->type == TYPE_DIRECTORY && !strcmp(meta->name, parsed[j]))
        {
            j++;
            while (1)
            {
                int offsetCalculation = meta->offset / DIRECTORY_INDEX_SIZE;
                for (int k = 0; k < offsetCalculation; ++k)
                {
                    dir = (struct DirectoryIndex *)malloc(sizeof(struct DirectoryIndex));
                    memcpy(dir, dataBlocksInMemory[i].block + (k * DIRECTORY_INDEX_SIZE), DIRECTORY_INDEX_SIZE);

                    if (!strcmp(dir->name, parsed[j]))
                    {
                        i = dir->indexNumber;
                        break;
                    }
                    else
                        continue;
                }
                break;
            }
        }
        else if (i != totalBlocks)
            i++;
        else
            break;
    }

    if (!strcmp(dir->name, parsed[count]) > 0)
    {
        struct MetaData* fileMeta = (struct MetaData *)malloc(sizeof(struct MetaData));
        memcpy (fileMeta, indexBlocksInMemory[dir->indexNumber].index, METADATA);
        free   (dir);
        return fileMeta;
    }
    else
    {
        free(dir);
        return NULL;
    }

}

void closeFile(struct MetaData** fileMeta)
{
    free(*fileMeta);
    *fileMeta = NULL;
}

void deleteFile(struct MetaData* fileMeta)
{
    if (fileMeta == 0)
    {
        fprintf(stderr, "Error: File Has Been Closed (Call openFile Before deleteFile)\n");
        exit(EXIT_FAILURE);
    }
    struct FileData* fileData = (struct FileData *)malloc(sizeof(struct FileData));
    memcpy    (dataBlocksInMemory[fileMeta->index].block,  FILE_DATA_NULLCHARS, FILE_DATA);
    memcpy    (indexBlocksInMemory[fileMeta->index].index, UNUSED_INDEX, INDEX_BLOCK_SIZE);
    free      (fileData);
    closeFile (&fileMeta);
}

struct FileData* readFile(struct MetaData* fileMeta)
{
    struct FileData* data = (struct FileData *)malloc(sizeof(struct FileData));
    if (fileMeta == 0)
    {
        fprintf (stderr, "Error: File Not Allocated (Call openFile Before readFile)\n");
        exit    (EXIT_FAILURE);
    }

    if (fileMeta->pointer < 0)
    {
        memcpy(data, dataBlocksInMemory[fileMeta->index].block, FILE_DATA);
        return data;
    }
}

void writeFile(struct MetaData* fileMeta, char* data)
{
    if (fileMeta == 0)
    {
        fprintf (stderr, "Error: File Has Been Closed (Call openFile Before writeFile)\n");
        exit    (EXIT_FAILURE);
    }

    struct FileData* fileData = (struct FileData *)malloc(sizeof(struct FileData));
    memcpy(fileData, dataBlocksInMemory[fileMeta->index].block, FILE_DATA);
    if (!strcmp(fileData->data, "NULL"))
    {
        fileData->data = data;
        memcpy(dataBlocksInMemory[fileMeta->index].block, fileData, FILE_DATA);
    }
    else
    {
        char* newData = (char *)malloc(sizeof(char) * (strlen(fileData->data) + strlen(data)));
        strcpy(newData, fileData->data);
        strcat(newData, data);
        fileData->data = newData;
        memcpy(dataBlocksInMemory[fileMeta->index].block, fileData, FILE_DATA);
    }
}

void createFixedSizeFile(int totalBlocks, int numberOfBlocks, int numberOfIndices)
{
    int fd;
    if (fd = open(FILENAME, O_RDONLY|O_WRONLY|O_CREAT, 0777) < 0)
    {
        fprintf(stderr, "File: %s Failed to Load\n", FILENAME);
    }

    ftruncate(fd, (off_t)MAX_FILE_SIZE);
    close(fd);

    FILE* _fd = fopen(FILENAME, "w");
    
    // Index Loop
    for (int i = 0; i < numberOfIndices; ++i)
    {
        fprintf(_fd, "%c", NULLCHAR);
    }
    
    // Data Loop
    fprintf(_fd, "%100s", " "); /* 100 # of DATA_BLOCK_SIZE */
    for (int i = 0; i < numberOfBlocks; ++i)
    {
        fprintf(_fd, "%c", NULLCHAR);
    }

    fclose(_fd);
}

void displayFileSystem()
{
    printf("FILESYSTEM:\n");
    for (int i = 0; i < totalBlocks; ++i)
    {
        if (memcmp(UNUSED_INDEX, indexBlocksInMemory[i].index, INDEX_BLOCK_SIZE) != 0)
        {
            struct MetaData* read = (struct MetaData *)malloc(sizeof(struct MetaData));
            memcpy(read, indexBlocksInMemory[i].index, METADATA);

            printf("Type: %d | ", read->type);
            if (read->type == TYPE_DIRECTORY)
                printf("Directory Name: %s | ", read->name);
            else
                printf("File Name: %s | ", read->name);                
            //printf("%s\n", read->timeCreated);
            printf("Pointer: %d | ", read->pointer);
            printf("OffSet: %d | ", read->offset);
            printf("Index Number: %d | ", read->index);

            printf("\n");
            
            if (read->type == TYPE_DIRECTORY)
            {
                for (int j = 0; j < read->offset / DIRECTORY_INDEX_SIZE; ++j)
                {
                    struct DirectoryIndex* readFI = (struct DirectoryIndex *)malloc(sizeof(struct DirectoryIndex));
                    memcpy(readFI, dataBlocksInMemory[i].block + (DIRECTORY_INDEX_SIZE * j), DIRECTORY_INDEX_SIZE);
                    printf("Name: %s | ", readFI->name);
                    printf("Index Number: %d\n", readFI->indexNumber);
                }
            }
            else
            {
                struct FileData* readFD = (struct FileData *)malloc(sizeof(struct FileData));
                memcpy(readFD, dataBlocksInMemory[read->index].block, FILE_DATA);
                printf("Data: %s\n", readFD->data);
            }

            printf("\n");
        }
    }
}

int main(int argc, char* argv[])
{
    // This Variable is Used in Multiple Functions to Compare and to Set
    UNUSED_INDEX    = (char *)malloc(sizeof(char) * INDEX_BLOCK_SIZE);

    // Calculations
    totalBlocks     = MAX_FILE_SIZE / (DATA_BLOCK_SIZE + INDEX_BLOCK_SIZE);
    int numberOfBlocks  = totalBlocks   *  DATA_BLOCK_SIZE;
    int numberOfIndices = totalBlocks   *  INDEX_BLOCK_SIZE;

    printf("Total Blocks: %d\n", totalBlocks * 2);
    printf("Number of Data Blocks: %d | Number of Index Blocks: %d\n", totalBlocks, totalBlocks);
    printf("Size of One Data Block: %d | Size of One Index Block: %d\n", DATA_BLOCK_SIZE, INDEX_BLOCK_SIZE);
    printf("Total Size of Data Blocks: %d | Total Size of Index Blocks: %d\n\n", numberOfBlocks, numberOfIndices);
    
    // Create A Fixed Size File
    createFixedSizeFile(totalBlocks, numberOfBlocks, numberOfIndices);

    // Open File For MMAP
    int fd;
    if (fd = open(FILENAME, O_RDWR) < 0)
    {
        fprintf(stderr, "File: %s Failed to Load\n", FILENAME);
    }

    /* MMAP File System */
    indexBlocksInMemory = (struct IndexBlocks *)mmap(NULL, MAX_FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    dataBlocksInMemory  = (struct DataBlocks *)(indexBlocksInMemory + totalBlocks + 2);

    createDir("/root");
    createDir("/root/SubA");
    createDir("/root/SubB");
    createDir("/root/SubC");
    createDir("/root/SubD");
    createDir("/root/SubE");
    createDir("/root/SubF");
    createDir("/root/SubG");
    createDir("/root/SubH");

    /* This Should Over Load '/root' Because
       (DATA_BLOCK_SIZE / DIRECTORY_INDEX_SIZE) = 8.33 = 8
       Therefore Index For '/root' is Full So We Allocate
       Another Index to Extend '/root'... Now The First Indexs
       Pointer Will Point to the New Index*/
    createDir("/root/SubI");
    createDir("/root/SubJ");

    /* These Show That We Can Still Create Directories In The
       First Index of the '/root' */
    createDir("/root/SubH/Sub1");

    /* And These Show That We Can Create Directories In The
       Second Index of the '/root' */
    createDir("/root/SubJ/Sub2");

    createFile("/root/SubA/A.txt");

    struct MetaData* fdIndex = (struct MetaData *)openFile("/root/SubA/A.txt");
    if (fdIndex == NULL)
    {
        fprintf(stderr, "Error: File/Directory Does Not Exist\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        /* We Can Read and Write Data But... We Cannot Write Data
           and Read it From That Block Again Without Calling readFile
           Again. (Data Would Not Be Up to Date) */
        writeFile(fdIndex, "Hello");

        // We Can Also Append to The File
        writeFile(fdIndex, " World");
        closeFile(&fdIndex);

        /* The readFile Function Can Be Called Like So,
           File Must Be Opened Before It's Read 
           Make Sure to Comment-Out the closeFile(&fdIndex)
           Function Above */
        // struct FileData* data = (struct FileData *)readFile(fdIndex);
        // printf("DATA: %s\n\n", data->data);
    }

    /* File Must Be Opened to Be Deleted and Cannot Be Closed
       Before Calling deleteFile */
    //deleteFile(fdIndex);

    /* This Function Prints Everything In the File System */
    displayFileSystem();

    return EXIT_SUCCESS;
}