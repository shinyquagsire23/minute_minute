/*
    elm.c
    Copyright (C) 2009 yellow wood goblin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "elm.h"
#include "types.h"

#include <sys/iosupport.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/syslimits.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wctype.h>

//#include "gfx.h"

bool elm_initialized = false;
bool elm_mounted = false;
FRESULT elm_error = 0;

static FATFS fatfs = {0};
static devoptab_t devoptab = {0};
static const char* mount = "sdmc";

int _ELM_open_r(struct _reent *r, void *fileStruct, const char *path, int flags, int mode);
int _ELM_close_r(struct _reent *r, void* fd);
ssize_t _ELM_write_r(struct _reent *r, void* fd, const char *ptr, size_t len);
ssize_t _ELM_read_r(struct _reent *r, void* fd, char *ptr, size_t len);
off_t _ELM_seek_r(struct _reent *r, void* fd, off_t pos, int dir);
int _ELM_fstat_r(struct _reent *r, void* fd, struct stat *st);
int _ELM_stat_r(struct _reent *r, const char *file, struct stat *st);
int _ELM_link_r(struct _reent *r, const char *existing, const char *newLink);
int _ELM_unlink_r(struct _reent *r, const char *name);
int _ELM_chdir_r(struct _reent *r, const char *name);
int _ELM_rename_r(struct _reent *r, const char *oldName, const char *newName);
int _ELM_mkdir_r(struct _reent *r, const char *path, int mode);
DIR_ITER* _ELM_diropen_r(struct _reent *r, DIR_ITER *dirState, const char *path);
int _ELM_dirreset_r(struct _reent *r, DIR_ITER *dirState);
int _ELM_dirnext_r(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat);
int _ELM_dirclose_r(struct _reent *r, DIR_ITER *dirState);
int _ELM_statvfs_r(struct _reent *r, const char *path, struct statvfs *buf);
int _ELM_ftruncate_r(struct _reent *r, void* fd, off_t len);
int _ELM_fsync_r(struct _reent *r, void* fd);

static TCHAR CvtBuf[_MAX_LFN + 1];

void _ELM_init()
{
    if (elm_initialized) return;

    devoptab_t* dotab = &devoptab;

    dotab->name = mount;

    dotab->deviceData = &fatfs;
    dotab->structSize = sizeof(FIL);
    dotab->dirStateSize = sizeof(DIR_EX);

    dotab->open_r = _ELM_open_r;
    dotab->close_r = _ELM_close_r;

    dotab->read_r = _ELM_read_r;
    dotab->write_r = _ELM_write_r;
    dotab->seek_r = _ELM_seek_r;

    dotab->ftruncate_r = _ELM_ftruncate_r;
    dotab->fsync_r = _ELM_fsync_r;

    dotab->fstat_r = _ELM_fstat_r;
    dotab->stat_r = _ELM_stat_r;
    dotab->statvfs_r = _ELM_statvfs_r;

    dotab->link_r = _ELM_link_r;
    dotab->unlink_r = _ELM_unlink_r;
    dotab->rename_r = _ELM_rename_r;

    dotab->chdir_r = _ELM_chdir_r;
    dotab->mkdir_r = _ELM_mkdir_r;

    dotab->diropen_r = _ELM_diropen_r;
    dotab->dirclose_r = _ELM_dirclose_r;
    dotab->dirreset_r = _ELM_dirreset_r;
    dotab->dirnext_r = _ELM_dirnext_r;

    elm_initialized = true;
}

static const char* _ELM_realpath(const char* path)
{
    char buffer[MAX_FILENAME_LENGTH];

    strcpy(buffer, path);

    for (int i = 0; buffer[i]; i++)
        buffer[i] = tolower((u32) buffer[i]);

    int i;
    bool matched = false;

    for (i = 0; i < _VOLUMES; i++)
    {
        if (!strcmp(buffer, devoptab.name) && buffer[strlen(devoptab.name)] == ':')
        {
            matched = true;
            break;
        }
    }

    if (!matched) return path;

    return path + strlen(devoptab.name);
}

static TCHAR* _ELM_mbstoucs2(const char* src, size_t* len)
{
    mbstate_t ps = {0};
    wchar_t tempChar;
    int bytes;
    TCHAR* dst = CvtBuf;

    while (*src != '\0')
    {
        bytes = mbrtowc(&tempChar, src, MB_CUR_MAX, &ps);

        if (bytes > 0)
        {
            *dst = (TCHAR) tempChar;
            src += bytes;
            dst++;
        }
        else if (bytes == 0) break;
        else
        {
            dst = CvtBuf;
            break;
        }
    }

    *dst = '\0';
    if (len) *len = dst - CvtBuf;

    return CvtBuf;
}

static size_t _ELM_ucs2tombs(char* dst, const TCHAR* src)
{
    mbstate_t ps = {0};
    size_t count = 0;
    int bytes;
    char buff[MB_CUR_MAX];
    int i;

    while (*src != '\0')
    {
        bytes = wcrtomb(buff, *src, &ps);

        if (bytes < 0) return -1;

        if (bytes > 0)
        {
            for (i = 0; i < bytes; i++)
                *dst++ = buff[i];

            src++;
            count += bytes;
        }
        else break;
    }

    *dst = L'\0';
    return count;
}

int _ELM_errnoparse(struct _reent* r, int suc, int fail)
{
    int ret = fail;

    switch (elm_error)
    {
        case FR_OK:
            ret = suc;
            break;
        case FR_NO_FILE:
            r->_errno = ENOENT;
            break;
        case FR_NO_PATH:
            r->_errno = ENOENT;
            break;
        case FR_INVALID_OBJECT:
            r->_errno = EINVAL;
            break;
        case FR_INVALID_NAME:
            r->_errno = EINVAL;
            break;
        case FR_INVALID_DRIVE:
            r->_errno = EINVAL;
            break;
        case FR_INVALID_PARAMETER:
            r->_errno = EINVAL;
            break;
        case FR_TIMEOUT:
            r->_errno = ETIME;
            break;
        case FR_LOCKED:
            r->_errno = EACCES;
            break;
        case FR_NOT_ENOUGH_CORE:
            r->_errno = ENOMEM;
            break;
        case FR_TOO_MANY_OPEN_FILES:
            r->_errno = ENFILE;
            break;
        case FR_EXIST:
            r->_errno = EEXIST;
            break;
        case FR_DENIED:
            r->_errno = EACCES;
            break;
        case FR_NOT_READY:
            r->_errno = ENODEV;
            break;
        case FR_WRITE_PROTECTED:
            r->_errno = EROFS;
            break;
        case FR_DISK_ERR:
            r->_errno = EIO;
            break;
        case FR_INT_ERR:
            r->_errno = EIO;
            break;
        case FR_NOT_ENABLED:
            r->_errno = EINVAL;
            break;
        case FR_NO_FILESYSTEM:
            r->_errno = EIO;
            break;
        case FR_MKFS_ABORTED:
            r->_errno = EINVAL;
            break;
    }

    return ret;
}

int _ELM_open_r(struct _reent* r, void* fileStruct, const char* path, int flags, int mode)
{
    FIL* fp = (FIL*) fileStruct;
    BYTE m = 0;
    bool truncate = false;
    const TCHAR* p = _ELM_mbstoucs2(_ELM_realpath(path), NULL);

#if !_FS_READONLY
    if (flags & O_WRONLY)
        m |= FA_WRITE;
    else if (flags & O_RDWR)
        m |= FA_READ | FA_WRITE;
    else
#endif
        m |= FA_READ;

#if !_FS_READONLY
    if (flags & O_CREAT)
    {
        if (flags & O_EXCL)
            m |= FA_CREATE_NEW;
        else if (flags & O_TRUNC)
            m |= FA_CREATE_ALWAYS;
        else
            m |= FA_OPEN_ALWAYS;
    }
    else
#endif
    {
        if (flags & O_TRUNC)
            truncate = true;

        m |= FA_OPEN_EXISTING;
    }

    elm_error = f_open(fp, p, m);

#if (_FS_MINIMIZE < 1) && (!_FS_READONLY)
    if (elm_error == FR_OK && truncate)
        elm_error = f_truncate(fp);

    if (elm_error == FR_OK && (flags & O_APPEND))
        elm_error = f_lseek(fp, fp->fsize);
#endif

    return _ELM_errnoparse(r, (int) fp, -1);
}

int _ELM_close_r(struct _reent* r, void* fd)
{
    FIL* fp = (FIL*) fd;
    elm_error = f_close(fp);
    return _ELM_errnoparse(r, 0, -1);
}

ssize_t _ELM_write_r(struct _reent* r, void* fd, const char* ptr, size_t len)
{
#if !_FS_READONLY
    FIL* fp = (FIL*) fd;
    unsigned int written;

    elm_error = f_write(fp, ptr, len, &written);

    return _ELM_errnoparse(r, written, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

ssize_t _ELM_read_r(struct _reent* r, void* fd, char* ptr, size_t len)
{
    FIL* fp = (FIL*) fd;
    unsigned int read;
    elm_error = f_read(fp, ptr, len, &read);
    return _ELM_errnoparse(r, read, -1);
}

off_t _ELM_seek_r(struct _reent* r, void* fd, off_t pos, int dir)
{
#if _FS_MINIMIZE < 3
    FIL* fp = (FIL*) fd;
    int off = 0;

    switch (dir)
    {
        case SEEK_SET:
            off = pos;
            break;
        case SEEK_END:
            off = fp->fsize - pos;
            break;
        case SEEK_CUR:
            off = fp->fptr + pos;
            break;
    }

    elm_error = f_lseek(fp, off);
    return _ELM_errnoparse(r, fp->fptr, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int _ELM_fstat_r(struct _reent* r, void* fd, struct stat* st)
{
    r->_errno = ENOSYS;
    return -1;
}

static time_t _ELM_filetime_to_time(uint16_t t, uint16_t d)
{
    struct tm timeParts;

    timeParts.tm_hour = t >> 11;
    timeParts.tm_min = (t >> 5) & 0x3F;
    timeParts.tm_sec = (t & 0x1F) << 1;

    timeParts.tm_mday = d & 0x1F;
    timeParts.tm_mon = ((d >> 5) & 0x0F) - 1;
    timeParts.tm_year = (d >> 9) + 80;

    timeParts.tm_isdst = 0;

    return mktime(&timeParts);
}

static void _ELM_fileinfo_to_stat(const TCHAR* path, const FILINFO* fi, struct stat* st)
{
    memset(st, 0, sizeof(*st));

    st->st_mode = (fi->fattrib & AM_DIR) ? S_IFDIR : 0;
    st->st_nlink = 1;
    st->st_uid = 1;
    st->st_gid = 2;
    st->st_rdev = st->st_dev;
    st->st_size = fi->fsize;
    st->st_mtime = _ELM_filetime_to_time(fi->ftime, fi->fdate);
    //st->st_spare1 = fi->fattrib;
}

static int _ELM_chk_mounted(int disk)
{
    if (ELM_VALID_DISK(disk))
    {
#if !_FS_READONLY
        if (!fatfs.fs_type)
        {
            FILINFO fi;
            TCHAR path[3];
            const TCHAR* path_ptr = path;

            path[0] = L'0' + disk;
            path[1] = L':';
            path[2] = 0;

            f_stat(path_ptr, &fi);
        }
#endif
        return fatfs.fs_type;
    }

    return false;
}

static void _ELM_disk_to_stat(int disk, struct stat* st)
{
    memset(st, 0, sizeof(*st));

    if (_ELM_chk_mounted(disk))
    {
        FATFS* fs = &(fatfs);

        st->st_dev = (dev_t) fs;
        st->st_ino = fs->dirbase;
        st->st_mode = S_IFDIR;
        st->st_nlink = 1;
        st->st_uid = 1;
        st->st_gid = 2;
        st->st_rdev = st->st_dev;
    }
}

int _ELM_stat_r(struct _reent* r, const char* file, struct stat* st)
{
#if _FS_MINIMIZE < 1
    size_t len = 0;
    TCHAR* p = _ELM_mbstoucs2(_ELM_realpath(file), &len);

    if (p[len - 1] == L'/')
        p[len - 1] = L'\0';

    for(int i = 0; i < _VOLUMES; i++)
    {
        if(p[0] == L'0' + i && p[1] == L':' && p[2] == L'\0')
        {
            _ELM_disk_to_stat(p[0] - L'0', st);
            return 0;
        }
    }

    FILINFO fi;
    TCHAR buffer[_MAX_LFN + 1];
    fi.lfname = buffer;
    fi.lfsize = sizeof(buffer) / sizeof(buffer[0]);

    elm_error = f_stat(p, &fi);
    _ELM_fileinfo_to_stat(p, &fi, st);

    return _ELM_errnoparse(r, 0, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int _ELM_link_r(struct _reent *r, const char* existing, const char* newLink)
{
    r->_errno = ENOSYS;
    return -1;
}

int _ELM_unlink_r(struct _reent *r, const char *path)
{
#if (_FS_MINIMIZE < 1) && (!_FS_READONLY)
    const TCHAR* p = _ELM_mbstoucs2(_ELM_realpath(path), NULL);
    elm_error = f_unlink(p);
    return _ELM_errnoparse(r, 0, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int _ELM_chdir_r(struct _reent* r, const char* path)
{
#if _FS_RPATH
    const char* p = _ELM_realpath(path);
    elm_error = f_chdir(p);
    return _ELM_errnoparse(r, 0, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int _ELM_rename_r(struct _reent* r, const char* path, const char* pathp)
{
#if (_FS_MINIMIZE < 1) && (!_FS_READONLY)
    TCHAR p[_MAX_LFN + 1];
    const TCHAR* pp;

    memcpy(p, _ELM_mbstoucs2(_ELM_realpath(path), NULL), sizeof(p));
    pp = _ELM_mbstoucs2(_ELM_realpath(pathp), NULL);

    for(int i = 0; i < _VOLUMES; i++)
    {
        if(pp[0] == L'0' + i && pp[1] == L':')
        {
            pp += 2;
            break;
        }
    }

    elm_error = f_rename(p, pp);
    return _ELM_errnoparse(r, 0, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int _ELM_mkdir_r(struct _reent *r, const char *path, int mode)
{
#if (_FS_MINIMIZE < 1) && (!_FS_READONLY)
    const TCHAR* p = _ELM_mbstoucs2(_ELM_realpath(path), NULL);
    elm_error = f_mkdir(p);
    return _ELM_errnoparse(r, 0, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

DIR_ITER* _ELM_diropen_r(struct _reent* r, DIR_ITER* dirState, const char* path)
{
#if _FS_MINIMIZE < 2
    size_t len = 0;
    TCHAR* p = _ELM_mbstoucs2(_ELM_realpath(path), &len);

    if (p[len - 1] == L'/')
    {
        p[len - 1] = L'\0';
        --len;
    }

    DIR_EX* dir = (DIR_EX*) dirState->dirStruct;
    memcpy(dir->name, (void*) p, (len + 1) * sizeof(TCHAR));
    dir->namesize = len + 1;

    elm_error = f_opendir(&(dir->dir), p);

    if (elm_error != FR_OK)
        return (DIR_ITER*) _ELM_errnoparse(r, (int) dirState, 0);

    elm_error = f_readdir(&(dir->dir), NULL);
    return (DIR_ITER*) _ELM_errnoparse(r, (int) dirState, 0);
#else
    r->_errno = ENOSYS;
    return 0;
#endif
}

int _ELM_dirreset_r(struct _reent* r, DIR_ITER* dirState)
{
#if _FS_MINIMIZE < 2
    DIR_EX* dir = (DIR_EX*) dirState->dirStruct;
    elm_error = f_readdir(&(dir->dir), NULL);
    return _ELM_errnoparse(r, 0, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int dirnext(DIR_ITER *dirState, char *filename, struct stat *filestat)
{
    struct _reent *r = _REENT;
    int ret = -1;

    if (dirState != NULL)
        ret = _ELM_dirnext_r(r, dirState, filename, filestat);

    return ret;
}

int _ELM_dirnext_r(struct _reent* r, DIR_ITER* dirState, char* filename, struct stat* st)
{
#if _FS_MINIMIZE < 2
    FILINFO fi;
    TCHAR buffer[_MAX_LFN + 1];

    fi.lfname = buffer;
    fi.lfsize = sizeof(buffer) / sizeof(buffer[0]);

    DIR_EX* dir = (DIR_EX*) dirState->dirStruct;
    elm_error = f_readdir(&(dir->dir), &fi);

    if (elm_error != FR_OK)
        return _ELM_errnoparse(r, 0, -1);

    if (!fi.fname[0])
        return -1;

#ifdef _USE_LFN
    _ELM_ucs2tombs(filename, fi.lfname[0] ? fi.lfname : fi.fname);
#else
    strcpy(filename, fi.fname);
#endif

    if (st != NULL)
    {
        TCHAR path[_MAX_LFN + 1];
        size_t len = 0;

        memcpy(path, dir->name, (dir->namesize - 1) * sizeof(TCHAR));
        path[dir->namesize - 1] = L'/';

        memcpy(path + dir->namesize, _ELM_mbstoucs2(filename, &len),
                (len + 1) * sizeof(TCHAR));

        _ELM_fileinfo_to_stat(path, &fi, st);
    }
    return 0;
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int _ELM_dirclose_r(struct _reent* r, DIR_ITER* dirState)
{
    return _ELM_dirreset_r(r, dirState);
}

int _ELM_statvfs_r(struct _reent* r, const char* path, struct statvfs* buf)
{
    r->_errno = ENOSYS;
    return -1;
}

int _ELM_ftruncate_r(struct _reent* r, void* fd, off_t len)
{
#if (_FS_MINIMIZE < 1) && (!_FS_READONLY)
    FIL* fp = (FIL*) fd;
    int ptr = fp->fptr;

    elm_error = f_lseek(fp, len);

    if (elm_error != FR_OK)
        return _ELM_errnoparse(r, 0, -1);

    if (ptr > len)
        ptr = len;

    elm_error = f_truncate(fp);

    if (elm_error != FR_OK)
        return _ELM_errnoparse(r, 0, -2);

    fp->fptr = ptr;
    return 0;
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int _ELM_fsync_r(struct _reent* r, void* fd)
{
#if !_FS_READONLY
    elm_error = f_sync((FIL*) fd);
    return _ELM_errnoparse(r, 0, -1);
#else
    r->_errno = ENOSYS;
    return -1;
#endif
}

int ELM_Mount(void)
{
    _ELM_init();

    if (elm_mounted) return 0;

    FRESULT res = FR_OK;
    char buffer[_MAX_LFN];

    strcpy(buffer, mount);
    strcat(buffer, ":");
    //sprintf(buffer, "%s:", mount);
    res = f_mount(&fatfs, buffer, 1);

    if (res == FR_OK)
    {
        _ELM_chk_mounted(0);
        int dev = AddDevice(&devoptab);
        setDefaultDevice(dev);

        elm_mounted = true;
    }

    elm_error = 0;
    return res;
}

void ELM_Unmount(void)
{
    char buffer[_MAX_LFN];

    //sprintf(buffer, "%s:", mount);
    strcpy(buffer, mount);
    strcat(buffer, ":");
    RemoveDevice(buffer);
    f_mount(NULL, buffer, 1);

    elm_mounted = false;
}

int ELM_ClusterSizeFromDisk(int disk, uint32_t* size)
{
    if (_ELM_chk_mounted(disk))
    {
        *size = fatfs.csize * ELM_SS(fatfs);
        return true;
    }

    return false;
}

int ELM_ClustersFromDisk(int disk, uint32_t* clusters)
{
    if (_ELM_chk_mounted(disk))
    {
        *clusters = fatfs.n_fatent - 2;
        return true;
    }

    return false;
}

int ELM_FreeClustersFromDisk(int disk, uint32_t* clusters)
{
    if (ELM_VALID_DISK(disk))
    {
        FATFS *fs;
        DWORD free_clusters = 0;
        TCHAR path[3];

        path[0] = L'0' + disk;
        path[1] = L':';
        path[2] = 0;

        if (f_getfree(path, &free_clusters, &fs) == FR_OK)
        {
            *clusters = free_clusters;
            return true;
        }
    }

    return false;
}

int ELM_SectorsFromDisk(int disk, uint32_t* sectors)
{
    if (_ELM_chk_mounted(disk))
    {
        *sectors = (fatfs.n_fatent - 2) * fatfs.csize;
        return true;
    }

    return false;
}

int ELM_ClusterSizeFromHandle(int fildes, uint32_t* size)
{
    __handle* handle = __get_handle(fildes);

    if(handle)
    {
        FIL* fd = (FIL*)handle->fileStruct;
        *size = fd->fs->csize * ELM_SS(fd->fs);
        return true;
    }

    return false;
}

int ELM_SectorsPerClusterFromHandle(int fildes, uint32_t* per)
{
    __handle* handle = __get_handle(fildes);

    if(handle)
    {
        FIL* fd = (FIL*)handle->fileStruct;
        *per = fd->fs->csize;
        return true;
    }

    return false;
}

DWORD get_fat(FATFS* fs, DWORD clst);
DWORD clust2sect(FATFS *fs, DWORD clst);

uint32_t ELM_GetFAT(int fildes, uint32_t cluster, uint32_t* sector)
{
    uint32_t result = 0;
    __handle* handle = __get_handle(fildes);

    if (sector) *sector = 0;

    if (handle)
    {
        FIL* fd = (FIL*) handle->fileStruct;

        if (cluster == 1)
            result = fd->sclust;

        else
        {
            result = get_fat(fd->fs, cluster);

            if (result < 2 || result >= fd->fs->n_fatent)
                result = 0;
        }

        if (sector && result)
            *sector = clust2sect(fd->fs, result);
    }
    return result;
}

#if !_FS_READONLY
int ELM_DirEntry(int fildes, uint64_t* entry)
{
    __handle* handle = __get_handle(fildes);

    if(handle)
    {
        FIL* fd = (FIL*)handle->fileStruct;
        uint64_t value = fd->dir_sect;
        value = value * ELM_SS(fd->fs) + (fd->dir_ptr - fd->fs->win);
        *entry = value;
        return true;
    }

    return false;
}
#endif

uint32_t ELM_GetSectorCount(unsigned char drive)
{
    uint32_t temp;
    disk_ioctl(drive, GET_SECTOR_COUNT, &temp);
    return temp;
}
