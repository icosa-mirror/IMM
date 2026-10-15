//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#pragma once
#include <atomic>


namespace ImmCore {

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)

#define __WFILE__     WIDEN(__FILE__)

#ifdef WINDOWS
#define __WFUNCTION__ WIDEN(__FUNCTION__)
#else
#define __WFUNCTION__ L"unkown"
#endif


#define LT_ERROR    __WFILE__, __WFUNCTION__, __LINE__, 1
#define LT_WARNING  __WFILE__, __WFUNCTION__, __LINE__, 2
#define LT_MESSAGE  __WFILE__, __WFUNCTION__, __LINE__, 3
#define LT_DEBUG    __WFILE__, __WFUNCTION__, __LINE__, 4

#define PILOG_TXT 1
#define PILOG_CNS 2


class piLog
{
public:
	piLog();
	~piLog();

    bool Init( const wchar_t *path, int loggers);
    void End( void );
    using Callback = void (*)(void *, int, const wchar_t *);
    void SetCallback(Callback callback, void * context) { mCallback = callback; mContext = context; }
    void Printf( const wchar_t *file, const wchar_t *func, int line, int type, const wchar_t *format, ... );
private:
    bool Captures(int severity) const { return mCallback && severity >= 1 && severity <= 2; }
    void Capture(int severity, const wchar_t * message) const
    { if (Captures(severity)) mCallback(mContext, severity, message); }
    std::atomic<int> mMessageCounter{0};
    Callback mCallback = nullptr;
    void * mContext = nullptr;
    void *mImp = nullptr;
};


} // namespace ImmCore
