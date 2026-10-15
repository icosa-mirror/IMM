#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ImmCore
{
// Strict, locale-independent UTF-8 to native wchar_t (UTF-16 on Windows).
inline bool DecodeUtf8(const char * data, size_t length, std::wstring & result)
{
    result.clear();
    if (data == nullptr) return false;
    for (size_t offset = 0; offset < length;)
    {
        const uint8_t first = static_cast<uint8_t>(data[offset++]);
        uint32_t codePoint = 0;
        uint32_t continuationCount = 0;
        uint32_t minimum = 0;
        if (first < 0x80)
        {
            codePoint = first;
        }
        else if ((first & 0xe0) == 0xc0)
        {
            codePoint = first & 0x1f;
            continuationCount = 1;
            minimum = 0x80;
        }
        else if ((first & 0xf0) == 0xe0)
        {
            codePoint = first & 0x0f;
            continuationCount = 2;
            minimum = 0x800;
        }
        else if ((first & 0xf8) == 0xf0)
        {
            codePoint = first & 0x07;
            continuationCount = 3;
            minimum = 0x10000;
        }
        else
        {
            return false;
        }
        if (continuationCount > length - offset)
            return false;
        for (uint32_t i = 0; i < continuationCount; i++)
        {
            const uint8_t next = static_cast<uint8_t>(data[offset++]);
            if ((next & 0xc0) != 0x80)
                return false;
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        if (codePoint < minimum || codePoint == 0 || codePoint > 0x10ffff ||
            (codePoint >= 0xd800 && codePoint <= 0xdfff))
            return false;
        if (sizeof(wchar_t) == 2 && codePoint > 0xffff)
        {
            codePoint -= 0x10000;
            result.push_back(static_cast<wchar_t>(0xd800 + (codePoint >> 10)));
            result.push_back(static_cast<wchar_t>(0xdc00 + (codePoint & 0x3ff)));
        }
        else
        {
            result.push_back(static_cast<wchar_t>(codePoint));
        }
    }
    return !result.empty();
}

inline std::string EncodeUtf8(const wchar_t * text)
{
    std::string result;
    if (!text) return result;
    for (size_t i = 0; text[i]; ++i)
    {
        uint32_t cp = static_cast<uint32_t>(text[i]);
        if (sizeof(wchar_t) == 2 && cp >= 0xd800 && cp <= 0xdbff)
        {
            const uint32_t low = static_cast<uint32_t>(text[i + 1]);
            if (low >= 0xdc00 && low <= 0xdfff)
            { cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00; ++i; }
            else cp = 0xfffd;
        }
        else if ((cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) cp = 0xfffd;
        if (cp < 0x80) result.push_back(static_cast<char>(cp));
        else if (cp < 0x800)
        {
            result.push_back(static_cast<char>(0xc0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
        }
        else if (cp < 0x10000)
        {
            result.push_back(static_cast<char>(0xe0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
        }
        else
        {
            result.push_back(static_cast<char>(0xf0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
        }
    }
    return result;
}
}
