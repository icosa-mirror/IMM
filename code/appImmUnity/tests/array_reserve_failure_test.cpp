// The allocator substitution is confined to this translation unit and a unique
// trivial element type. No production specialization or allocator is replaced.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "libImmCore/src/libBasics/piStr.h"

namespace
{
    bool failNextReallocation = false;
    unsigned int reallocationCalls = 0;

    void * TestRealloc(void * buffer, size_t bytes)
    {
        ++reallocationCalls;
        if (failNextReallocation)
        {
            failNextReallocation = false;
            return nullptr;
        }
        return realloc(buffer, bytes);
    }

    struct ReserveFailureValue
    {
        uint64_t value;
    };

    void requireReserve(bool condition, const char * message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "[IMM_ARRAY_RESERVE_FAILURE_TEST] %s\n", message);
            std::exit(1);
        }
    }
}

#define realloc TestRealloc
#include "libImmCore/src/libBasics/piTArray.h"
// Compile the real untyped implementation under a private test class name to
// intercept its allocator without replacing symbols from the linked library.
#define piArray ReserveFailureRawArray
#include "libImmCore/src/libBasics/piArray.cpp"
#undef piArray
#undef realloc

static void CheckUntypedArrayReserveFailure()
{
    ImmCore::ReserveFailureRawArray array;
    requireReserve(array.Init(4, sizeof(ReserveFailureValue), true), "untyped initial allocation");
    for (uint64_t index = 0; index < 3; ++index)
    {
        const ReserveFailureValue value{ index + 201 };
        requireReserve(array.Append(&value, false) != nullptr, "untyped initial values");
    }
    void * before = array.GetAddress(0);
    const uint64_t length = array.GetLength();
    const uint64_t capacity = array.GetMaxLength();
    const unsigned int callsBeforeFailure = reallocationCalls;
    failNextReallocation = true;
    requireReserve(!array.SetMaxLengthNoShrink(128), "untyped forced reserve failure must propagate");
    requireReserve(reallocationCalls == callsBeforeFailure + 1 && !failNextReallocation,
        "untyped failure injection reached reserve allocator");
    requireReserve(array.GetAddress(0) == before && array.GetLength() == length &&
        array.GetMaxLength() == capacity, "untyped failed reserve preserves pointer, length and capacity");
    for (uint64_t index = 0; index < length; ++index)
        requireReserve(array.GetUInt64(index) == index + 201, "untyped failed reserve preserves live bytes");

    const unsigned int callsBeforeOverflow = reallocationCalls;
    requireReserve(!array.SetMaxLengthNoShrink(UINT64_MAX), "untyped overflowing allocation must reject");
    requireReserve(reallocationCalls == callsBeforeOverflow && array.GetAddress(0) == before &&
        array.GetLength() == length && array.GetMaxLength() == capacity,
        "untyped overflow rejects before allocation and preserves array state");
    requireReserve(array.SetMaxLengthNoShrink(128), "untyped successful reserve after failure");
    requireReserve(array.GetLength() == length && array.GetMaxLength() >= 128,
        "untyped successful reserve grows only capacity");
    for (uint64_t index = 0; index < length; ++index)
        requireReserve(array.GetUInt64(index) == index + 201, "untyped growth zeroing must not overwrite live bytes");
    for (uint64_t index = length; index < array.GetMaxLength(); ++index)
        requireReserve(array.GetUInt64(index) == 0, "untyped growth zeroes unused complete objects");
    array.End();

    ImmCore::ReserveFailureRawArray zeroSize;
    requireReserve(zeroSize.Init(0, 0, true), "zero-size test initialization");
    const unsigned int callsBeforeZeroSize = reallocationCalls;
    requireReserve(!zeroSize.SetMaxLengthNoShrink(8) && reallocationCalls == callsBeforeZeroSize,
        "zero object size rejects without allocation or division");
    zeroSize.End();
}

void CheckArrayReserveFailure()
{
    ImmCore::piTArray<ReserveFailureValue> array;
    requireReserve(array.Init(4, true), "initial allocation");
    for (uint64_t index = 0; index < 3; ++index)
    {
        const ReserveFailureValue value{ index + 101 };
        requireReserve(array.Append(value, false), "initial values");
    }
    ReserveFailureValue * before = array.GetAddress(0);
    const uint64_t length = array.GetLength();
    const uint64_t capacity = array.GetMaxLength();
    failNextReallocation = true;
    const unsigned int callsBeforeFailure = reallocationCalls;
    requireReserve(!array.SetMaxLengthNoShrink(128), "forced reserve failure must propagate");
    requireReserve(reallocationCalls == callsBeforeFailure + 1 && !failNextReallocation,
        "failure injection reached the real reserve allocator");
    requireReserve(array.GetAddress(0) == before && array.GetLength() == length &&
        array.GetMaxLength() == capacity, "failed reserve preserves pointer, length and capacity");
    for (uint64_t index = 0; index < length; ++index)
        requireReserve(array[index].value == index + 101, "failed reserve preserves every live value");

    const unsigned int callsBeforeOverflow = reallocationCalls;
    requireReserve(!array.SetMaxLengthNoShrink(UINT64_MAX), "overflowing allocation must reject");
    requireReserve(reallocationCalls == callsBeforeOverflow && array.GetAddress(0) == before &&
        array.GetLength() == length && array.GetMaxLength() == capacity,
        "overflow rejects before allocation and preserves array state");

    requireReserve(array.SetMaxLengthNoShrink(128), "successful reserve after failure");
    requireReserve(array.GetLength() == length && array.GetMaxLength() >= 128,
        "successful reserve grows capacity without changing length");
    for (uint64_t index = 0; index < length; ++index)
        requireReserve(array[index].value == index + 101, "successful reserve preserves live values");
    for (uint64_t index = length; index < array.GetMaxLength(); ++index)
        requireReserve(array.GetAddress(index)->value == 0, "successful zeroing reserve clears unused storage");
    array.End();
    CheckUntypedArrayReserveFailure();
    std::puts("[IMM_ARRAY_RESERVE_FAILURE_TEST] forced allocation failure, overflow rejection, preserved ownership and successful recovery passed");
}
