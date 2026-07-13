#include "libImmCore/src/libBasics/piLog.h"

#include <cstdarg>

namespace ImmCore
{
    piLog::piLog() : mImp(nullptr) {}
    piLog::~piLog() = default;

    bool piLog::Init(const wchar_t* path, int loggers)
    {
        (void)path;
        (void)loggers;
        return true;
    }

    void piLog::End() {}

    void piLog::Printf(const wchar_t* file, const wchar_t* func, int line, int type, const wchar_t* format, ...)
    {
        (void)file;
        (void)func;
        (void)line;
        (void)type;
        (void)format;
    }
}
