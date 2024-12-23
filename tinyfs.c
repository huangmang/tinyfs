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
        printf("No free inodes available.\n");
        return -1;
    }

    // Initialize inode
    strncpy(fs->inodes[inode_num].name, name, MAX_FILENAME);
    fs->inodes[inode_num].size = 0;
    memset(fs->inodes[inode_num].blocks, -1, sizeof(fs->inodes[inode_num].blocks));
    fs->inodes[inode_num].created = time(NULL);
    fs->inodes[inode_num].modified = fs->inodes[inode_num].created;

    // Write inode table
    fseek(fs->fp, fs->super.inode_table_start * BLOCK_SIZE, SEEK_SET);
    fwrite(fs->inodes, sizeof(Inode), MAX_INODES, fs->fp);
    fflush(fs->fp);

    printf("File %s created with inode %d.\n", name, inode_num);
    return inode_num;
}

// Write data to a file
int fs_write_file(FileSystem *fs, const char *name, const char *data) {
    int inode_num = fs_find_inode(fs, name);
    if (inode_num == -1) {
        printf("File %s not found.\n", name);
        return -1;
    }

    Inode *inode = &fs->inodes[inode_num];
    int data_len = strlen(data);
    if (data_len > MAX_FILE_SIZE) {
        printf("Data too large. Max size is %d bytes.\n", MAX_FILE_SIZE);
        return -1;
    }

    // Calculate number of blocks needed
    int blocks_needed = (data_len + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Allocate blocks
    for (int i = 0; i < blocks_needed; i++) {
        if (inode->blocks[i] == -1) {
            int block = get_free_block(fs);
            if (block == -1) {
                printf("No free blocks available.\n");
                return -1;
            }
            inode->blocks[i] = block;
            set_block(fs, block, 1);
            fs->super.free_blocks--;

            // Update bitmap
            fseek(fs->fp, fs->super.bitmap_start * BLOCK_SIZE, SEEK_SET);
            fwrite(fs->bitmap, sizeof(fs->bitmap), 1, fs->fp);

            // Update superblock
            fseek(fs->fp, 0, SEEK_SET);
            fwrite(&fs->super, sizeof(Superblock), 1, fs->fp);
        }
    }

    // Write data to blocks
    for (int i = 0; i < blocks_needed; i++) {
        int block = inode->blocks[i];
        if (block == -1) continue;
        char buffer[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        int copy_len = (data_len > BLOCK_SIZE) ? BLOCK_SIZE : data_len;
        memcpy(buffer, data + i * BLOCK_SIZE, copy_len);
        fseek(fs->fp, block * BLOCK_SIZE, SEEK_SET);
        fwrite(buffer, sizeof(char), BLOCK_SIZE, fs->fp);
        data_len -= copy_len;
    }

    // Update inode
    inode->size = strlen(data);
    inode->modified = time(NULL);

    // Write inode table
    fseek(fs->fp, fs->super.inode_table_start * BLOCK_SIZE, SEEK_SET);
    fwrite(fs->inodes, sizeof(Inode), MAX_INODES, fs->fp);
    fflush(fs->fp);

    printf("Data written to file %s.\n", name);
    return 0;
}

// Read data from a file
int fs_read_file(FileSystem *fs, const char *name) {
    int inode_num = fs_find_inode(fs, name);
    if (inode_num == -1) {
        printf("File %s not found.\n", name);
        return -1;
    }

    Inode *inode = &fs->inodes[inode_num];
    if (inode->size == 0) {
        printf("File %s is empty.\n", name);
        return 0;
    }

    char *data = malloc(inode->size + 1);
    if (!data) {
        perror("Failed to allocate memory for reading");
        return -1;
    }
    data[inode->size] = '\0';

    int bytes_read = 0;
    for (int i = 0; i < MAX_FILE_SIZE / BLOCK_SIZE && bytes_read < inode->size; i++) {
        int block = inode->blocks[i];
        if (block == -1) break;
        char buffer[BLOCK_SIZE];
        fseek(fs->fp, block * BLOCK_SIZE, SEEK_SET);
        fread(buffer, sizeof(char), BLOCK_SIZE, fs->fp);
        int copy_len = (inode->size - bytes_read > BLOCK_SIZE) ? BLOCK_SIZE : (inode->size - bytes_read);
        memcpy(data + bytes_read, buffer, copy_len);
        bytes_read += copy_len;
    }

    printf("Content of file %s:\n%s\n", name, data);
    free(data);
    return 0;
}

// List all files
void fs_list_files(FileSystem *fs) {
    printf("Files in filesystem:\n");
    for (int i = 0; i < MAX_INODES; i++) {
        if (fs->inodes[i].name[0] != '\0') {
            printf(" - %s (size: %d bytes)\n", fs->inodes[i].name, fs->inodes[i].size);
        }
    }
}

// Main menu
void menu() {
    printf("\nSimple File System Menu:\n");
    printf("1. Create File\n");
    printf("2. Write to File\n");
    printf("3. Read from File\n");
    printf("4. List Files\n");
    printf("5. Exit\n");
    printf("Choose an option: ");
}

// Main function
int main() {
    FileSystem *fs = fs_init();

    int choice;
    char filename[MAX_FILENAME];
    char data[MAX_FILE_SIZE + 1];
    while (1) {
        menu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input.\n");
            while (getchar() != '\n'); // Clear input buffer
            continue;
        }

        switch (choice) {
            case 1:
                printf("Enter filename to create: ");
                scanf("%s", filename);
                fs_create_file(fs, filename);
                break;
            case 2:
                printf("Enter filename to write: ");
                scanf("%s", filename);
                printf("Enter data to write: ");
                getchar(); // Consume newline
                fgets(data, sizeof(data), stdin);
                data[strcspn(data, "\n")] = '\0'; // Remove newline
                fs_write_file(fs, filename, data);
                break;
            case 3:
                printf("Enter filename to read: ");
                scanf("%s", filename);
                fs_read_file(fs, filename);
                break;
            case 4:
                fs_list_files(fs);
                break;
            case 5:
                fs_close_fs(fs);
                printf("Exiting.\n");
                exit(EXIT_SUCCESS);
            default:
                printf("Invalid choice.\n");
        }
    }

    return 0;
}
