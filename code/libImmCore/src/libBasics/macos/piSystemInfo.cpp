//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <time.h>
#include <wchar.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <CoreGraphics/CoreGraphics.h>

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
    if (total)
    {
        uint64_t memsize = 0;
        size_t size = sizeof(memsize);
        if (sysctlbyname("hw.memsize", &memsize, &size, 0, 0) != 0)
            memsize = 0;
        *total = memsize;
    }

    if (avail)
    {
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        vm_statistics64_data_t vmstat;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmstat, &count) != KERN_SUCCESS)
        {
            *avail = 0;
            return;
        }
        uint64_t pageSize = 0;
        host_page_size(mach_host_self(), (vm_size_t*)&pageSize);
        const uint64_t freePages = vmstat.free_count + vmstat.inactive_count;
        *avail = freePages * pageSize;
    }
}

int piSystemInfo_getCPUs(void)
{
    int ncpu = 1;
    size_t size = sizeof(ncpu);
    sysctlbyname("hw.ncpu", &ncpu, &size, 0, 0);
    return ncpu;
}

void piSystemInfo_getProcessor(wchar_t *str, int length, int *mhz)
{
    if (mhz)
        *mhz = 0;

    char cpu[256];
    size_t size = sizeof(cpu);
    if (sysctlbyname("machdep.cpu.brand_string", cpu, &size, 0, 0) == 0)
    {
        wchar_t *wcpu = pistr2ws(cpu);
        if (wcpu)
        {
            iCopyWString(str, length, wcpu);
            free(wcpu);
            return;
        }
    }
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
    CGDirectDisplayID display = CGMainDisplayID();
    res[0] = (int)CGDisplayPixelsWide(display);
    res[1] = (int)CGDisplayPixelsHigh(display);
}

int piSystemInfo_getIntegratedMultitouch(void)
{
    return 0;
}

int piSystemInfo_getNumMonitors(void)
{
    uint32_t count = 0;
    CGGetActiveDisplayList(0, 0, &count);
    return (int)count;
}

}
