#include "libImmCore/src/libBasics/piStreamArrayO.h"
#include "libImmCore/src/libMesh/piMeshSerialized.h"
#include "libImmExporter/src/document/layerModel3d.h"
#include "libImmExporter/src/toImmersive/toImmersiveLayerModel.h"
#include "libImmImporter/src/fromImmersive/fromImmersiveLayerModel.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include "libImmCore/src/libBasics/piTArray.h"
#include "libImmCore/src/libBasics/piImage.h"
#include "libImmExporter/src/document/sequence.h"
#include "libImmExporter/src/document/layerPaint.h"
#include "libImmExporter/src/document/layerPicture.h"
#include "libImmExporter/src/document/layerSpawnArea.h"
#include "libImmExporter/src/toImmersive/toImmersive.h"
#include "libImmImporter/src/fromImmersive/fromImmersive.h"
#include "libImmImporter/src/document/layerPicture.h"
#include "libImmImporter/src/document/layerPaint.h"

namespace {
using namespace ImmCore;
struct Fixture { const char* name; const wchar_t* label; int kind; int variant; };
constexpr Fixture Fixtures[] = {
    {"paint-segment", L"Paint Segment", 0, 1}, {"paint-circle", L"Paint Circle", 0, 2},
    {"paint-ellipse", L"Paint Ellipse", 0, 3}, {"paint-square", L"Paint Square", 0, 4},
    {"picture-flat", L"Flat Picture", 1, 0}, {"picture-equirect-mono", L"Mono Panorama", 1, 1},
    {"picture-equirect-stereo", L"Stereo Panorama", 1, 2}, {"picture-cube-cross", L"Cube Cross", 1, 3},
    {"picture-cube-strip", L"Cube Strip", 1, 4}
};
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
ImmExporter::Layer* AddLayer(ImmExporter::Sequence& sequence, ImmExporter::Layer::Type type,
    const wchar_t* name, const trans3d& transform = trans3d::identity())
{
    auto* layer = sequence.CreateLayer(sequence.GetRoot());
    Require(layer && layer->Init(type, name, true, transform, trans3d::identity(), 1, false, piTick(0), 0),
        "Initialize fixture layer");
    return layer;
}
void AddPaint(ImmExporter::Layer* layer, int brush)
{
    auto* paint = new ImmExporter::LayerPaint();
    paint->Init(); layer->SetImplementation(paint);
    auto* drawing = paint->CreateDrawing();
    Require(drawing && drawing->Init(1, false), "Initialize fixture drawing");
    auto* element = drawing->GetElement(0);
    Require(element && element->Init(4, static_cast<ImmExporter::Element::BrushSectionType>(brush),
        ImmExporter::Element::VisibilityType::Always), "Initialize fixture stroke");
    for (int point = 0; point < 4; ++point) {
        auto* value = element->GetPoint(point);
        value->mPos = vec3(-.75f + .5f * point, point == 1 ? .2f : point == 2 ? -.1f : 0, 0);
        value->mNor = vec3(0, 0, -1); value->mDir = vec3(0, 0, -1);
        value->mCol = vec3(.8f, .2f, .1f); value->mTra = 1; value->mWid = .2f;
        value->mLen = static_cast<float>(point) * .5f; value->mTim = static_cast<float>(point) / 3;
    }
    element->ComputeBoundingBox(); drawing->ComputeBoundingBox(); paint->AddFrame(0);
}
void AddPicture(ImmExporter::Layer* layer, int variant)
{
    auto* picture = new ImmExporter::LayerPicture();
    Require(picture->Init(), "Initialize fixture picture"); layer->SetImplementation(picture);
    picture->SetType(static_cast<ImmExporter::LayerPicture::PictureType>(variant));
    const int width = variant == 4 ? 16 : 64;
    const int height = variant == 1 ? 32 : variant == 3 ? 48 : variant == 4 ? 96 : 64;
    piImage image;
    const piImage::Format format = piImage::FORMAT_I_RGBA;
    Require(image.Init(piImage::TYPE_2D, width, height, 1, 1, &format), "Allocate fixture image");
    auto* pixels = static_cast<unsigned char*>(image.GetData(0));
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const bool rightEye = variant == 2 && y >= height / 2;
        const unsigned char red = ((x / 8 + y / 8) & 1) ? 192 : 96;
        const int pixel = 4 * (y * width + x);
        pixels[pixel] = rightEye ? 16 : red;
        pixels[pixel + 1] = rightEye ? red : 32; pixels[pixel + 2] = 16; pixels[pixel + 3] = 255;
    }
    if (variant >= 3) {
        const int crossX[] = {2, 0, 1, 1, 3, 1}, crossY[] = {1, 1, 0, 2, 1, 1};
        for (int face = 0; face < 6; ++face)
            for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
                const int px = variant == 3 ? crossX[face] * 16 + x : x;
                const int py = variant == 3 ? crossY[face] * 16 + y : face * 16 + y;
                pixels[4 * (py * width + px)] = static_cast<unsigned char>((face + 1) * 32);
            }
    }
    Require(picture->AssignAsset(&image, false), "Assign fixture picture pixels"); image.Free();
}
void VerifyImport(const std::filesystem::path& path, const Fixture& fixture, piLog& log)
{
    for (auto technique : {ImmImporter::Drawing::Static, ImmImporter::Drawing::Pretessellated}) {
        ImmImporter::Sequence sequence;
        // The importer retains the path while its asynchronous asset thread runs.
        const auto filename = path.wstring();
        Require(ImmImporter::ImportFromDisk(&sequence, &log, filename.c_str(),
            ImmImporter::Drawing::ColorSpace::Linear, technique), "Import generated fixture");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (ImmImporter::IsLoadingAsync() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(!ImmImporter::IsLoadingAsync(), "Fixture import timed out");
        int content = 0;
        Require(sequence.Recurse([&](ImmImporter::Layer* layer, int, int, bool) {
            if (layer->GetType() == ImmImporter::Layer::Type::Group ||
                layer->GetType() == ImmImporter::Layer::Type::SpawnArea) return true;
            ++content;
            Require(layer->GetLoaded() && layer->GetImplementation(), "Fixture asset did not decode");
            if (fixture.kind == 0) {
                Require(layer->GetType() == ImmImporter::Layer::Type::Paint, "Wrong imported paint type");
                auto* paint = static_cast<ImmImporter::LayerPaint*>(layer->GetImplementation());
                Require(paint->GetNumDrawings() == 1 && paint->GetDrawing(0)->GetLoaded(), "Missing fixture drawing");
                Require(paint->GetDrawing(0)->GetNumStrokes() == 1 &&
                    paint->GetDrawing(0)->GetNumGeometryChunks(fixture.variant) > 0, "Missing fixture brush geometry");
            }
            else {
                Require(layer->GetType() == ImmImporter::Layer::Type::Picture, "Wrong imported picture type");
                auto* picture = static_cast<ImmImporter::LayerPicture*>(layer->GetImplementation());
                Require(int(picture->GetType()) == fixture.variant, "Wrong imported picture format");
                const int width = fixture.variant == 4 ? 16 : 64;
                const int height = fixture.variant == 1 ? 32 : fixture.variant == 3 ? 48 : fixture.variant == 4 ? 96 : 64;
                Require(picture->GetImage()->GetXRes() == width && picture->GetImage()->GetYRes() == height,
                    "Wrong imported picture dimensions");
            }
            return true;
        }, false, false, false, false), "Traverse generated fixture");
        Require(content == 1 && sequence.GetInitialSpawnArea(), "Unexpected fixture scene topology");
        sequence.Deinit(&log);
    }
}
}

void VerifyModelSerialization(piLog& log)
{
    for (int layout = 0; layout < 5; ++layout) {
        ImmImporter::LayerModel model;
        Require(model.Init(false, ImmImporter::LayerModel::ShadingModel::Unlit), "Initialize source model");
    auto* mesh = model.GetMesh();
    piMesh::VertexFormat formats[2] = {};
    formats[0].mStride = (layout == 1 ? 11 : layout == 4 ? 10 : 7) * sizeof(float);
    formats[0].mNumElems = layout == 4 ? 3 : 2;
    formats[0].mElems[0] = {3, piMesh::VertexElemDataType::Float, false, 0};
    formats[0].mElems[1] = {4, piMesh::VertexElemDataType::Float, false, 3 * sizeof(float)};
    if (layout == 2) {
        formats[0].mStride = 3 * sizeof(float); formats[0].mNumElems = 1;
        formats[1].mStride = 4 * sizeof(float); formats[1].mNumElems = 1;
        formats[1].mElems[0] = {4, piMesh::VertexElemDataType::Float, false, 0};
    }
    if (layout == 3) {
        formats[0].mStride = 16;
        formats[0].mElems[1] = {4, piMesh::VertexElemDataType::UByte, true, 12};
    }
    if (layout == 4) formats[0].mElems[2] = {3, piMesh::VertexElemDataType::Float, false, 7 * sizeof(float)};
    if (!mesh->Init(layout == 2 ? 2 : 1, 4, formats, piMesh::Type::Polys, 1, 2))
        throw std::runtime_error("Allocate model input fixture");
    float vertices[4][11] = {
        {-.6f,-.5f,.5f, .5f,.5f,.5f,1, 0,0,1,99}, {.6f,-.5f,.5f, .5f,.5f,.5f,1, 0,0,1,99},
        {.6f,.5f,.5f, .5f,.5f,.5f,1, 0,0,1,99}, {-.6f,.5f,.5f, .5f,.5f,.5f,1, 0,0,1,99}};
    for (uint32_t vertex = 0; vertex < 4; ++vertex) {
        if (layout == 3) {
            unsigned char packed[16] = {};
            std::memcpy(packed, vertices[vertex], 12);
            packed[12] = packed[13] = packed[14] = 128; packed[15] = 255;
            mesh->SetVertex(0, vertex, packed);
        }
        else {
            mesh->SetVertex(0, vertex, vertices[vertex]);
            if (layout == 2) mesh->SetVertex(1, vertex, vertices[vertex] + 3);
        }
    }
    mesh->SetTriangle(0, 0, 0, 1, 2); mesh->SetTriangle(0, 1, 0, 2, 3); mesh->CalcBBox(0, 0);

        for (int shading = 0; shading < 2; ++shading)
        for (bool wireframe : {false, true}) {
            ImmExporter::LayerModel exported;
            Require(exported.Init() && exported.AssignAsset(mesh, false), "Clone exported model");
            exported.SetShadingModel(static_cast<ImmExporter::LayerModel::ShadingModel>(shading));
            exported.SetRenderWireframe(wireframe);
            piTArray<uint8_t> metadata, asset;
            Require(metadata.Init(0, false) && asset.Init(0, false), "Allocate model serialization buffers");
            piOStreamArray metadataOut(&metadata), assetOut(&asset);
            Require(ImmExporter::tiLayerModel::ExportData(&metadataOut, &exported) &&
                ImmExporter::tiLayerModel::ExportAsset(&assetOut, &exported), "Serialize model layer");
            const uint32_t unsupportedVersion = 2, supportedVersion = 1;
            std::memcpy(metadata.GetAddress(0), &unsupportedVersion, 4);
            metadata.SetLength(0);
            piIStreamArray rejectedMetadata(&metadata);
            Require(!ImmImporter::fiLayerModel::ReadData(&rejectedMetadata, &log), "Reject unsupported model metadata");
            std::memcpy(metadata.GetAddress(0), &supportedVersion, 4);
            const uint64_t assetBytes = asset.GetLength();
            Require(assetBytes > 8 && piMeshValidateSerialized(asset.GetAddress(8), assetBytes - 8), "Validate exported mesh");
            Require(!piMeshValidateSerialized(asset.GetAddress(8), assetBytes - 9), "Reject truncated mesh");
            const uint32_t invalidStreams = piMesh_MAXVERTEXARRAYS + 1;
            uint32_t savedStreams;
            std::memcpy(&savedStreams, asset.GetAddress(8 + 28), 4);
            std::memcpy(asset.GetAddress(8 + 28), &invalidStreams, 4);
            Require(!piMeshValidateSerialized(asset.GetAddress(8), assetBytes - 8), "Reject excessive mesh streams");
            std::memcpy(asset.GetAddress(8 + 28), &savedStreams, 4);
            metadata.SetLength(0); asset.SetLength(0);
            piIStreamArray metadataIn(&metadata), assetIn(&asset);
            auto* decoded = static_cast<ImmImporter::LayerModel*>(ImmImporter::fiLayerModel::ReadData(&metadataIn, &log));
            Require(decoded && ImmImporter::fiLayerModel::ReadAsset(decoded, &assetIn, &log), "Deserialize model layer");
            const uint64_t excessiveSize = 256ull * 1024 * 1024 + 1, originalSize = assetBytes - 8;
            std::memcpy(asset.GetAddress(0), &excessiveSize, 8);
            asset.SetLength(0);
            piIStreamArray rejectedAsset(&asset);
            Require(!ImmImporter::fiLayerModel::ReadAsset(decoded, &rejectedAsset, &log), "Reject excessive model asset size");
            std::memcpy(asset.GetAddress(0), &originalSize, 8);
            Require(int(decoded->GetShadingModel()) == shading && decoded->GetRenderWireframe() == wireframe, "Roundtrip model flags");
            auto* result = decoded->GetMesh();
            Require(std::memcmp(&result->mBBox, &mesh->mBBox, sizeof(mesh->mBBox)) == 0, "Roundtrip model bounds");
            std::printf("IMM_MODEL_LAYER_IO layout=%d streams=%d expectedStreams=%d indexArrays=%d faces=%u\n",
                layout, result->mVertexData.mNumVertexArrays, mesh->mVertexData.mNumVertexArrays,
                result->mFaceData.mNumIndexArrays, result->mFaceData.mIndexArray[0].mNum);
            Require(result->mVertexData.mNumVertexArrays == mesh->mVertexData.mNumVertexArrays &&
                result->mFaceData.mNumIndexArrays == 1 && result->mFaceData.mIndexArray[0].mNum == 2,
                "Roundtrip mesh stream and triangle counts");
            for (int stream = 0; stream < mesh->mVertexData.mNumVertexArrays; ++stream) {
                const auto& expected = mesh->mVertexData.mVertexArray[stream];
                const auto& actual = result->mVertexData.mVertexArray[stream];
                Require(actual.mNum == expected.mNum && actual.mFormat.mStride == expected.mFormat.mStride &&
                    actual.mFormat.mNumElems == expected.mFormat.mNumElems &&
                    std::memcmp(actual.mBuffer, expected.mBuffer, expected.mNum * expected.mFormat.mStride) == 0,
                    "Roundtrip mesh vertex bytes");
                for (int element = 0; element < expected.mFormat.mNumElems; ++element) {
                    const auto& a = actual.mFormat.mElems[element]; const auto& e = expected.mFormat.mElems[element];
                    Require(a.mType == e.mType && a.mNumComponents == e.mNumComponents &&
                        a.mNormalize == e.mNormalize && a.mOffset == e.mOffset, "Roundtrip vertex attributes");
                }
            }
            Require(std::memcmp(result->mFaceData.mIndexArray[0].mBuffer, mesh->mFaceData.mIndexArray[0].mBuffer,
                2 * sizeof(piMesh::Face32)) == 0, "Roundtrip mesh triangle bytes");
            decoded->Deinit(); decoded->Deinit(); delete decoded;
            exported.Deinit(); metadata.End(); asset.End();
        }
        model.Deinit();
    }
    std::puts("IMM_MODEL_LAYER_IO PASS five mesh layouts and shading/wireframe flags with bounded decoding");
}
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) {
        std::fprintf(stderr, "Usage: imm_urp_content_fixtures <output-directory> [reference-directory]\n"); return 2;
    }
    try {
        const auto directory = std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(directory);
        piLog log;
        Require(log.Init((directory / "generation.log").wstring().c_str(), 0), "Initialize fixture log");
        VerifyModelSerialization(log);
        std::ofstream manifest(directory / "manifest.json", std::ios::binary);
        manifest << "{\"schema\":\"imm-urp-content-fixtures-v1\",\"fixtures\":[\n";
        bool first = true;
        for (const auto& fixture : Fixtures) {
            ImmExporter::Sequence sequence;
            const ImmExporter::Sequence::Requirements requirements = {};
            Require(sequence.Init(ImmExporter::Sequence::Type::Still, 0, requirements, vec3(0.0f), 30), "Initialize fixture sequence");
            auto* spawnLayer = AddLayer(sequence, ImmExporter::Layer::Type::SpawnArea, L"Fixture Camera", trans3d::translate(0, 0, 3));
            auto* spawn = new ImmExporter::LayerSpawnArea();
            Require(spawn->Init(), "Initialize fixture spawn area"); spawnLayer->SetImplementation(spawn);
            ImmExporter::LayerSpawnArea::Volume volume = {};
            volume.mType = ImmExporter::LayerSpawnArea::Volume::Type::Sphere;
            volume.mShape.mSphere = vec4(0.0f, 1.0f, 0.0f, 2.0f);
            volume.mAllowTranslationX = volume.mAllowTranslationY = volume.mAllowTranslationZ = true;
            spawn->SetVolume(volume);
            spawn->SetTracking(ImmExporter::LayerSpawnArea::TrackingLevel::Floor);
            sequence.SetInitialSpawnArea(spawnLayer);
            const auto type = fixture.kind == 0 ? ImmExporter::Layer::Type::Paint : ImmExporter::Layer::Type::Picture;
            auto* layer = AddLayer(sequence, type, fixture.label);
            if (fixture.kind == 0) AddPaint(layer, fixture.variant);
            else AddPicture(layer, fixture.variant);
            const std::string filename = std::string(fixture.name) + ".imm";
            const auto path = directory / filename;
            Require(ImmExporter::ExportToFile(path.string().c_str(), &sequence, 128000, ImmExporter::tiLayerSound::AudioType::OPUS), "Export fixture");
            sequence.Deinit(); VerifyImport(path, fixture, log);
            if (!first) manifest << ",\n";
            first = false;
            manifest << "{\"file\":\"" << filename << "\",\"kind\":\"" <<
                (fixture.kind == 0 ? "paint" : "picture") <<
                "\",\"variant\":" << fixture.variant << ",\"depthRole\":\"" <<
                (fixture.kind == 1 && fixture.variant != 0 ? "backdrop" : "surface") << "\"}";
            std::printf("IMM_URP_CONTENT_FIXTURES PASS %s both paint storage imports\n", filename.c_str());
        }
        manifest << "\n]}\n"; manifest.close(); Require(!manifest.fail(), "Write fixture manifest"); log.End();
        if (argc == 3) {
            const auto reference = std::filesystem::absolute(argv[2]);
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                const auto name = entry.path().filename();
                if (entry.path().extension() != ".imm" && name != "manifest.json") continue;
                std::ifstream actual(entry.path(), std::ios::binary), expected(reference / name, std::ios::binary);
                Require(actual.good() && expected.good(), "Missing reference fixture");
                Require(std::equal(std::istreambuf_iterator<char>(actual), std::istreambuf_iterator<char>(),
                    std::istreambuf_iterator<char>(expected), std::istreambuf_iterator<char>()), "Fixture differs from reference");
            }
            std::puts("IMM_URP_CONTENT_FIXTURES PASS deterministic committed fixtures");
        }
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "IMM_URP_CONTENT_FIXTURES FAIL %s\n", error.what()); return 1; }
}
