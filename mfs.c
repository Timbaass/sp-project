#include "mfs.h"

#include <stdio.h>
#include <string.h>

long mfs_metadata_size(void) {
    return (long)(sizeof(SuperBlock) +
                  sizeof(Inode) * MFS_MAX_FILES +
                  sizeof(DirectoryEntry) * MFS_MAX_FILES +
                  sizeof(int32_t) * MFS_MAX_BLOCKS);
}

long mfs_block_offset(uint32_t block_index) {
    return mfs_metadata_size() + (long)block_index * (long)MFS_BLOCK_SIZE;
}

int mfs_is_valid_name(const char *name) {
    size_t len;

    if (name == NULL) {
        return 0;
    }

    len = strlen(name);
    if (len == 0 || len >= MFS_MAX_NAME) {
        return 0;
    }

    if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
        return 0;
    }

    return 1;
}

void mfs_init_empty(FileSystem *fs, uint32_t total_blocks) {
    uint32_t i;

    memset(fs, 0, sizeof(*fs));
    fs->super.magic = MFS_MAGIC;
    fs->super.block_size = MFS_BLOCK_SIZE;
    fs->super.total_blocks = total_blocks;
    fs->super.max_files = MFS_MAX_FILES;
    fs->super.free_blocks = total_blocks;
    fs->super.file_count = 0;

    for (i = 0; i < MFS_MAX_FILES; ++i) {
        fs->inodes[i].first_block = FAT_EOF;
        fs->dirs[i].inode_index = FAT_EOF;
    }

    for (i = 0; i < MFS_MAX_BLOCKS; ++i) {
        fs->fat[i] = FAT_FREE;
    }
}

int mfs_write_metadata(FILE *fp, const FileSystem *fs) {
    if (fseek(fp, 0, SEEK_SET) != 0) {
        return 0;
    }
    if (fwrite(&fs->super, sizeof(fs->super), 1, fp) != 1) {
        return 0;
    }
    if (fwrite(fs->inodes, sizeof(fs->inodes), 1, fp) != 1) {
        return 0;
    }
    if (fwrite(fs->dirs, sizeof(fs->dirs), 1, fp) != 1) {
        return 0;
    }
    if (fwrite(fs->fat, sizeof(fs->fat), 1, fp) != 1) {
        return 0;
    }
    return fflush(fp) == 0;
}

int mfs_read_metadata(FILE *fp, FileSystem *fs) {
    if (fseek(fp, 0, SEEK_SET) != 0) {
        return 0;
    }
    if (fread(&fs->super, sizeof(fs->super), 1, fp) != 1) {
        return 0;
    }
    if (fs->super.magic != MFS_MAGIC ||
        fs->super.block_size != MFS_BLOCK_SIZE ||
        fs->super.max_files != MFS_MAX_FILES ||
        fs->super.total_blocks == 0 ||
        fs->super.total_blocks > MFS_MAX_BLOCKS) {
        return 0;
    }
    if (fread(fs->inodes, sizeof(fs->inodes), 1, fp) != 1) {
        return 0;
    }
    if (fread(fs->dirs, sizeof(fs->dirs), 1, fp) != 1) {
        return 0;
    }
    if (fread(fs->fat, sizeof(fs->fat), 1, fp) != 1) {
        return 0;
    }
    return 1;
}

int mfs_open_existing(const char *disk_path, const char *mode, FILE **fp, FileSystem *fs) {
    *fp = fopen(disk_path, mode);
    if (*fp == NULL) {
        fprintf(stderr, "Hata: disk dosyasi acilamadi: %s\n", disk_path);
        return 0;
    }

    if (!mfs_read_metadata(*fp, fs)) {
        fclose(*fp);
        *fp = NULL;
        fprintf(stderr, "Hata: gecerli bir mini dosya sistemi degil: %s\n", disk_path);
        return 0;
    }

    return 1;
}

int mfs_find_directory_entry(const FileSystem *fs, const char *name) {
    uint32_t i;

    for (i = 0; i < MFS_MAX_FILES; ++i) {
        if (fs->dirs[i].used && strcmp(fs->dirs[i].name, name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

int mfs_find_free_inode(const FileSystem *fs) {
    uint32_t i;

    for (i = 0; i < MFS_MAX_FILES; ++i) {
        if (!fs->inodes[i].used) {
            return (int)i;
        }
    }

    return -1;
}

int mfs_find_free_directory_entry(const FileSystem *fs) {
    uint32_t i;

    for (i = 0; i < MFS_MAX_FILES; ++i) {
        if (!fs->dirs[i].used) {
            return (int)i;
        }
    }

    return -1;
}

static void clear_data_block(FILE *fp, int32_t block_index) {
    uint8_t zero[MFS_BLOCK_SIZE];

    memset(zero, 0, sizeof(zero));
    if (block_index >= 0) {
        fseek(fp, mfs_block_offset((uint32_t)block_index), SEEK_SET);
        fwrite(zero, sizeof(zero), 1, fp);
    }
}

void mfs_free_file_blocks(FileSystem *fs, Inode *inode, FILE *fp) {
    int32_t current = inode->first_block;
    uint32_t guard = 0;

    while (current != FAT_EOF && current >= 0 && guard < fs->super.total_blocks) {
        int32_t next = fs->fat[current];
        fs->fat[current] = FAT_FREE;
        fs->super.free_blocks++;
        clear_data_block(fp, current);
        current = next;
        guard++;
    }

    inode->first_block = FAT_EOF;
    inode->block_count = 0;
    inode->size = 0;
}

int mfs_allocate_blocks(FileSystem *fs, uint32_t needed_blocks, int32_t *first_block) {
    uint32_t i;
    uint32_t allocated = 0;
    int32_t first = FAT_EOF;
    int32_t previous = FAT_EOF;

    *first_block = FAT_EOF;

    if (needed_blocks == 0) {
        return 1;
    }

    if (needed_blocks > fs->super.free_blocks) {
        return 0;
    }

    for (i = 0; i < fs->super.total_blocks && allocated < needed_blocks; ++i) {
        if (fs->fat[i] == FAT_FREE) {
            if (first == FAT_EOF) {
                first = (int32_t)i;
            }
            if (previous != FAT_EOF) {
                fs->fat[previous] = (int32_t)i;
            }
            fs->fat[i] = FAT_EOF;
            previous = (int32_t)i;
            fs->super.free_blocks--;
            allocated++;
        }
    }

    if (allocated != needed_blocks) {
        int32_t current = first;
        uint32_t guard = 0;

        while (current != FAT_EOF && current >= 0 && guard < fs->super.total_blocks) {
            int32_t next = fs->fat[current];
            fs->fat[current] = FAT_FREE;
            fs->super.free_blocks++;
            current = next;
            guard++;
        }
        return 0;
    }

    *first_block = first;
    return 1;
}
