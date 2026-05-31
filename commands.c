#include "commands.h"
#include "mfs.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cli_print_usage(void) {
    puts("Kullanim:");
    puts("  minifs init <disk> [blok_sayisi]");
    puts("  minifs create <disk> <dosya_adi>");
    puts("  minifs delete <disk> <dosya_adi>");
    puts("  minifs write <disk> <dosya_adi> <metin...>");
    puts("  minifs read <disk> <dosya_adi>");
    puts("  minifs list <disk>");
    puts("  minifs status <disk>");
    puts("");
    puts("Ornek:");
    puts("  minifs init disk.bin 128");
    puts("  minifs create disk.bin not.txt");
    puts("  minifs write disk.bin not.txt Merhaba mini dosya sistemi");
    puts("  minifs read disk.bin not.txt");
}

static char *join_text_args(int argc, char **argv, int start_index) {
    int i;
    size_t total = 0;
    char *result;

    for (i = start_index; i < argc; ++i) {
        total += strlen(argv[i]);
        if (i + 1 < argc) {
            total++;
        }
    }

    result = (char *)malloc(total + 1);
    if (result == NULL) {
        return NULL;
    }

    result[0] = '\0';
    for (i = start_index; i < argc; ++i) {
        strcat(result, argv[i]);
        if (i + 1 < argc) {
            strcat(result, " ");
        }
    }

    return result;
}

int cmd_init(int argc, char **argv) {
    const char *disk_path;
    uint32_t total_blocks = 128;
    FileSystem fs;
    FILE *fp;
    long final_size;

    if (argc < 3 || argc > 4) {
        cli_print_usage();
        return 1;
    }

    disk_path = argv[2];
    if (argc == 4) {
        char *end = NULL;
        unsigned long parsed = strtoul(argv[3], &end, 10);

        if (end == argv[3] || *end != '\0' || parsed == 0 || parsed > MFS_MAX_BLOCKS) {
            fprintf(stderr, "Hata: blok_sayisi 1 ile %u arasinda olmali.\n", MFS_MAX_BLOCKS);
            return 1;
        }
        total_blocks = (uint32_t)parsed;
    }

    mfs_init_empty(&fs, total_blocks);

    fp = fopen(disk_path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Hata: disk dosyasi olusturulamadi: %s\n", disk_path);
        return 1;
    }

    if (!mfs_write_metadata(fp, &fs)) {
        fclose(fp);
        fprintf(stderr, "Hata: metadata yazilamadi.\n");
        return 1;
    }

    final_size = mfs_metadata_size() + (long)total_blocks * (long)MFS_BLOCK_SIZE;
    if (fseek(fp, final_size - 1, SEEK_SET) != 0 || fputc(0, fp) == EOF) {
        fclose(fp);
        fprintf(stderr, "Hata: disk boyutu ayarlanamadi.\n");
        return 1;
    }

    fclose(fp);
    printf("Disk olusturuldu: %s\n", disk_path);
    printf("Blok boyutu: %u bayt, veri blogu: %u, toplam boyut: %ld bayt\n",
           MFS_BLOCK_SIZE, total_blocks, final_size);
    return 0;
}

int cmd_create(int argc, char **argv) {
    const char *disk_path;
    const char *name;
    FileSystem fs;
    FILE *fp;
    int inode_index;
    int dir_index;

    if (argc != 4) {
        cli_print_usage();
        return 1;
    }

    disk_path = argv[2];
    name = argv[3];

    if (!mfs_is_valid_name(name)) {
        fprintf(stderr, "Hata: dosya adi bos olamaz, / veya \\ iceremez ve %u karakterden kisa olmali.\n",
                MFS_MAX_NAME);
        return 1;
    }

    if (!mfs_open_existing(disk_path, "r+b", &fp, &fs)) {
        return 1;
    }

    if (mfs_find_directory_entry(&fs, name) >= 0) {
        fclose(fp);
        fprintf(stderr, "Hata: dosya zaten var: %s\n", name);
        return 1;
    }

    inode_index = mfs_find_free_inode(&fs);
    dir_index = mfs_find_free_directory_entry(&fs);
    if (inode_index < 0 || dir_index < 0) {
        fclose(fp);
        fprintf(stderr, "Hata: dizin veya inode tablosu dolu.\n");
        return 1;
    }

    fs.inodes[inode_index].used = 1;
    fs.inodes[inode_index].size = 0;
    fs.inodes[inode_index].first_block = FAT_EOF;
    fs.inodes[inode_index].block_count = 0;

    fs.dirs[dir_index].used = 1;
    strncpy(fs.dirs[dir_index].name, name, MFS_MAX_NAME - 1);
    fs.dirs[dir_index].name[MFS_MAX_NAME - 1] = '\0';
    fs.dirs[dir_index].inode_index = inode_index;
    fs.super.file_count++;

    if (!mfs_write_metadata(fp, &fs)) {
        fclose(fp);
        fprintf(stderr, "Hata: metadata guncellenemedi.\n");
        return 1;
    }

    fclose(fp);
    printf("Dosya olusturuldu: %s\n", name);
    return 0;
}

int cmd_delete(int argc, char **argv) {
    const char *disk_path;
    const char *name;
    FileSystem fs;
    FILE *fp;
    int dir_index;
    int inode_index;

    if (argc != 4) {
        cli_print_usage();
        return 1;
    }

    disk_path = argv[2];
    name = argv[3];

    if (!mfs_open_existing(disk_path, "r+b", &fp, &fs)) {
        return 1;
    }

    dir_index = mfs_find_directory_entry(&fs, name);
    if (dir_index < 0) {
        fclose(fp);
        fprintf(stderr, "Hata: dosya bulunamadi: %s\n", name);
        return 1;
    }

    inode_index = fs.dirs[dir_index].inode_index;
    if (inode_index < 0 || inode_index >= (int)MFS_MAX_FILES || !fs.inodes[inode_index].used) {
        fclose(fp);
        fprintf(stderr, "Hata: dizin girdisi bozuk.\n");
        return 1;
    }

    mfs_free_file_blocks(&fs, &fs.inodes[inode_index], fp);
    memset(&fs.inodes[inode_index], 0, sizeof(fs.inodes[inode_index]));
    fs.inodes[inode_index].first_block = FAT_EOF;
    memset(&fs.dirs[dir_index], 0, sizeof(fs.dirs[dir_index]));
    fs.dirs[dir_index].inode_index = FAT_EOF;
    if (fs.super.file_count > 0) {
        fs.super.file_count--;
    }

    if (!mfs_write_metadata(fp, &fs)) {
        fclose(fp);
        fprintf(stderr, "Hata: metadata guncellenemedi.\n");
        return 1;
    }

    fclose(fp);
    printf("Dosya silindi: %s\n", name);
    return 0;
}

int cmd_write(int argc, char **argv) {
    const char *disk_path;
    const char *name;
    char *text;
    size_t text_len;
    uint32_t needed_blocks;
    FileSystem fs;
    FILE *fp;
    int dir_index;
    int inode_index;
    int32_t first_block;
    int32_t current;
    size_t written = 0;
    uint8_t block[MFS_BLOCK_SIZE];

    if (argc < 5) {
        cli_print_usage();
        return 1;
    }

    disk_path = argv[2];
    name = argv[3];
    text = join_text_args(argc, argv, 4);
    if (text == NULL) {
        fprintf(stderr, "Hata: bellek ayrilamadi.\n");
        return 1;
    }

    text_len = strlen(text);
    needed_blocks = (uint32_t)((text_len + MFS_BLOCK_SIZE - 1u) / MFS_BLOCK_SIZE);

    if (!mfs_open_existing(disk_path, "r+b", &fp, &fs)) {
        free(text);
        return 1;
    }

    dir_index = mfs_find_directory_entry(&fs, name);
    if (dir_index < 0) {
        fclose(fp);
        free(text);
        fprintf(stderr, "Hata: dosya bulunamadi: %s\n", name);
        return 1;
    }

    inode_index = fs.dirs[dir_index].inode_index;
    if (inode_index < 0 || inode_index >= (int)MFS_MAX_FILES || !fs.inodes[inode_index].used) {
        fclose(fp);
        free(text);
        fprintf(stderr, "Hata: dizin girdisi bozuk.\n");
        return 1;
    }

    if (needed_blocks > fs.super.free_blocks + fs.inodes[inode_index].block_count) {
        fclose(fp);
        free(text);
        fprintf(stderr, "Hata: yeterli bos blok yok. Gerekli: %u, kullanilabilir: %u\n",
                needed_blocks, fs.super.free_blocks + fs.inodes[inode_index].block_count);
        return 1;
    }

    mfs_free_file_blocks(&fs, &fs.inodes[inode_index], fp);
    if (!mfs_allocate_blocks(&fs, needed_blocks, &first_block)) {
        fclose(fp);
        free(text);
        fprintf(stderr, "Hata: blok ayrilamadi.\n");
        return 1;
    }

    current = first_block;
    while (current != FAT_EOF) {
        size_t remaining = text_len - written;
        size_t chunk = remaining < MFS_BLOCK_SIZE ? remaining : MFS_BLOCK_SIZE;

        memset(block, 0, sizeof(block));
        if (chunk > 0) {
            memcpy(block, text + written, chunk);
        }

        if (fseek(fp, mfs_block_offset((uint32_t)current), SEEK_SET) != 0 ||
            fwrite(block, sizeof(block), 1, fp) != 1) {
            fclose(fp);
            free(text);
            fprintf(stderr, "Hata: veri blogu yazilamadi.\n");
            return 1;
        }

        written += chunk;
        current = fs.fat[current];
    }

    fs.inodes[inode_index].size = (uint32_t)text_len;
    fs.inodes[inode_index].block_count = needed_blocks;
    fs.inodes[inode_index].first_block = first_block;

    if (!mfs_write_metadata(fp, &fs)) {
        fclose(fp);
        free(text);
        fprintf(stderr, "Hata: metadata guncellenemedi.\n");
        return 1;
    }

    fclose(fp);
    printf("Dosya yazildi: %s (%lu bayt, %u blok)\n",
           name, (unsigned long)text_len, needed_blocks);
    free(text);
    return 0;
}

int cmd_read(int argc, char **argv) {
    const char *disk_path;
    const char *name;
    FileSystem fs;
    FILE *fp;
    int dir_index;
    int inode_index;
    Inode *inode;
    int32_t current;
    uint32_t guard = 0;
    uint32_t remaining;
    uint8_t block[MFS_BLOCK_SIZE];

    if (argc != 4) {
        cli_print_usage();
        return 1;
    }

    disk_path = argv[2];
    name = argv[3];

    if (!mfs_open_existing(disk_path, "rb", &fp, &fs)) {
        return 1;
    }

    dir_index = mfs_find_directory_entry(&fs, name);
    if (dir_index < 0) {
        fclose(fp);
        fprintf(stderr, "Hata: dosya bulunamadi: %s\n", name);
        return 1;
    }

    inode_index = fs.dirs[dir_index].inode_index;
    if (inode_index < 0 || inode_index >= (int)MFS_MAX_FILES || !fs.inodes[inode_index].used) {
        fclose(fp);
        fprintf(stderr, "Hata: dizin girdisi bozuk.\n");
        return 1;
    }

    inode = &fs.inodes[inode_index];
    current = inode->first_block;
    remaining = inode->size;

    while (current != FAT_EOF && current >= 0 && guard < fs.super.total_blocks && remaining > 0) {
        uint32_t chunk = remaining < MFS_BLOCK_SIZE ? remaining : MFS_BLOCK_SIZE;

        if (fseek(fp, mfs_block_offset((uint32_t)current), SEEK_SET) != 0 ||
            fread(block, sizeof(block), 1, fp) != 1) {
            fclose(fp);
            fprintf(stderr, "Hata: veri blogu okunamadi.\n");
            return 1;
        }

        fwrite(block, 1, chunk, stdout);
        remaining -= chunk;
        current = fs.fat[current];
        guard++;
    }

    putchar('\n');
    fclose(fp);

    if (remaining != 0) {
        fprintf(stderr, "Hata: blok zinciri beklenenden once bitti.\n");
        return 1;
    }

    return 0;
}

int cmd_list(int argc, char **argv) {
    const char *disk_path;
    FileSystem fs;
    FILE *fp;
    uint32_t i;
    int any = 0;

    if (argc != 3) {
        cli_print_usage();
        return 1;
    }

    disk_path = argv[2];
    if (!mfs_open_existing(disk_path, "rb", &fp, &fs)) {
        return 1;
    }

    printf("%-32s %10s %10s %12s\n", "Dosya", "Boyut", "Blok", "Ilk blok");
    printf("%-32s %10s %10s %12s\n", "-----", "-----", "----", "--------");
    for (i = 0; i < MFS_MAX_FILES; ++i) {
        if (fs.dirs[i].used) {
            int inode_index = fs.dirs[i].inode_index;
            if (inode_index >= 0 && inode_index < (int)MFS_MAX_FILES && fs.inodes[inode_index].used) {
                printf("%-32s %10u %10u %12d\n",
                       fs.dirs[i].name,
                       fs.inodes[inode_index].size,
                       fs.inodes[inode_index].block_count,
                       fs.inodes[inode_index].first_block);
                any = 1;
            }
        }
    }

    if (!any) {
        puts("Dosya yok.");
    }

    fclose(fp);
    return 0;
}

static void print_file_blocks(const FileSystem *fs, const Inode *inode) {
    int32_t current = inode->first_block;
    uint32_t guard = 0;

    if (inode->block_count == 0) {
        printf("-");
        return;
    }

    while (current != FAT_EOF && current >= 0 && guard < fs->super.total_blocks) {
        if (guard > 0) {
            printf(" -> ");
        }
        printf("%d", current);
        current = fs->fat[current];
        guard++;
    }
}

int cmd_status(int argc, char **argv) {
    const char *disk_path;
    FileSystem fs;
    FILE *fp;
    uint32_t i;

    if (argc != 3) {
        cli_print_usage();
        return 1;
    }

    disk_path = argv[2];
    if (!mfs_open_existing(disk_path, "rb", &fp, &fs)) {
        return 1;
    }

    printf("Disk: %s\n", disk_path);
    printf("Metadata boyutu: %ld bayt\n", mfs_metadata_size());
    printf("Blok boyutu: %u bayt\n", fs.super.block_size);
    printf("Toplam veri blogu: %u\n", fs.super.total_blocks);
    printf("Bos blok: %u\n", fs.super.free_blocks);
    printf("Dolu blok: %u\n", fs.super.total_blocks - fs.super.free_blocks);
    printf("Dosya sayisi: %u / %u\n", fs.super.file_count, fs.super.max_files);
    printf("Veri kapasitesi: %lu bayt\n",
           (unsigned long)fs.super.total_blocks * (unsigned long)fs.super.block_size);
    puts("");

    puts("Blok haritasi (. bos, # dolu):");
    for (i = 0; i < fs.super.total_blocks; ++i) {
        putchar(fs.fat[i] == FAT_FREE ? '.' : '#');
        if ((i + 1) % 64 == 0 || i + 1 == fs.super.total_blocks) {
            putchar('\n');
        }
    }

    puts("");
    puts("Dosya bloklari:");
    for (i = 0; i < MFS_MAX_FILES; ++i) {
        if (fs.dirs[i].used) {
            int inode_index = fs.dirs[i].inode_index;
            if (inode_index >= 0 && inode_index < (int)MFS_MAX_FILES && fs.inodes[inode_index].used) {
                printf("  %s: ", fs.dirs[i].name);
                print_file_blocks(&fs, &fs.inodes[inode_index]);
                putchar('\n');
            }
        }
    }

    fclose(fp);
    return 0;
}
