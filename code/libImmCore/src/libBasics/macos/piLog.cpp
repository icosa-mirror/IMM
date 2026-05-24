//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../piTypes.h"
#include "../piSystemInfo.h"
#include "../piThread.h"
#include "../piStr.h"
#include "../piLog.h"

namespace ImmCore {

typedef struct
{
    unsigned int  free_memory_MB;
    unsigned int  total_memory_MB;
    int     number_cpus;
    wchar_t processor[512];
    int     mhz;
    wchar_t date[512];
    wchar_t gpuVendor[64];
    wchar_t gpuModel[512];
    unsigned int  total_videomemory_MB;
    int     mScreenResolution[2];
    int     mIntegratedMultitouch;
    int     mNumMonitors;
}piLogStartInfo;

class piLogger
{
public:
    virtual ~piLogger() {}
    virtual bool Init(const wchar_t *path, const piLogStartInfo *info) = 0;
    virtual void End(void) = 0;
    virtual void Printf(int messageId, int threadId, const wchar_t *file, const wchar_t *func, int line, int type, const wchar_t *str) = 0;
};

class piTxtLogger: public piLogger
{
public:
    bool Init(const wchar_t *path, const piLogStartInfo *info)
    {
        char *cpath = piws2str(path);
        if (!cpath)
            return false;
        mFp = fopen(cpath, "wt");
        free(cpath);
        if (!mFp)
            return false;

        fwprintf(mFp, L"===================================================\n");
        fwprintf(mFp, L"date  : %s\n", info->date);
        fwprintf(mFp, L"\n");
        fwprintf(mFp, L"Memory: %d / %d Megabytes \n", info->free_memory_MB, info->total_memory_MB);
        fwprintf(mFp, L"CPU   : %s\n", info->processor);
        fwprintf(mFp, L"Units : %d\n", info->number_cpus);
        fwprintf(mFp, L"Speed : %d Mhz\n", info->mhz);
        fwprintf(mFp, L"OS    : macOS\n");
        fwprintf(mFp, L"GPU   : %s, %s\n", info->gpuVendor, info->gpuModel);
        fwprintf(mFp, L"VRam  : %d Megabytes \n", info->total_videomemory_MB);
        fwprintf(mFp, L"Screen: %d x %d\n", info->mScreenResolution[0], info->mScreenResolution[1]);
        fwprintf(mFp, L"Multitouch Integrated: %d\n", info->mIntegratedMultitouch);
        fwprintf(mFp, L"Monitors: %d\n", info->mNumMonitors);
        fwprintf(mFp, L"===================================================\n");
        fflush(mFp);

        return true;
    }

    void End(void)
    {
        if (mFp)
            fclose(mFp);
        mFp = 0;
    }

    void Printf(int messageId, int threadId, const wchar_t *file, const wchar_t *func, int line, int type, const wchar_t *str)
    {
        (void)messageId;
        if (!mFp)
            return;

        switch (type)
        {
        case 1:
        case 2:
            fwprintf(mFp, L"[%d]  %s::%s (%d) :", threadId, file, func, line);
            fwprintf(mFp, str);
            fwprintf(mFp, L"\n");
            break;
        case 3:
        case 4:
            fwprintf(mFp, str);
            fwprintf(mFp, L"\n");
            break;
        }
        fflush(mFp);
    }

private:
    FILE *mFp = 0;
};

class piCnsLogger: public piLogger
{
public:
    bool Init(const wchar_t *path, const piLogStartInfo *info)
    {
        (void)path;
        fwprintf(stdout, L"===================================================\n");
        fwprintf(stdout, L"date      : %s\n", info->date);
        fwprintf(stdout, L"\n");
        fwprintf(stdout, L"Memory    : %d / %d Megabytes \n", info->free_memory_MB, info->total_memory_MB);
        fwprintf(stdout, L"CPU       : %s\n", info->processor);
        fwprintf(stdout, L"Units     : %d\n", info->number_cpus);
        fwprintf(stdout, L"Speed     : %d Mhz\n", info->mhz);
        fwprintf(stdout, L"OS        : macOS\n");
        fwprintf(stdout, L"GPU       : %s, %s\n", info->gpuVendor, info->gpuModel);
        fwprintf(stdout, L"VRam      : %d Megabytes \n", info->total_videomemory_MB);
        fwprintf(stdout, L"Screen    : %d x %d\n", info->mScreenResolution[0], info->mScreenResolution[1]);
        fwprintf(stdout, L"Monitors  : %d\n", info->mNumMonitors);
        fwprintf(stdout, L"Multitouch: %d\n", info->mIntegratedMultitouch);
        fwprintf(stdout, L"===================================================\n");
        fflush(stdout);
        return true;
    }

    void End(void)
    {
    }

    void Printf(int messageId, int threadId, const wchar_t *file, const wchar_t *func, int line, int type, const wchar_t *str)
    {
        (void)messageId;
        switch (type)
        {
        case 1:
        case 2:
            fwprintf(stdout, L"[%d]  %s::%s (%d) :", threadId, file, func, line);
            fwprintf(stdout, str);
            fwprintf(stdout, L"\n");
            break;
        case 3:
        case 4:
            fwprintf(stdout, str);
            fwprintf(stdout, L"\n");
            break;
        }
        fflush(stdout);
    }
};

static void piLogStartInfo_Get(piLogStartInfo *info)
{
    memset(info, 0, sizeof(piLogStartInfo));

    uint64_t fm = 0;
    uint64_t tm = 0;
    uint64_t vm = 0;
    piSystemInfo_getFreeRAM(&fm, &tm);
    vm = piSystemInfo_getVideoMemory();

    info->number_cpus = piSystemInfo_getCPUs();
    info->free_memory_MB = (unsigned int)(fm >> 20L);
    info->total_memory_MB = (unsigned int)(tm >> 20L);
    piSystemInfo_getTime(info->date, 511);
    piSystemInfo_getProcessor(info->processor, 511, &info->mhz);
    piSystemInfo_getGfxCardIdentification(info->gpuVendor, 64, info->gpuModel, 512);
    info->total_videomemory_MB = (unsigned int)(vm >> 20L);
    piSystemInfo_getScreenResolution(info->mScreenResolution);
    info->mIntegratedMultitouch = piSystemInfo_getIntegratedMultitouch();
    info->mNumMonitors = piSystemInfo_getNumMonitors();
}

struct iLog
{
    int       mNumLoggers;
    piLogger *mLoggers[8];
    int       mMessageCounter;
};

piLog::piLog()
{
}

piLog::~piLog()
{
}

bool piLog::Init(const wchar_t *path, int loggers)
{
    iLog *me = (iLog*)malloc(sizeof(iLog));
    if (!me)
        return false;
    mImp = me;

    piLogStartInfo info;
    piLogStartInfo_Get(&info);

    me->mNumLoggers = 0;
    me->mMessageCounter = 0;

    for (int i = 0; i < 2; i++)
    {
        if ((loggers & (1 << i)) == 0)
            continue;

        piLogger *lo = 0;
        if (i == 0) lo = new piTxtLogger();
        if (i == 1) lo = new piCnsLogger();
        if (!lo)
            continue;

        if (!lo->Init(path, &info))
            return false;

        me->mLoggers[me->mNumLoggers++] = lo;
    }

    return true;
}

void piLog::End(void)
{
    iLog *me = (iLog*)mImp;
    for (int i = 0; i < me->mNumLoggers; i++)
    {
        piLogger *lo = me->mLoggers[i];
        lo->End();
        delete lo;
    }
    free(me);
}

void piLog::Printf(const wchar_t *file, const wchar_t *func, int line, int type, const wchar_t *format, ...)
{
#ifndef _DEBUG
    if (type == 5) return;
#endif

    iLog *me = (iLog*)mImp;
    if (me->mNumLoggers < 1)
        return;

    va_list arglist;
    va_start(arglist, format);

    const size_t tmpstrLen = 4096;
    wchar_t *tmpstr = (wchar_t *)malloc(tmpstrLen * sizeof(wchar_t));
    if (!tmpstr)
    {
        va_end(arglist);
        return;
    }

    const int res = vswprintf(tmpstr, tmpstrLen, format, arglist);
    va_end(arglist);
    if (res < 0)
    {
        free(tmpstr);
        return;
    }

    for (int i = 0; i < me->mNumLoggers; i++)
    {
        piLogger *lo = me->mLoggers[i];
        lo->Printf(me->mMessageCounter, (int)(uint64_t)piThread_GetOSID(), file, func, line, type, tmpstr);
    }

    free(tmpstr);
    me->mMessageCounter++;
}

}
