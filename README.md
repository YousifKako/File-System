SubParser
    SubParser is a string parser from the Shell lab but slightly modified

struct MetaData
    Struct holds metadata

struct FileData
    Struct holds file bytes
    - This is written in the data section

These two structs contain the same content... Helps me differentiate
between loading a file or a directory
    struct FileIndex
        Struct holds Filename and Index Number
        - This is stored in the data section of its
          current directory

    struct DirectoryIndex
        Struct holds DirectoryName and Index Number
        - This is stored in the data section of its
          current directory

struct IndexBlocks
    To Load MMAP

struct DataBlocks
    To Load MMAP

int findAndSetIndex(char* directoryName, int type)
    This is called to find free space in the filesystem
    and allocates it

void createFile(char* filePath)
    This is used to create a file in the filesystem
void createDir(char* directoryIndex)
    This is used to create a directory in the filesystem
    
void* openFile(char* filePath)
    This function opens a specific file

void closeFile(struct MetaData** fileMeta)
    This functions closes a specific file

void deleteFile(struct MetaData* fileMeta)
    This function deletes a file from the filesystem
    - File must be opened before this function is called
    
struct FileData* readFile(struct MetaData* fileMeta)
    This function reads a specific file

void writeFile(struct MetaData* fileMeta, char* data)
    This function writes to a specific file
    - You may also append to the file by calling the
      function multiple times with new data each time

void createFixedSizeFile(int totalBlocks, int numberOfBlocks, int numberOfIndices)
    This function creates a fixed size file and sets
    evey byte to '\0'

void displayFileSystem()
    This function prints the entire filesystem