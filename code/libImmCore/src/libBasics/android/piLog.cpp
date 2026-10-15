#include "../piLog.h"
#include "../piUtf8.h"
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <string>
#include <android/log.h>
namespace ImmCore {
piLog::piLog() {}
piLog::~piLog() {}
bool piLog::Init(const wchar_t * path, int loggers)
{
    if ((loggers & PILOG_TXT) && path && *path)
    {
        const std::string utf8 = EncodeUtf8(path);
        mImp = fopen(utf8.c_str(), "wb");
        return mImp != nullptr;
    }
    return true;
}
void piLog::End() { if (mImp) fclose(static_cast<FILE *>(mImp)); mImp = nullptr; }
void piLog::Printf(const wchar_t *, const wchar_t *, int, int type, const wchar_t * format, ...)
{
    if (mCallback && !mImp && !Captures(type)) return;
    std::wstring portableFormat(format);
    for (size_t pos = 0; (pos = portableFormat.find(L"%s", pos)) != std::wstring::npos; pos += 3)
        portableFormat.replace(pos, 2, L"%ls");
    wchar_t output[8192];
    va_list args;
    va_start(args, format);
    const int length = vswprintf(output, 8192, portableFormat.c_str(), args);
    va_end(args);
    if (length < 0) return;
    Capture(type, output);
    const std::string utf8 = EncodeUtf8(output);
    if (mImp)
    {
        FILE * file = static_cast<FILE *>(mImp);
        flockfile(file);
        fwrite(utf8.data(), 1, utf8.size(), file);
        fputc('\n', file);
        fflush(file);
        funlockfile(file);
    }
    // Retain native logcat for non-Unity hosts; Unity forwards captured warnings itself.
    if (!mCallback)
        __android_log_print(type == 1 ? ANDROID_LOG_ERROR : type == 2 ? ANDROID_LOG_WARN :
            type == 4 ? ANDROID_LOG_INFO : ANDROID_LOG_DEBUG, "piLog", "%s", utf8.c_str());
}
}
