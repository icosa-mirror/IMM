#include "libImmCore/src/libBasics/piFile.h"
#include "libImmCore/src/libBasics/piStr.h"

#include <cstdio>
#include <cstdlib>

namespace ImmCore
{
    bool piFile::Open(const wchar_t* name, const wchar_t* mode)
    {
        char* narrowName = piws2str(name);
        char* narrowMode = piws2str(mode);
        if (narrowName == nullptr || narrowMode == nullptr)
        {
            free(narrowName);
            free(narrowMode);
            return false;
        }
        mInternal = std::fopen(narrowName, narrowMode);
        free(narrowName);
        free(narrowMode);
        return mInternal != nullptr;
    }

    bool piFile::Seek(uint64_t pos, SeekMode mode)
    {
        int origin = SEEK_SET;
        if (mode == CURRENT) origin = SEEK_CUR;
        if (mode == END) origin = SEEK_END;
        return std::fseek(static_cast<FILE*>(mInternal), static_cast<long>(pos), origin) == 0;
    }

    uint64_t piFile::Tell()
    {
        return static_cast<uint64_t>(std::ftell(static_cast<FILE*>(mInternal)));
    }

    void piFile::Close()
    {
        if (mInternal != nullptr)
        {
            std::fclose(static_cast<FILE*>(mInternal));
            mInternal = nullptr;
        }
    }

    uint64_t piFile::GetLength()
    {
        const uint64_t position = Tell();
        Seek(0u, END);
        const uint64_t length = Tell();
        Seek(position, SET);
        return length;
    }
}
