// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <time.h>
#include <wchar.h>
#include <stdlib.h>

#include "../piSystemInfo.h"
#include "../piStr.h"

namespace ImmCore {

static void iCopyWString(wchar_t *dst, int len, const wchar_t *src)
{
    if (!dst || len <= 0)
        return;
    piwstrncpy(dst, len, src, len - 1);
    dst[len - 1] = 0;
}

void piSystemInfo_getFreeRAM(uint64_t *avail, uint64_t *total)
{
    if (avail) *avail = 0;
    if (total) *total = 0;
}

int piSystemInfo_getCPUs(void)
{
    return 1;
}

void piSystemInfo_getProcessor(wchar_t *str, int length, int *mhz)
{
    if (mhz)
        *mhz = 0;
    iCopyWString(str, length, L"Unknown");
}

void piSystemInfo_getTime(wchar_t *str, int length)
{
    time_t t = time(0);
    struct tm tmv;
    localtime_r(&t, &tmv);
    wchar_t buf[128];
    wcsftime(buf, sizeof(buf) / sizeof(buf[0]), L"%Y-%m-%d %H:%M:%S", &tmv);
    iCopyWString(str, length, buf);
}

void piSystemInfo_getGfxCardIdentification(wchar_t *vendorID, int vlen, wchar_t *deviceID, int dlen)
{
    iCopyWString(vendorID, vlen, L"Apple");
    iCopyWString(deviceID, dlen, L"Unknown");
}

uint64_t piSystemInfo_getVideoMemory(void)
{
    return 0;
}

void piSystemInfo_getScreenResolution(int *res)
{
    if (!res)
        return;
    res[0] = 0;
    res[1] = 0;
}

int piSystemInfo_getIntegratedMultitouch(void)
{
    return 0;
}

int piSystemInfo_getNumMonitors(void)
{
    return 1;
}

}
