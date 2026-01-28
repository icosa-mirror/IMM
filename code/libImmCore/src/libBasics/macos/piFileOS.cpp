//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>
#include <copyfile.h>
#include <stdlib.h>
#include <limits.h>

#include "../piTypes.h"
#include "../piFile.h"
#include "../piStr.h"

namespace ImmCore {

static bool iWideToUtf8(const wchar_t *wstr, char **out)
{
    if (!wstr)
        return false;
    char *res = piws2str(wstr);
    if (!res)
        return false;
    *out = res;
    return true;
}

bool piFile::Open(const wchar_t *name, const wchar_t *mode)
{
    char *nameStr = 0;
    char *modeStr = 0;
    if (!iWideToUtf8(name, &nameStr) || !iWideToUtf8(mode, &modeStr))
    {
        if (nameStr) free(nameStr);
        if (modeStr) free(modeStr);
        return false;
    }

    FILE *fp = fopen(nameStr, modeStr);
    free(nameStr);
    free(modeStr);
    if (!fp)
        return false;

    mInternal = (void*)fp;
    return true;
}

bool piFile::Seek(uint64_t pos, SeekMode mode)
{
    int cmode = 0;
    if (mode == CURRENT) cmode = SEEK_CUR;
    if (mode == END) cmode = SEEK_END;
    if (mode == SET) cmode = SEEK_SET;

    return (fseeko((FILE*)mInternal, (off_t)pos, cmode) == 0);
}

uint64_t piFile::Tell(void)
{
    return (uint64_t)ftello((FILE*)mInternal);
}

void piFile::Close(void)
{
    fclose((FILE*)mInternal);
}

bool piFile::Exists(const wchar_t *name)
{
    char *nameStr = 0;
    if (!iWideToUtf8(name, &nameStr))
        return false;
    const int res = access(nameStr, F_OK);
    free(nameStr);
    return (res == 0);
}

uint64_t piFile::GetDiskSpace(const wchar_t *name)
{
    char *nameStr = 0;
    if (!iWideToUtf8(name, &nameStr))
        return 0;

    struct statvfs statv;
    if (statvfs(nameStr, &statv) != 0)
    {
        free(nameStr);
        return 0;
    }
    free(nameStr);
    return (uint64_t)statv.f_bavail * (uint64_t)statv.f_frsize;
}

bool piFile::DirectoryExists(const wchar_t *dirName_in)
{
    char *nameStr = 0;
    if (!iWideToUtf8(dirName_in, &nameStr))
        return false;

    struct stat st;
    const int res = stat(nameStr, &st);
    free(nameStr);
    if (res != 0)
        return false;
    return S_ISDIR(st.st_mode) != 0;
}

bool piFile::HaveWriteAccess(const wchar_t *name)
{
    char *nameStr = 0;
    if (!iWideToUtf8(name, &nameStr))
        return false;
    const int res = access(nameStr, W_OK);
    const int err = errno;
    free(nameStr);
    if (res == -1)
        return err == ENOENT;
    return true;
}

bool piFile::Copy(const wchar_t *dst, const wchar_t *src, bool failIfexists)
{
    char *dstStr = 0;
    char *srcStr = 0;
    if (!iWideToUtf8(dst, &dstStr) || !iWideToUtf8(src, &srcStr))
    {
        if (dstStr) free(dstStr);
        if (srcStr) free(srcStr);
        return false;
    }
    copyfile_flags_t flags = 0;
    if (failIfexists)
        flags |= COPYFILE_EXCL;
    const int res = copyfile(srcStr, dstStr, 0, flags);
    free(dstStr);
    free(srcStr);
    return res == 0;
}

bool piFile::Rename(const wchar_t *dst, const wchar_t *src)
{
    char *dstStr = 0;
    char *srcStr = 0;
    if (!iWideToUtf8(dst, &dstStr) || !iWideToUtf8(src, &srcStr))
    {
        if (dstStr) free(dstStr);
        if (srcStr) free(srcStr);
        return false;
    }
    const int res = rename(srcStr, dstStr);
    free(dstStr);
    free(srcStr);
    return res == 0;
}

bool piFile::Delete(const wchar_t *target)
{
    char *nameStr = 0;
    if (!iWideToUtf8(target, &nameStr))
        return false;
    const int res = unlink(nameStr);
    free(nameStr);
    return res == 0;
}

uint64_t piFile::GetLength(void)
{
    FILE *fp = (FILE*)mInternal;
    if (!fp)
        return 0;
    const off_t cur = ftello(fp);
    fseeko(fp, 0, SEEK_END);
    const off_t end = ftello(fp);
    fseeko(fp, cur, SEEK_SET);
    return (uint64_t)end;
}

bool piFile::DirectoryCreate(const wchar_t *name, bool failOnExists)
{
    char *nameStr = 0;
    if (!iWideToUtf8(name, &nameStr))
        return false;
    const int res = mkdir(nameStr, 0755);
    const int err = errno;
    free(nameStr);
    if (res == 0)
        return true;
    if (err == EEXIST)
        return !failOnExists;
    return false;
}

static bool iDirectoryDeleteRecursive(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
        return false;

    struct dirent *entry = 0;
    while ((entry = readdir(dir)) != 0)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char child[PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) < 0)
        {
            closedir(dir);
            return false;
        }
        struct stat st;
        if (lstat(child, &st) != 0)
        {
            closedir(dir);
            return false;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (!iDirectoryDeleteRecursive(child))
            {
                closedir(dir);
                return false;
            }
        }
        else
        {
            if (unlink(child) != 0)
            {
                closedir(dir);
                return false;
            }
        }
    }
    closedir(dir);
    return (rmdir(path) == 0);
}

bool piFile::DirectoryDelete(const wchar_t *name, bool evenIfNotEmpty)
{
    char *nameStr = 0;
    if (!iWideToUtf8(name, &nameStr))
        return false;

    bool ok = false;
    if (evenIfNotEmpty)
        ok = iDirectoryDeleteRecursive(nameStr);
    else
        ok = (rmdir(nameStr) == 0);

    free(nameStr);
    return ok;
}

uint64_t piFile::GetLength(const wchar_t *filename)
{
    char *nameStr = 0;
    if (!iWideToUtf8(filename, &nameStr))
        return 0;
    FILE *fp = fopen(nameStr, "rb");
    free(nameStr);
    if (!fp)
        return 0;

    const off_t cur = ftello(fp);
    fseeko(fp, 0, SEEK_END);
    const off_t end = ftello(fp);
    fseeko(fp, cur, SEEK_SET);
    fclose(fp);
    return (uint64_t)end;
}

}
