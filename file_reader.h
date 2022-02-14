//
// Created by root on 12/9/20.
//
#include <stdint.h>

#ifndef PROJECT1_FILE_READER_H
#define PROJECT1_FILE_READER_H

#define bytes_per_sec 512
#define IN_RANGE(num, min, max) ((num-min)*(num-max) <= 0)
#define FAT_DELETED_MAGIC ((char)0xE5)
#define FAT_DIRECTORY = 0x10
#define FAT_VOLUME_LABEL = 0x08
typedef uint32_t lba_t;


/****/    /****/   /****/
struct dir_entry_dwa {
    char name[8];
    char ext[3];
    struct {
        uint8_t is_readonly: 1;
        uint8_t is_hidden: 1;
        uint8_t is_system: 1;
        uint8_t volume_label: 1;
        uint8_t is_directory: 1;
        uint8_t is_archived: 1;
        uint8_t filler: 2;
    } attribute;
    char smieci[14];
    uint16_t first_cluster;
    uint32_t size;
};
/****/    /****/   /****/


/****/    /****/   /****/
struct dir_entry_t {
    char name[13];
    size_t size;
    uint8_t is_archived;
    uint8_t is_readonly;
    uint8_t is_system;
    uint8_t is_hidden;
    uint8_t is_directory;
}__attribute__(( packed ));
/****/    /****/   /****/


/****/    /****/   /****/
struct fat_super_t {
    uint8_t __jump_code[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;
    uint8_t __reserved;
    uint16_t sectors_per_fat;

    uint32_t __reserved2;

    uint32_t hidden_sectors;
    uint32_t logical_sectors32;

    uint16_t __reserved3;
    uint8_t __reserved4;

    uint32_t serial_number;

    char label[11];
    char fsid[8];

    uint8_t __boot_code[448];
    uint16_t magic; // 55 aa
} __attribute__(( packed ));
/****/    /****/   /****/


/****/    /****/   /****/
struct disk_t {
    FILE *fp;
    struct fat_super_t psuper;
};
/****/    /****/   /****/

struct disk_t *disk_open_from_file(const char *volume_file_name);

int disk_read(struct disk_t *pdisk, int32_t first_sector, void *buffer, int32_t sectors_to_read);

int disk_close(struct disk_t *pdisk);


/****/    /****/   /****/
struct volume_t {
    struct disk_t *ptr;
    lba_t volume_start;
    lba_t fat1_start;
    lba_t fat2_start;
    lba_t root_start;
    lba_t sectors_per_root;
    lba_t data_start;
    lba_t available_clusters;
    uint64_t available_bytes;
    uint32_t fat_entry_count;
};
/****/    /****/   /****/
/*
{

    struct disk_t *ptr;
    int16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t sectors_per_fat;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;
    uint32_t logical_sectors32;
} __attribute__(( packed ));
*/

struct volume_t *fat_open(struct disk_t *pdisk, uint32_t first_sector);

int fat_close(struct volume_t *pvolume);


/****/    /****/   /****/
struct file_t {
    char filename[8];
    char ext[3];
    uint32_t first_cluster;
    uint32_t size;
    uint32_t position;
    struct volume_t *ptr;
};
/****/    /****/   /****/


struct file_t *file_open(struct volume_t *pvolume, const char *file_name);

int file_close(struct file_t *stream);

size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);

int32_t file_seek(struct file_t *stream, int32_t offset, int whence);


/****/    /****/   /****/
struct dir_t {
    char directoryname[8];
    uint32_t first_cluster;
    uint32_t entry;
    struct volume_t *ptrvol;
};
/****/    /****/   /****/


struct dir_t *dir_open(struct volume_t *pvolume, const char *dir_path);

int dir_read(struct dir_t *pdir, struct dir_entry_t *pentry);

int dir_close(struct dir_t *pdir);

#endif //PROJECT1_FILE_READER_H

