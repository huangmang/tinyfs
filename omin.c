#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FS_FILENAME "filesystem.img"
#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 1024
#define MAX_INODES 128
#define MAX_FILENAME 32
#define MAX_FILE_SIZE (BLOCK_SIZE * 10) // 10 blocks per file
#define MAX_FILES 100

typedef struct {
    int total_blocks;
    int free_blocks;
    int block_size;
    int inode_table_start;
    int bitmap_start;
    int data_start;
} Superblock;

typedef struct {
    char name[MAX_FILENAME];
    int size; // in bytes
    int blocks[MAX_FILE_SIZE / BLOCK_SIZE];
    time_t created;
    time_t modified;
} Inode;

typedef struct {
    Superblock super;
    Inode inodes[MAX_INODES];
    unsigned char bitmap[TOTAL_BLOCKS];
    FILE *fp;
} FileSystem;

// Function prototypes
int fs_find_inode(FileSystem *fs, const char *name);
void fs_close_fs(FileSystem *fs);
int get_free_block(FileSystem *fs);
void set_block(FileSystem *fs, int block, int value);
FileSystem* fs_init();
int fs_create_file(FileSystem *fs, const char *name);
int fs_write_file(FileSystem *fs, const char *name, const char *data);
int fs_read_file(FileSystem *fs, const char *name);
void fs_list_files(FileSystem *fs);
void menu();

// Helper functions for bitmap management
int get_free_block(FileSystem *fs) {
    for (int i = fs->super.data_start; i < fs->super.total_blocks; i++) {
        if (fs->bitmap[i] == 0) {
            return i;
        }
    }
    return -1; // No free block
}

void set_block(FileSystem *fs, int block, int value) {
    if (block < 0 || block >= TOTAL_BLOCKS) return;
    fs->bitmap[block] = value;
}

// Initialize filesystem structure
FileSystem* fs_init() {
    FileSystem *fs = malloc(sizeof(FileSystem));
    if (!fs) {
        perror("Failed to allocate memory for filesystem");
        exit(EXIT_FAILURE);
    }

    // Open or create filesystem image
    fs->fp = fopen(FS_FILENAME, "r+b");
    if (!fs->fp) {
        // File does not exist, create and format
        fs->fp = fopen(FS_FILENAME, "w+b");
        if (!fs->fp) {
            perror("Failed to create filesystem image");
            free(fs);
            exit(EXIT_FAILURE);
        }
        // Initialize superblock
        fs->super.total_blocks = TOTAL_BLOCKS;
        fs->super.free_blocks = TOTAL_BLOCKS - 1; // Block 0 for superblock
        fs->super.block_size = BLOCK_SIZE;
        fs->super.inode_table_start = 1;
        fs->super.bitmap_start = fs->super.inode_table_start + (sizeof(Inode) * MAX_INODES) / BLOCK_SIZE + ((sizeof(Inode) * MAX_INODES) % BLOCK_SIZE ? 1 : 0);
        fs->super.data_start = fs->super.bitmap_start + (sizeof(fs->bitmap) / BLOCK_SIZE) + ((sizeof(fs->bitmap) % BLOCK_SIZE) ? 1 : 0);

        // Initialize inodes
        memset(fs->inodes, 0, sizeof(fs->inodes));

        // Initialize bitmap
        memset(fs->bitmap, 0, sizeof(fs->bitmap));
        set_block(fs, 0, 1); // Superblock is used

        // Write superblock
        fseek(fs->fp, 0, SEEK_SET);
        fwrite(&fs->super, sizeof(Superblock), 1, fs->fp);

        // Write inode table
        fseek(fs->fp, fs->super.inode_table_start * BLOCK_SIZE, SEEK_SET);
        fwrite(fs->inodes, sizeof(Inode), MAX_INODES, fs->fp);

        // Write bitmap
        fseek(fs->fp, fs->super.bitmap_start * BLOCK_SIZE, SEEK_SET);
        fwrite(fs->bitmap, sizeof(fs->bitmap), 1, fs->fp);

        // Initialize data blocks with zeros
        char buffer[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        for (int i = fs->super.data_start; i < TOTAL_BLOCKS; i++) {
            fwrite(buffer, sizeof(char), BLOCK_SIZE, fs->fp);
        }

        fflush(fs->fp);
        printf("Filesystem initialized and formatted.\n");
    } else {
        // Load existing filesystem
        // Read superblock
        fseek(fs->fp, 0, SEEK_SET);
        fread(&fs->super, sizeof(Superblock), 1, fs->fp);

        // Read inode table
        fseek(fs->fp, fs->super.inode_table_start * BLOCK_SIZE, SEEK_SET);
        fread(fs->inodes, sizeof(Inode), MAX_INODES, fs->fp);

        // Read bitmap
        fseek(fs->fp, fs->super.bitmap_start * BLOCK_SIZE, SEEK_SET);
        fread(fs->bitmap, sizeof(fs->bitmap), 1, fs->fp);

        printf("Filesystem mounted.\n");
    }

    return fs;
}

// Close filesystem
void fs_close_fs(FileSystem *fs) {
    if (fs) {
        if (fs->fp) fclose(fs->fp);
        free(fs);
    }
}

// Find inode by name
int fs_find_inode(FileSystem *fs, const char *name) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (fs->inodes[i].name[0] != '\0' && strcmp(fs->inodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

// Create a new file
int fs_create_file(FileSystem *fs, const char *name) {
    if (fs_find_inode(fs, name) != -1) {
        printf("File %s already exists.\n", name);
        return -1;
    }

    // Find free inode
    int inode_num = -1;
    for (int i = 0; i < MAX_INODES; i++) {
        if (fs->inodes[i].name[0] == '\0') {
            inode_num = i;
            break;
        }
    }

    if (inode_num == -1) {
        printf("No free inode available.\n");
        return -1;
    }

    // Find free blocks and assign to file
    int num_blocks = (strlen(name) + 1) / BLOCK_SIZE + 1;  // Assume file size based on name length for simplicity
    for (int i = 0; i < num_blocks; i++) {
        int free_block = get_free_block(fs);
        if (free_block == -1) {
            printf("No free blocks available.\n");
            return -1;
        }
        fs->inodes[inode_num].blocks[i] = free_block;
        set_block(fs, free_block, 1);
    }

    // Assign file name and initialize inode
    strcpy(fs->inodes[inode_num].name, name);
    fs->inodes[inode_num].size = strlen(name) + 1;
    fs->inodes[inode_num].created = time(NULL);
    fs->inodes[inode_num].modified = fs->inodes[inode_num].created;

    // Write inode table to disk
    fseek(fs->fp, fs->super.inode_table_start * BLOCK_SIZE, SEEK_SET);
    fwrite(fs->inodes, sizeof(Inode), MAX_INODES, fs->fp);

    printf("File %s created successfully.\n", name);
    return 0;
}

// Main function with user interaction
int main() {
    FileSystem *fs = fs_init();
    int choice;
    char filename[MAX_FILENAME];
    char data[MAX_FILE_SIZE];

    while (1) {
        menu();
        scanf("%d", &choice);
        switch (choice) {
            case 1:
                printf("Enter file name: ");
                scanf("%s", filename);
                fs_create_file(fs, filename);
                break;
            case 2:
                // Other cases for read/write/list can be implemented here
                printf("Option 2: Implement read/write functionality\n");
                break;
            case 3:
                fs_list_files(fs);
                break;
            case 0:
                fs_close_fs(fs);
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid choice, please try again.\n");
        }
    }
}
void fs_list_files(FileSystem *fs) {
    printf("Listing files:\n");
    for (int i = 0; i < MAX_INODES; i++) {
        if (fs->inodes[i].name[0] != '\0') {
            printf("File: %s, Size: %d bytes, Created: %s, Modified: %s\n", 
                   fs->inodes[i].name, 
                   fs->inodes[i].size, 
                   ctime(&fs->inodes[i].created), 
                   ctime(&fs->inodes[i].modified));
        }
    }
}

void menu() {
    printf("\nFile System Menu:\n");
    printf("1. Create file\n");
    printf("2. Read file\n");
    printf("3. List files\n");
    printf("0. Exit\n");
    printf("Enter your choice: ");
}


