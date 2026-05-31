#ifndef MFS_H
#define MFS_H

#include <stdint.h>
#include <stdio.h>

#define MFS_MAGIC 0x3153464Du
#define MFS_BLOCK_SIZE 512u
#define MFS_MAX_FILES 64u
#define MFS_MAX_BLOCKS 1024u
#define MFS_MAX_NAME 32u

#define FAT_FREE (-2)
#define FAT_EOF (-1)

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t max_files;
    uint32_t free_blocks;
    uint32_t file_count;
    uint8_t reserved[40];
} SuperBlock;

typedef struct {
    uint8_t used;
    uint32_t size;
    int32_t first_block;
    uint32_t block_count;
    uint8_t reserved[16];
} Inode;

typedef struct {
    uint8_t used;
    char name[MFS_MAX_NAME];
    int32_t inode_index;
} DirectoryEntry;
#pragma pack(pop)

typedef struct {
    SuperBlock super;
    Inode inodes[MFS_MAX_FILES];
    DirectoryEntry dirs[MFS_MAX_FILES];
    int32_t fat[MFS_MAX_BLOCKS];
} FileSystem;

long mfs_metadata_size(void);
long mfs_block_offset(uint32_t block_index);

int mfs_is_valid_name(const char *name);
void mfs_init_empty(FileSystem *fs, uint32_t total_blocks);

int mfs_write_metadata(FILE *fp, const FileSystem *fs);
int mfs_read_metadata(FILE *fp, FileSystem *fs);
int mfs_open_existing(const char *disk_path, const char *mode, FILE **fp, FileSystem *fs);

int mfs_find_directory_entry(const FileSystem *fs, const char *name);
int mfs_find_free_inode(const FileSystem *fs);
int mfs_find_free_directory_entry(const FileSystem *fs);

void mfs_free_file_blocks(FileSystem *fs, Inode *inode, FILE *fp);
int mfs_allocate_blocks(FileSystem *fs, uint32_t needed_blocks, int32_t *first_block);

#endif
