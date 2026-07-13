#include "imm_web_decoder.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    bool expect(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            return false;
        }
        return true;
    }

    std::vector<uint8_t> readFile(const char* path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    bool testSample(const char* path)
    {
        const std::vector<uint8_t> source = readFile(path);
        ImmWebDocumentSummary summary{};
        ImmWebError error{};
        const ImmWebStatus status = imm_web_inspect(source.data(), source.size(), &summary, &error);

        bool result = true;
        result &= expect(status == IMM_WEB_STATUS_OK, std::string("sample1 inspect failed: ") + error.message);
        result &= expect(summary.schema_version == IMM_WEB_OUTPUT_SCHEMA_VERSION, "schema version mismatch");
        result &= expect(summary.format_version == 0x00010001u, "format version mismatch");
        result &= expect(summary.source_size == 5831101u, "source size mismatch");
        result &= expect(summary.chunk_count == 5u, "top-level chunk count mismatch");
        result &= expect(summary.chunk_flags == 31u, "required top-level chunks were not all found");
        result &= expect(summary.sequence_type == 1u, "sequence type mismatch");
        result &= expect(summary.sequence_capabilities == 2u, "sequence capabilities mismatch");
        result &= expect(summary.asset_count == 38u, "asset count mismatch");
        result &= expect(summary.sequence_size > 0u, "sequence chunk is empty");
        result &= expect(summary.resource_table_size == 4u + 38u * 20u, "resource table size mismatch");
        return result;
    }

    bool testFailures()
    {
        bool result = true;
        ImmWebDocumentSummary summary{};
        ImmWebError error{};

        result &= expect(
            imm_web_inspect(nullptr, 0u, &summary, &error) == IMM_WEB_STATUS_INVALID_ARGUMENT,
            "null source did not fail with invalid argument");

        const uint8_t truncated[8]{};
        result &= expect(
            imm_web_inspect(truncated, sizeof(truncated), &summary, &error) == IMM_WEB_STATUS_TRUNCATED,
            "truncated header did not fail");

        uint8_t invalidHeader[16]{};
        result &= expect(
            imm_web_inspect(invalidHeader, sizeof(invalidHeader), &summary, &error) == IMM_WEB_STATUS_INVALID_SIGNATURE,
            "invalid first signature did not fail");

        return result;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Expected sample IMM path\n";
        return 2;
    }

    const bool passed = testSample(argv[1]) && testFailures();
    if (!passed)
    {
        return 1;
    }

    std::cout << "IMM_WEB_DECODER_TEST: passed\n";
    return 0;
}
