#pragma once
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <utility>
#include "piUtf8.h"

namespace ImmCore
{
// Native-only sink: Unity polls on its main thread, with no managed callback lifetime.
class piDiagnosticQueue
{
public:
    static void Capture(void * context, int severity, const wchar_t * message) noexcept
    {
        if (!context || severity < 1 || severity > 2) return;
        try
        {
            auto & self = *static_cast<piDiagnosticQueue *>(context);
            // Bound both individual messages and queue length. Warnings/errors are infrequent.
            std::wstring bounded(message ? message : L"");
            if (bounded.size() > 8192) bounded.resize(8192);
            std::string utf8 = EncodeUtf8(bounded.c_str());
            std::lock_guard<std::mutex> guard(self.mMutex);
            if (self.mMessages.size() == 64) { self.mMessages.pop_front(); ++self.mDropped; }
            self.mMessages.push_back({severity, std::move(utf8)});
        }
        catch (...) {} // Diagnostics must never throw through the renderer/importer.
    }

    // 0 empty; negative required bytes (including NUL), without consuming; positive copied bytes.
    int Read(char * output, int capacity, int * severity)
    {
        if (!severity || capacity < 0 || (capacity > 0 && !output)) return 0;
        *severity = 0;
        std::lock_guard<std::mutex> guard(mMutex);
        if (mDropped)
        {
            const std::string notice = "Native diagnostic queue overflow: " + std::to_string(mDropped) + " messages dropped.";
            const int needed = static_cast<int>(notice.size() + 1);
            if (capacity < needed) return -needed;
            std::memcpy(output, notice.c_str(), needed); *severity = 2; mDropped = 0;
            return needed;
        }
        if (mMessages.empty()) return 0;
        const auto & message = mMessages.front();
        const int needed = static_cast<int>(message.text.size() + 1);
        if (capacity < needed) return -needed;
        std::memcpy(output, message.text.c_str(), needed); *severity = message.severity;
        mMessages.pop_front();
        return needed;
    }
private:
    struct Message { int severity; std::string text; };
    std::mutex mMutex;
    std::deque<Message> mMessages;
    unsigned int mDropped = 0;
};
}
