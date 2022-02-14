//
// Created by root on 12/9/20.
//

#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include <math.h>
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"
/**************************************************************/




/****/    /****/   /****/
struct disk_t *disk_open_from_file(const char *volume_file_name)
{

    if (volume_file_name == NULL)
    {
        errno = EFAULT;
        return NULL;
    }

    FILE *f;
    f = fopen(volume_file_name, "rb");

    if (f == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    struct disk_t *dysk;
    dysk = (struct disk_t *) calloc(1, sizeof(struct disk_t));

    if (dysk == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    fread(&dysk->psuper, sizeof(struct fat_super_t), 1, f);
    dysk->fp = f;

    return dysk;
}
/****/    /****/   /****/



/****/    /****/   /****/
int disk_read(struct disk_t *pdisk, int32_t first_sector, void *buffer, int32_t sectors_to_read)
{
    if (pdisk == NULL || first_sector < 0 || buffer == NULL || sectors_to_read < 0)
    {
        errno = EFAULT;
        return -1;
    }

    fseek(pdisk->fp, first_sector * pdisk->psuper.bytes_per_sector, SEEK_SET);

    int32_t spr = (int32_t) fread(buffer, pdisk->psuper.bytes_per_sector, sectors_to_read, pdisk->fp);
    if (spr != sectors_to_read)
    {
        errno = ERANGE;
        return -1;
    }

    return sectors_to_read;
}
/****/    /****/   /****/



/****/    /****/   /****/
int disk_close(struct disk_t *pdisk)
{
    if (pdisk == NULL)
    {
        errno = EFAULT;
        return -1;
    }
    fclose(pdisk->fp);
    free(pdisk);
    return 0;
}
/****/    /****/   /****/



/**************************************************************/



/****/    /****/   /****/
struct volume_t *fat_open(struct disk_t *pdisk, uint32_t first_sector)
{
    // walidacja: //
    if (pdisk == NULL || pdisk->psuper.bytes_per_sector == 0)
    {
        errno = EFAULT;
        return NULL;
    }
    if (!(pdisk->psuper.bytes_per_sector > 0 &&
          (pdisk->psuper.bytes_per_sector & (pdisk->psuper.bytes_per_sector - 1)) == 0))
    {
        errno = EINVAL;
        return NULL;
    }
    if (!(pdisk->psuper.reserved_sectors > 0))
    {
        errno = EINVAL;
        return NULL;
    }
    if (!(IN_RANGE(pdisk->psuper.fat_count, 1, 2)))
    {
        errno = EINVAL;
        return NULL;
    }
    if (!(pdisk->psuper.root_dir_capacity * 32 % pdisk->psuper.bytes_per_sector == 0))
    {
        errno = EINVAL;
        return NULL;
    }
    if (!(pdisk->psuper.logical_sectors16 ^ pdisk->psuper.logical_sectors32))
    {
        errno = EINVAL;
        return NULL;
    }

    if (pdisk->psuper.logical_sectors16 == 0)
    {
        if (!(pdisk->psuper.logical_sectors32 > 65535))
        {
            errno = EINVAL;
            return NULL;
        }
    }

    // // // // // // // // // // // // // // // // // // // //

    struct fat_super_t psuper = pdisk->psuper;
    lba_t volume_start = first_sector;

    lba_t fat1_position = volume_start + psuper.reserved_sectors;

    lba_t fat2_position = volume_start + psuper.reserved_sectors + psuper.sectors_per_fat;

    lba_t rootdir_position = volume_start + psuper.reserved_sectors + psuper.fat_count * psuper.sectors_per_fat;

    lba_t sectors_per_rootdir = (psuper.root_dir_capacity * 32) / psuper.bytes_per_sector;

    //if ( (psuper.root_dir_capacity * sizeof(struct dir_entry_t)) % psuper.bytes_per_sector != 0)
    //    sectors_per_rootdir++;

    //lba_t cluster2_position = rootdir_position + sectors_per_rootdir;
    //(void)cluster2_position;


    if (psuper.fat_count != 2)
    {
        errno = EINVAL;
        return NULL;
    }

    uint8_t *fat1_data = (uint8_t *) malloc(psuper.bytes_per_sector * psuper.sectors_per_fat);
    uint8_t *fat2_data = (uint8_t *) malloc(psuper.bytes_per_sector * psuper.sectors_per_fat);

    if (fat1_data == NULL || fat2_data == NULL)
    {
        free(fat1_data);
        free(fat2_data);
        errno = ENOMEM;
        return NULL;
    }

    int spr1 = disk_read(pdisk, fat1_position, fat1_data, psuper.sectors_per_fat);
    if (spr1 != psuper.sectors_per_fat)
    {
        free(fat1_data);
        free(fat2_data);
        errno = EINVAL;
        return NULL;
    }

    int spr2 = disk_read(pdisk, fat2_position, fat2_data, psuper.sectors_per_fat);
    if (spr2 != psuper.sectors_per_fat)
    {
        free(fat1_data);
        free(fat2_data);
        errno = EINVAL;
        return NULL;
    }

    if (memcmp(fat1_data, fat2_data, psuper.bytes_per_sector * psuper.sectors_per_fat) != 0)
    {
        free(fat1_data);
        free(fat2_data);
        errno = EINVAL;
        return NULL;
    }

    struct volume_t *out = (struct volume_t *) calloc(1, sizeof(struct volume_t));
    if (!out)
    {
        free(fat1_data);
        free(fat2_data);
        errno = ENOMEM;
        return NULL;
    }

    out->volume_start = volume_start;

    out->fat1_start = fat1_position;

    out->fat2_start = fat2_position;

    out->root_start = rootdir_position;

    out->sectors_per_root = sectors_per_rootdir;

    out->data_start = rootdir_position + sectors_per_rootdir;

    out->available_clusters = (pdisk->psuper.logical_sectors16 + pdisk->psuper.logical_sectors32
                               - pdisk->psuper.reserved_sectors -
                               pdisk->psuper.fat_count * pdisk->psuper.sectors_per_fat - sectors_per_rootdir) /
                              pdisk->psuper.sectors_per_cluster;

    out->available_bytes = psuper.bytes_per_sector * psuper.sectors_per_cluster * out->available_clusters;

    out->fat_entry_count = psuper.sectors_per_fat * psuper.bytes_per_sector / 32;

    out->ptr = pdisk;
    /*
    out->sectors_per_fat=psuper.sectors_per_fat;
    out->logical_sectors16=psuper.logical_sectors16;
    out->logical_sectors32=psuper.logical_sectors32;
    out->bytes_per_sector=psuper.bytes_per_sector;
    out->reserved_sectors=psuper.reserved_sectors;
    out->sectors_per_cluster=psuper.sectors_per_cluster;
    out->fat_count=psuper.fat_count;
    out->root_dir_capacity=psuper.root_dir_capacity;
*/

    free(fat1_data);
    free(fat2_data);

    return out;

}
/****/    /****/   /****/



/****/    /****/   /****/
int fat_close(struct volume_t *pvolume)
{
    if (pvolume == NULL)
    {
        errno = EFAULT;
        return -1;
    }
    free(pvolume);
    return 0;
}
/****/    /****/   /****/



/**************************************************************/



/****/    /****/   /****/
struct file_t *file_open(struct volume_t *pvolume, const char *file_name)
{
    if (pvolume == NULL || file_name == NULL)
    {
        errno = EFAULT;
        return NULL;
    }

    struct file_t *plik = (struct file_t *) calloc(1, sizeof(struct file_t));
    if (!plik)
    {
        errno = ENOMEM;
        return NULL;
    }

    struct dir_entry_dwa *pdirectory = (struct dir_entry_dwa *) calloc(pvolume->fat_entry_count,
                                                                       sizeof(struct dir_entry_dwa));
    if (!pdirectory)
    {
        errno = ENOMEM;
        free(plik);
        return NULL;
    }

    char nazwa[9];
    char roz[4];
    memset(nazwa, ' ', 8);
    memset(roz, ' ', 3);
    nazwa[8] = '\0';
    roz[3] = '\0';

    for (int i = 0; i <= 8; i++)
    {
        if ( file_name[i] == '.')
        {
            i++;
            for (int j = 0; j < 3; j++)
            {
                if ( file_name[i + j] == '\0')
                    break;

                roz[j] = file_name[i + j];
            }
            break;
        }
        if (file_name[i] == '\0') break;
        nazwa[i] = file_name[i];
    }
    uint32_t spr = disk_read(pvolume->ptr, pvolume->root_start, pdirectory, pvolume->sectors_per_root);

    if (spr != pvolume->sectors_per_root)
    {
        free(plik);
        free(pdirectory);
        return NULL;
    }

    for (uint32_t i = 0; i < pvolume->fat_entry_count; i++)
    {
        struct dir_entry_dwa wejscie = pdirectory[i];
        if ( memcmp(wejscie.name, nazwa, 8) )
            continue;
        if (memcmp(wejscie.ext, roz, 3))
            continue;
        if (wejscie.attribute.is_directory == 1)
        {
            free(plik);
            free(pdirectory);
            errno = EISDIR;
            return NULL;
        }
        if (wejscie.size == 0)
        {
            free(plik);
            free(pdirectory);
            errno = EISDIR;
            return NULL;
        }

        plik->first_cluster = wejscie.first_cluster;
        plik->size = wejscie.size;
        plik->ptr = pvolume;

        memcpy(plik->ext, wejscie.ext, 3);
        memcpy(plik->filename, wejscie.name, 8);
        break;

    }
    if (plik->size == 0)
    {
        errno = ENOENT;
        free(plik);
        free(pdirectory);
        return NULL;
    }

    free(pdirectory);

    return plik;
}
/****/    /****/   /****/



/****/    /****/   /****/
int file_close(struct file_t *stream)
{
    if (stream == NULL)
    {
        errno = EFAULT;
        return -1;
    }
    free(stream);
    return 0;
}
/****/    /****/   /****/



/**************************************************************/



/****/    /****/   /****/
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream)
{
    if (ptr == NULL || size < 1 || nmemb < 1 || stream == NULL)
    {
        errno = EFAULT;
        return -1;
    }

    uint16_t bytes = fminf32(size * nmemb, stream->size - stream->position);
    if (bytes == 0)
        return 0;

    struct volume_t *volume = stream->ptr;
    struct disk_t *disk = volume->ptr;
    struct fat_super_t super = disk->psuper;

    uint16_t cstart = stream->first_cluster;

    uint16_t offset = stream->position % (disk->psuper.bytes_per_sector * disk->psuper.sectors_per_cluster);

    uint16_t clusters_to_read =
            (bytes + offset) / (disk->psuper.bytes_per_sector * disk->psuper.sectors_per_cluster) + 1;


    uint16_t *fat_buff = (uint16_t *) calloc(disk->psuper.sectors_per_fat, disk->psuper.bytes_per_sector);

    int32_t spr = disk_read(disk, volume->fat1_start, fat_buff, disk->psuper.sectors_per_fat);

    if (spr != disk->psuper.sectors_per_fat)
    {
        errno = ERANGE;
        return -1;
    }

    uint16_t clustersize = disk->psuper.bytes_per_sector * disk->psuper.sectors_per_cluster;

    for (uint16_t i = clustersize; i <= stream->position; i += clustersize)
    {
        if (cstart == 0xFFF8)
        {
            errno = ENXIO;
            free(fat_buff);
            return -1;
        }
        cstart = fat_buff[cstart];

    }
    uint8_t *data = (uint8_t *) calloc(clusters_to_read, clustersize);

    for (uint16_t i = 0; i < clusters_to_read; i++, cstart = fat_buff[cstart])
    {
        if (cstart == 0xFFF8)
            break;
        spr = disk_read(disk, volume->data_start + (cstart - 2) * super.sectors_per_cluster,
                        data + i * clustersize, disk->psuper.sectors_per_cluster);
        if (spr == -1)
        {
            free(fat_buff);
            return -1;
        }
    }

    memcpy(ptr, data + offset, bytes);
    stream->position += bytes;
    free(fat_buff);
    free(data);


    return bytes / size;
}
/****/    /****/   /****/



/****/    /****/   /****/
int32_t file_seek(struct file_t *stream, int32_t offset, int whence)
{
    if (stream == NULL)
    {
        errno = EFAULT;
        return -1;
    }
    if (IN_RANGE(whence, SEEK_SET, SEEK_END) == 0)
    {
        errno = EINVAL;
        return -1;
    }

    int32_t pom;
    if (whence == SEEK_SET)
        pom = offset;
    else if (whence == SEEK_CUR)
        pom = stream->position + offset;
    else if (whence == SEEK_END)
        pom = stream->size + offset;

    if (pom < 0 && (uint32_t) pom > stream->size)
    {
        errno = ENXIO;
        return -1;
    }

    stream->position = pom;

    return pom;
}
/****/    /****/   /****/



/**************************************************************/



/****/    /****/   /****/
struct dir_t *dir_open(struct volume_t *pvolume, const char *dir_path)
{

    if (pvolume == NULL || dir_path == NULL)
    {
        errno = EFAULT;
        return NULL;
    }
    struct dir_t *pdirectory = (struct dir_t *) calloc(1, sizeof(struct dir_t));
    if (!pdirectory)
    {
        errno = ENOMEM;
        return NULL;
    }

    if (memcmp("\\", dir_path, 1))
    {
        errno = ENOTDIR;
        free(pdirectory);
        return NULL;
    }

    pdirectory->entry = 0;
    pdirectory->first_cluster = 0;
    pdirectory->ptrvol = pvolume;

    return pdirectory;
}
/****/    /****/   /****/



/****/    /****/   /****/
int dir_read(struct dir_t *pdir, struct dir_entry_t *pentry)
{

    if (pdir == NULL || pentry == NULL)
    {
        errno = EFAULT;
        return -1;
    }


    struct dir_entry_dwa *pdirectory = (struct dir_entry_dwa *) calloc(pdir->ptrvol->fat_entry_count,
                                                                       sizeof(struct dir_entry_dwa));
    if (!pdirectory)
    {
        errno = ENOMEM;
        return -1;
    }

    struct volume_t vol = *pdir->ptrvol;
    int32_t spr = disk_read(vol.ptr, vol.root_start, pdirectory, vol.sectors_per_root);
    if (spr == -1)
    {
        free(pdirectory);
        return -1;
    }

    uint16_t etr = pdir->entry;

    if (etr == vol.fat_entry_count)
    {
        errno = EIO;
        free(pdirectory);
        return 1;
    }

    for (; etr < vol.fat_entry_count; etr++)
    {
        struct dir_entry_dwa wejscie = pdirectory[etr];

        if (wejscie.attribute.is_directory == 0 && wejscie.size == 0)
            continue;

        if (wejscie.name[0] <= 0 || wejscie.name[0] == ' ')
            continue;

        if (wejscie.attribute.is_directory == 1 && wejscie.size != 0)
            continue;

        if (memcmp("   ", wejscie.ext, 3) && wejscie.attribute.is_directory == 1)
            continue;

        pentry->is_system = wejscie.attribute.is_system;
        pentry->is_archived = wejscie.attribute.is_archived;
        pentry->is_readonly = wejscie.attribute.is_readonly;
        pentry->is_hidden = wejscie.attribute.is_hidden;
        pentry->is_directory = wejscie.attribute.is_directory;
        pentry->size = wejscie.size;

        memset(pentry->name, '\0', 13);
        for (int i = 0; i <= 8; i++)
        {
            if (memcmp(wejscie.ext, "   ", 3) && wejscie.attribute.is_directory != 1 &&
                (i == 8 || wejscie.name[i] == ' '))
            {
                pentry->name[i] = '.';
                for (int j = 0; j < 3 && wejscie.ext[j] != ' '; j++)
                {
                    pentry->name[i + j + 1] = wejscie.ext[j];
                }
                break;
            } else if (wejscie.name[i] != ' ')
            {
                pentry->name[i] = wejscie.name[i];
            }
        }

        break;
    }
    if (etr == vol.fat_entry_count)
    {
        errno = EIO;
        free(pdirectory);
        return 1;
    }
    pdir->entry = etr + 1;

    free(pdirectory);
    return 0;
}
/****/    /****/   /****/



/****/    /****/   /****/
int dir_close(struct dir_t *pdir)
{
    if (pdir == NULL)
        return -1;

    free(pdir);

    return 0;
}
/****/    /*THE END*/   /****/

