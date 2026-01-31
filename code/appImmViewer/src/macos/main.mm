//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>

extern int piMainFunc(const wchar_t *path, const wchar_t **args, int numArgs, void *instance);

static bool iToWide(const char *src, wchar_t **out)
{
    if (!src)
        return false;
    size_t len = mbstowcs(0, src, 0);
    if (len == (size_t)-1)
        return false;
    wchar_t *dst = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!dst)
        return false;
    mbstowcs(dst, src, len + 1);
    *out = dst;
    return true;
}

int main(int argc, char **argv)
{
    char exePath[PATH_MAX];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) != 0)
        return -1;

    char realPath[PATH_MAX];
    if (!realpath(exePath, realPath))
        return -1;

    char dirPath[PATH_MAX];
    strncpy(dirPath, realPath, sizeof(dirPath));
    dirPath[sizeof(dirPath)-1] = 0;
    char *slash = strrchr(dirPath, '/');
    if (slash) *slash = 0;

    chdir(dirPath);

    wchar_t *wDir = 0;
    if (!iToWide(dirPath, &wDir))
        return -1;

    wchar_t **wArgs = (wchar_t**)malloc(sizeof(wchar_t*) * argc);
    if (!wArgs)
    {
        free(wDir);
        return -1;
    }

    int numArgs = 0;
    for (int i = 0; i < argc; i++)
    {
        wchar_t *wArg = 0;
        if (iToWide(argv[i], &wArg))
            wArgs[numArgs++] = wArg;
    }

    int res = piMainFunc(wDir, (const wchar_t**)wArgs, numArgs, 0);

    for (int i = 0; i < numArgs; i++)
        free(wArgs[i]);
    free(wArgs);
    free(wDir);

    return res;
}
