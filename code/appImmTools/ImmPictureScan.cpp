// ImmPictureScan - importer-backed picture layer metadata scanner.

#include "libImmCore/src/libBasics/piLog.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/fromImmersive/fromImmersive.h"
#include "libImmImporter/src/fromImmersive/strokeCollector.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

struct PictureInfo
{
    uint32_t layerId = 0;
    uint32_t contentType = 0;
    int width = 0;
    int height = 0;
    bool hasAlpha = false;
    bool viewerLocked = false;
    int dataSize = 0;
};

class PictureCollector final : public ImmImporter::IStrokeCollector
{
public:
    void OnBeginLayer(uint32_t, uint32_t, const wchar_t *, bool, float) override
    {
        mLayerCount++;
    }

    void OnPictureLayer(uint32_t layerId,
                        uint32_t contentType,
                        bool isViewerLocked,
                        int width,
                        int height,
                        bool hasAlpha,
                        const uint8_t *,
                        int pixelDataSize) override
    {
        PictureInfo info;
        info.layerId = layerId;
        info.contentType = contentType;
        info.width = width;
        info.height = height;
        info.hasAlpha = hasAlpha;
        info.viewerLocked = isViewerLocked;
        info.dataSize = pixelDataSize;
        mPictures.push_back(info);
    }

    void OnBeginDrawing(uint32_t) override {}
    void OnStroke(uint32_t, uint8_t, uint8_t, uint32_t, const ImmImporter::Point *, const ImmCore::bound3 &) override {}
    void OnEndDrawing() override {}
    void OnEndLayer() override {}

    int mLayerCount = 0;
    std::vector<PictureInfo> mPictures;
};

const char *PictureTypeName(uint32_t type)
{
    switch (type)
    {
    case 0: return "Image2D";
    case 1: return "Image360EquirectMono";
    case 2: return "Image360EquirectStereo";
    case 3: return "Image360CubemapCrossMono";
    case 4: return "Image360CubemapVstripMono";
    default: return "Unknown";
    }
}

std::string SanitizeField(const char *text)
{
    std::string value = text ? text : "";
    for (char &c : value)
    {
        if (c == '\t' || c == '\n' || c == '\r')
        {
            c = ' ';
        }
    }
    return value;
}

bool ToWidePath(const char *path, wchar_t *output, size_t outputCount)
{
    if (!path || !path[0] || !output || outputCount == 0)
    {
        return false;
    }

    const size_t len = mbstowcs(output, path, outputCount - 1);
    if (len == (size_t)-1)
    {
        return false;
    }
    output[len] = 0;
    return true;
}

void PrintUsage(const char *argv0)
{
    std::fprintf(stderr, "usage: %s [--no-header] file.imm [file2.imm ...]\n", argv0);
}

int ScanFile(const char *path, ImmCore::piLog *log)
{
    wchar_t pathW[PATH_MAX] = {};
    if (!ToWidePath(path, pathW, PATH_MAX))
    {
        std::printf("%s\tinvalid_path\t0\t0\t0\t0\t0\t0\t0\t\n", SanitizeField(path).c_str());
        return 1;
    }

    ImmImporter::Sequence sequence;
    PictureCollector collector;
    const bool ok = ImmImporter::ImportFromDisk(&sequence,
                                                log,
                                                pathW,
                                                ImmImporter::Drawing::ColorSpace::Gamma,
                                                ImmImporter::Drawing::PaintRenderingTechnique::Static,
                                                &collector);
    if (!ok)
    {
        std::printf("%s\timport_failed\t0\t0\t0\t0\t0\t0\t0\t\n", SanitizeField(path).c_str());
        return 1;
    }

    int image2d = 0;
    int equirect = 0;
    int cubemap = 0;
    int cubemapCross = 0;
    int cubemapVstrip = 0;
    std::string details;
    for (const PictureInfo &picture : collector.mPictures)
    {
        if (picture.contentType == 0)
        {
            image2d++;
        }
        else if (picture.contentType == 1 || picture.contentType == 2)
        {
            equirect++;
        }
        else if (picture.contentType == 3 || picture.contentType == 4)
        {
            cubemap++;
            if (picture.contentType == 3)
            {
                cubemapCross++;
            }
            else
            {
                cubemapVstrip++;
            }
        }

        if (!details.empty())
        {
            details += ";";
        }
        char item[256] = {};
        std::snprintf(item,
                      sizeof(item),
                      "layer=%u,type=%u:%s,%dx%d,alpha=%d,locked=%d,bytes=%d",
                      picture.layerId,
                      picture.contentType,
                      PictureTypeName(picture.contentType),
                      picture.width,
                      picture.height,
                      picture.hasAlpha ? 1 : 0,
                      picture.viewerLocked ? 1 : 0,
                      picture.dataSize);
        details += item;
    }

    std::printf("%s\tok\t%d\t%zu\t%d\t%d\t%d\t%d\t%d\t%s\n",
                SanitizeField(path).c_str(),
                collector.mLayerCount,
                collector.mPictures.size(),
                image2d,
                equirect,
                cubemap,
                cubemapCross,
                cubemapVstrip,
                details.c_str());
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    bool printHeader = true;
    int firstFile = 1;
    if (argc > 1 && std::strcmp(argv[1], "--no-header") == 0)
    {
        printHeader = false;
        firstFile = 2;
    }

    if (argc <= firstFile)
    {
        PrintUsage(argv[0]);
        return 2;
    }

    const char *logPath = std::getenv("IMM_PICTURE_SCAN_LOG_PATH");
    if (!logPath || !logPath[0])
    {
        logPath = "/tmp/imm_picture_scan_log.txt";
    }

    wchar_t logPathW[PATH_MAX] = {};
    if (!ToWidePath(logPath, logPathW, PATH_MAX))
    {
        std::fprintf(stderr, "ImmPictureScan: invalid log path\n");
        return 2;
    }

    ImmCore::piLog log;
    if (!log.Init(logPathW, PILOG_TXT))
    {
        std::fprintf(stderr, "ImmPictureScan: failed to initialize log\n");
        return 2;
    }

    if (printHeader)
    {
        std::printf("path\tstatus\tlayers\tpictures\timage2D\tequirect360\tcubemap360\tcubemapCross\tcubemapVstrip\tdetails\n");
    }

    int failures = 0;
    for (int i = firstFile; i < argc; i++)
    {
        failures += ScanFile(argv[i], &log);
    }

    log.End();
    return failures == 0 ? 0 : 1;
}
