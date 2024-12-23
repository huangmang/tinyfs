#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 1024
#define MAX_INODES 128
#define MAX_FILENAME 32
#define MAX_FILE_SIZE (BLOCK_SIZE * 10) // 10 blocks per file

// 超级块结构
typedef struct {
    int total_blocks;      // 文件系统总块数
    int free_blocks;       // 空闲块数
    int block_size;        // 块大小
    int inode_table_start; // inode表起始位置
    int bitmap_start;      // bitmap起始位置
    int data_start;        // 数据块起始位置
} Superblock;

// inode结构，存储文件的元数据
typedef struct {
    char name[MAX_FILENAME];   // 文件名
    int size;                  // 文件大小（字节数）
    int blocks[10];            // 存储文件数据块的索引，最多10个块
    time_t ctime;              // 创建时间
    time_t mtime;              // 修改时间
} Inode;

// 模拟文件系统结构
typedef struct {
    Superblock super;         // 超级块
    Inode inodes[MAX_INODES]; // inode表
    unsigned char bitmap[TOTAL_BLOCKS]; // bitmap：每个块是否被占用（0-未占用，1-已占用）
    char data[TOTAL_BLOCKS][BLOCK_SIZE]; // 模拟数据块
} FileSystem;

// 初始化文件系统
FileSystem fs;

// 查找文件名对应的inode号
int fs_find_inode(const char *name) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (strcmp(fs.inodes[i].name, name) == 0) {
            return i; // 返回inode的索引
        }
    }
    return -1; // 没有找到文件
}

// 获取一个空闲的数据块
int get_free_block() {
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (fs.bitmap[i] == 0) { // 0表示未占用
            return i;
        }
    }
    return -1; // 没有空闲块
}

// 更新文件的inode和bitmap
void fs_update_inode(int inode_index, int block_index, int size) {
    Inode *inode = &fs.inodes[inode_index];
    if (inode->size == 0) {
        inode->blocks[0] = block_index; // 初次写入
    } else {
        for (int i = 0; i < 10; i++) {
            if (inode->blocks[i] == 0) {
                inode->blocks[i] = block_index;
                break;
            }
        }
    }
    inode->size = size;  // 更新文件大小
    fs.bitmap[block_index] = 1; // 标记该块已被使用
}

// 写入文件
void fs_write_file(const char *filename, const char *data) {
    int inode_index = fs_find_inode(filename);
    if (inode_index == -1) {
        printf("File does not exist, creating a new one.\n");
        // 查找空闲的inode
        for (int i = 0; i < MAX_INODES; i++) {
            if (fs.inodes[i].name[0] == '\0') {
                inode_index = i;
                strcpy(fs.inodes[i].name, filename);
                fs.inodes[i].ctime = time(NULL); // 设置创建时间为当前时间
                break;
            }
        }
    }
    
    // 分配块并写入数据
    int data_len = strlen(data);
    int blocks_needed = (data_len + BLOCK_SIZE - 1) / BLOCK_SIZE; // 向上取整
    int current_size = 0;

    for (int i = 0; i < blocks_needed; i++) {
        int block_index = get_free_block();
        if (block_index == -1) {
            printf("No free blocks available.\n");
            return;
        }

        // 计算当前块的起始位置
        int start = i * BLOCK_SIZE;
        int end = (start + BLOCK_SIZE < data_len) ? start + BLOCK_SIZE : data_len;

        // 写入数据
        memcpy(fs.data[block_index], data + start, end - start);
        fs_update_inode(inode_index, block_index, end); // 更新inode
    }

    fs.inodes[inode_index].mtime = time(NULL); // 更新修改时间为当前时间
}

// 读取文件
void fs_read_file(const char *filename) {
    int inode_index = fs_find_inode(filename);
    if (inode_index == -1) {
        printf("File not found.\n");
        return;
    }

    Inode *inode = &fs.inodes[inode_index];
    printf("Reading file %s (size: %d bytes):\n", filename, inode->size);

    for (int i = 0; i < 10; i++) {
        if (inode->blocks[i] != 0) {
            printf("%s", fs.data[inode->blocks[i]]);
        }
    }
    printf("\n");
}

// 列出所有文件
void fs_list_files() {
    printf("Listing all files:\n");
    for (int i = 0; i < MAX_INODES; i++) {
        if (fs.inodes[i].name[0] != '\0') {
            printf("File: %s, Inode: %d, Size: %d bytes, Created: %sModified: %s",
                   fs.inodes[i].name, i, fs.inodes[i].size,
                   ctime(&fs.inodes[i].ctime),
                   ctime(&fs.inodes[i].mtime));
        }
    }
}

// 主菜单
void menu() {
    int choice;
    char filename[MAX_FILENAME];
    char data[MAX_FILE_SIZE];

    while (1) {
        printf("\nMenu:\n");
        printf("1. Create/Write File\n");
        printf("2. Read File\n");
        printf("3. List Files\n");
        printf("4. Exit\n");
        printf("Choose an option: ");
        scanf("%d", &choice);
        getchar();  // consume newline

        switch (choice) {
            case 1: // Write file
                printf("Enter file name: ");
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = 0;  // Remove newline

                printf("Enter file content: ");
                fgets(data, sizeof(data), stdin);
                data[strcspn(data, "\n")] = 0;  // Remove newline

                fs_write_file(filename, data);
                break;

            case 2: // Read file
                printf("Enter file name to read: ");
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = 0;  // Remove newline
                fs_read_file(filename);
                break;

            case 3: // List files
                fs_list_files();
                break;

            case 4: // Exit
                printf("Exiting...\n");
                return;

            default:
                printf("Invalid choice. Try again.\n");
        }
    }
}

// 初始化文件系统
void fs_init() {
    memset(&fs, 0, sizeof(FileSystem));
    fs.super.total_blocks = TOTAL_BLOCKS;
    fs.super.free_blocks = TOTAL_BLOCKS;
    fs.super.block_size = BLOCK_SIZE;
    fs.super.data_start = 0;
    fs.super.bitmap_start = 0;
    fs.super.inode_table_start = 0;
}

int main() {
    fs_init();
    menu();
    return 0;
}