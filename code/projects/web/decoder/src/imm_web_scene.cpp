#include "imm_web_decoder.h"

#include "appImmStrokeReader/src/strokeStore.h"
#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piStr.h"
#include "libImmCore/src/libBasics/piTArray.h"
#include "libImmImporter/src/document/sequence.h"
#include "libImmImporter/src/document/layerSpawnArea.h"
#include "libImmImporter/src/document/layerSound.h"
#include "libImmImporter/src/fromImmersive/fromImmersive.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    std::unique_ptr<ImmStrokeReader::StrokeStore> gStore;
    std::unique_ptr<ImmImporter::Sequence> gSequence;
    std::unique_ptr<ImmCore::piLog> gLog;
    const uint8_t* gSource = nullptr;
    size_t gSourceSize = 0;
    float gBackgroundColor[3] = {0.0f, 0.0f, 0.0f};
    constexpr uint32_t kTicksPerSecond = 12600u;
    constexpr uint32_t kNoContentLayer = std::numeric_limits<uint32_t>::max();

    struct StoredTimelineKey
    {
        uint32_t property;
        uint32_t interpolation;
        int64_t timeTicks;
        ImmImporter::Layer::AnimValue value;
    };

    struct StoredTimelineLayer
    {
        uint32_t id;
        int32_t parentId;
        uint32_t type;
        bool visible;
        bool timeline;
        bool spawnFloorLevel;
        float opacity;
        uint32_t maxRepeatCount;
        int64_t durationTicks;
        uint32_t contentLayerIndex;
        std::string name;
        ImmCore::trans3d local;
        ImmCore::trans3d world;
        ImmCore::trans3d pivot;
        ImmWebKeepAliveInfo keepAlive;
        std::vector<StoredTimelineKey> keys;
    };

    struct StoredChapter
    {
        int64_t startTicks;
        int64_t endTicks;
        uint32_t markerAction;
    };

    struct StoredSound
    {
        ImmWebSoundInfo info{};
        std::vector<uint8_t> bytes;
    };

    std::vector<StoredTimelineLayer> gTimelineLayers;
    std::vector<StoredChapter> gChapters;
    std::vector<StoredSound> gSounds;
    bool gAnimateOnStart = false;
    int64_t gDurationTicks = 0;

    void setError(ImmWebError* error, ImmWebStatus status, const char* message)
    {
        if (error == nullptr)
        {
            return;
        }
        std::memset(error, 0, sizeof(*error));
        error->status = static_cast<uint32_t>(status);
        const size_t copyLength = std::min(std::strlen(message), static_cast<size_t>(IMM_WEB_ERROR_MESSAGE_CAPACITY - 1u));
        std::memcpy(error->message, message, copyLength);
    }

    bool makeSourceArray(ImmCore::piTArray<uint8_t>* data)
    {
        if (data == nullptr || gSource == nullptr || gSourceSize == 0 || !data->Init(0u, false))
            return false;
        data->Set(const_cast<uint8_t*>(gSource), static_cast<uint64_t>(gSourceSize));
        return true;
    }

    ImmWebTransform convertTransform(const ImmStrokeReader::StrokeLayerTransformC& source)
    {
        ImmWebTransform result{};
        std::copy(source.rotation, source.rotation + 4, result.rotation);
        result.scale = source.scale;
        result.flip = static_cast<uint32_t>(source.flip);
        std::copy(source.translation, source.translation + 3, result.translation);
        return result;
    }

    ImmWebTransform convertTransform(const ImmCore::trans3d& source)
    {
        ImmWebTransform result{};
        result.rotation[0] = static_cast<float>(source.mRotation.x);
        result.rotation[1] = static_cast<float>(source.mRotation.y);
        result.rotation[2] = static_cast<float>(source.mRotation.z);
        result.rotation[3] = static_cast<float>(source.mRotation.w);
        result.scale = static_cast<float>(source.mScale);
        result.flip = static_cast<uint32_t>(source.mFlip);
        result.translation[0] = static_cast<float>(source.mTranslation.x);
        result.translation[1] = static_cast<float>(source.mTranslation.y);
        result.translation[2] = static_cast<float>(source.mTranslation.z);
        return result;
    }

    uint32_t findContentLayerIndex(const ImmStrokeReader::StrokeStore& store, uint32_t layerId)
    {
        const int layerCount = store.GetLayerCount();
        for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
        {
            ImmStrokeReader::StrokeLayerInfoC info{};
            if (store.GetLayerInfo(layerIndex, &info) && static_cast<uint32_t>(info.id) == layerId)
            {
                return static_cast<uint32_t>(layerIndex);
            }
        }
        return kNoContentLayer;
    }

    void capturePlaybackDocument(ImmImporter::Sequence& sequence, const ImmStrokeReader::StrokeStore& store)
    {
        gTimelineLayers.clear();
        gChapters.clear();
        gSounds.clear();
        gAnimateOnStart = sequence.GetAnimateOnStart();

        ImmImporter::Layer* root = sequence.GetRoot();
        gDurationTicks = root == nullptr ? 0 : ImmCore::piTick::CastInt(root->GetDuration());
        sequence.Recurse(
            [&store](ImmImporter::Layer* layer, int, int, bool) {
                StoredTimelineLayer stored{};
                stored.id = layer->GetID();
                stored.parentId = layer->GetParent() == nullptr
                    ? -1
                    : static_cast<int32_t>(layer->GetParent()->GetID());
                stored.type = static_cast<uint32_t>(layer->GetType());
                stored.visible = layer->GetVisible();
                stored.timeline = layer->GetIsTimeline();
                stored.spawnFloorLevel = false;
                if (layer->GetType() == ImmImporter::Layer::Type::SpawnArea)
                {
                    const auto* spawn = static_cast<const ImmImporter::LayerSpawnArea*>(layer->GetImplementation());
                    stored.spawnFloorLevel = spawn != nullptr &&
                        spawn->GetTracking() == ImmImporter::LayerSpawnArea::TrackingLevel::Floor;
                }
                stored.opacity = layer->GetOpacity();
                stored.maxRepeatCount = layer->GetMaxRepeatCount();
                stored.durationTicks = ImmCore::piTick::CastInt(layer->GetDuration());
                stored.contentLayerIndex = findContentLayerIndex(store, stored.id);
                stored.local = layer->GetTransform();
                stored.world = layer->GetTransformToWorld();
                stored.pivot = layer->GetPivot();
                if (layer->GetType() == ImmImporter::Layer::Type::Sound)
                {
                    const auto* sound = static_cast<const ImmImporter::LayerSound*>(layer->GetImplementation());
                    if (sound != nullptr && sound->GetEncodedSound().size() <= std::numeric_limits<uint32_t>::max())
                    {
                        StoredSound storedSound{};
                        storedSound.info.layer_id = layer->GetID();
                        storedSound.info.type = static_cast<uint32_t>(sound->GetType());
                        storedSound.info.asset_format = sound->GetEncodedFormat();
                        storedSound.info.channel_count = sound->GetEncodedChannels();
                        storedSound.info.looping = sound->GetLooping() ? 1u : 0u;
                        storedSound.info.play_on_load = sound->GetPlaying() ? 1u : 0u;
                        storedSound.info.gain = sound->GetGain();
                        ImmImporter::LayerSound::AttenuationParameters attenuation{};
                        sound->GetAttenuation(&attenuation);
                        storedSound.info.attenuation_type = static_cast<uint32_t>(attenuation.mType);
                        storedSound.info.attenuation_min = attenuation.mMin;
                        storedSound.info.attenuation_max = attenuation.mMax;
                        ImmImporter::LayerSound::ModifierParameters modifier{};
                        sound->GetModifier(&modifier);
                        storedSound.info.modifier_type = static_cast<uint32_t>(modifier.mType);
                        if (modifier.mType == ImmImporter::LayerSound::ModifierType::DirectionalCone)
                        {
                            storedSound.info.modifier_parameters[0] = modifier.mParams.mDirectionalCone.mAngleIn;
                            storedSound.info.modifier_parameters[1] = modifier.mParams.mDirectionalCone.mAngleBand;
                            storedSound.info.modifier_parameters[2] = modifier.mParams.mDirectionalCone.mAttenOut;
                        }
                        else if (modifier.mType == ImmImporter::LayerSound::ModifierType::DirectionalFrus)
                        {
                            storedSound.info.modifier_parameters[0] = modifier.mParams.mDirectionalFrus.mAngleInX;
                            storedSound.info.modifier_parameters[1] = modifier.mParams.mDirectionalFrus.mAngleInY;
                            storedSound.info.modifier_parameters[2] = modifier.mParams.mDirectionalFrus.mAngleBand;
                            storedSound.info.modifier_parameters[3] = modifier.mParams.mDirectionalFrus.mAttenOut;
                        }
                        storedSound.bytes = sound->GetEncodedSound();
                        storedSound.info.data_size = static_cast<uint32_t>(storedSound.bytes.size());
                        gSounds.push_back(std::move(storedSound));
                    }
                }
                const ImmImporter::KeepAlive* keepAlive = layer->GetKeepAlive();
                stored.keepAlive.type = static_cast<uint32_t>(keepAlive->GetType());
                if (keepAlive->GetType() == ImmImporter::KeepAlive::KeepAliveType::Wiggle)
                {
                    const ImmImporter::KeepAlive::Wiggle* wiggle = keepAlive->GetDataWiggle();
                    stored.keepAlive.parameters[0] = wiggle->mFrequency;
                    stored.keepAlive.parameters[1] = wiggle->mSpeed;
                    stored.keepAlive.parameters[2] = wiggle->mAmplitude;
                }
                else if (keepAlive->GetType() == ImmImporter::KeepAlive::KeepAliveType::Blink)
                {
                    const ImmImporter::KeepAlive::Blink* blink = keepAlive->GetDataBlink();
                    stored.keepAlive.waveform = static_cast<uint32_t>(blink->mWaveForm);
                    stored.keepAlive.parameters[0] = blink->mSpeed;
                    stored.keepAlive.parameters[1] = blink->mMinOut;
                    stored.keepAlive.parameters[2] = blink->mMaxOut;
                    stored.keepAlive.parameters[3] = blink->mMinIn;
                    stored.keepAlive.parameters[4] = blink->mMaxIn;
                }

                char* name = ImmCore::piws2str(layer->GetName().GetS());
                if (name != nullptr)
                {
                    stored.name = name;
                    std::free(name);
                }

                for (uint32_t property = 0;
                     property < static_cast<uint32_t>(ImmImporter::Layer::AnimProperty::MAX);
                     ++property)
                {
                    const auto animationProperty = static_cast<ImmImporter::Layer::AnimProperty>(property);
                    const uint32_t keyCount = layer->GetNumAnimKeys(animationProperty);
                    for (uint32_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                    {
                        const ImmImporter::Layer::AnimKey* key = layer->GetAnimKey(animationProperty, keyIndex);
                        if (key != nullptr)
                        {
                            stored.keys.push_back({
                                property,
                                static_cast<uint32_t>(key->mInterpolation),
                                ImmCore::piTick::CastInt(key->mTime),
                                key->mValue,
                            });
                        }
                    }
                }
                gTimelineLayers.push_back(std::move(stored));
                return true;
            },
            false,
            false,
            false,
            false);

        if (root == nullptr)
        {
            return;
        }

        std::vector<std::pair<int64_t, uint32_t>> markers;
        const uint32_t actionCount = root->GetNumAnimKeys(ImmImporter::Layer::AnimProperty::Action);
        bool hasPlayMarker = false;
        for (uint32_t index = 0; index < actionCount; ++index)
        {
            const ImmImporter::Layer::AnimKey* key = root->GetAnimKey(ImmImporter::Layer::AnimProperty::Action, index);
            if (key != nullptr &&
                static_cast<ImmImporter::Layer::AnimAction>(key->mValue.mInt) == ImmImporter::Layer::AnimAction::Play)
            {
                hasPlayMarker = true;
                break;
            }
        }

        markers.emplace_back(0, static_cast<uint32_t>(ImmImporter::Layer::AnimAction::Play));
        for (uint32_t index = 0; index < actionCount; ++index)
        {
            const ImmImporter::Layer::AnimKey* key = root->GetAnimKey(ImmImporter::Layer::AnimProperty::Action, index);
            if (key == nullptr)
            {
                continue;
            }
            const auto action = static_cast<ImmImporter::Layer::AnimAction>(key->mValue.mInt);
            if ((hasPlayMarker && action == ImmImporter::Layer::AnimAction::Play) ||
                (!hasPlayMarker && action == ImmImporter::Layer::AnimAction::Stop))
            {
                const int64_t markerTime = ImmCore::piTick::CastInt(key->mTime) + (hasPlayMarker ? 0 : 1);
                if (markerTime > 0)
                {
                    markers.emplace_back(markerTime, static_cast<uint32_t>(action));
                }
            }
        }

        std::sort(markers.begin(), markers.end());
        markers.erase(std::unique(markers.begin(), markers.end()), markers.end());
        if (!markers.empty())
        {
            gDurationTicks = std::max(gDurationTicks, markers.back().first);
        }
        for (size_t index = 0; index < markers.size(); ++index)
        {
            const int64_t start = markers[index].first;
            const int64_t end = index + 1 < markers.size()
                ? markers[index + 1].first
                : std::max(start, gDurationTicks);
            gChapters.push_back({start, end, markers[index].second});
        }
    }
}

static_assert(sizeof(ImmWebPlaybackInfo) == 32u, "ImmWebPlaybackInfo ABI changed");
static_assert(sizeof(ImmWebTimelineLayerInfo) == 296u, "ImmWebTimelineLayerInfo ABI changed");
static_assert(sizeof(ImmWebAnimationKey) == 80u, "ImmWebAnimationKey ABI changed");
static_assert(sizeof(ImmWebChapterInfo) == 24u, "ImmWebChapterInfo ABI changed");
static_assert(sizeof(ImmWebKeepAliveInfo) == 32u, "ImmWebKeepAliveInfo ABI changed");
static_assert(sizeof(ImmWebSoundInfo) == 64u, "ImmWebSoundInfo ABI changed");

extern "C" ImmWebStatus imm_web_decode_scene(
    const uint8_t* source,
    size_t sourceSize,
    ImmWebError* outError)
{
    imm_web_release_scene();
    if (source == nullptr || sourceSize == 0u)
    {
        setError(outError, IMM_WEB_STATUS_INVALID_ARGUMENT, "Scene source is required");
        return IMM_WEB_STATUS_INVALID_ARGUMENT;
    }

    ImmCore::piTArray<uint8_t> data;
    if (!data.Init(0u, false))
    {
        setError(outError, IMM_WEB_STATUS_SCENE_DECODE_FAILED, "Could not initialize scene input");
        return IMM_WEB_STATUS_SCENE_DECODE_FAILED;
    }
    data.Set(const_cast<uint8_t*>(source), static_cast<uint64_t>(sourceSize));

    ImmCore::piLog log;
    ImmImporter::Sequence sequence;
    auto store = std::make_unique<ImmStrokeReader::StrokeStore>();
    const bool decoded = ImmImporter::ImportFromMemory(
        &data,
        &sequence,
        &log,
        ImmImporter::Drawing::ColorSpace::Gamma,
        ImmImporter::Drawing::PaintRenderingTechnique::Static,
        store.get());
    if (!decoded)
    {
        setError(outError, IMM_WEB_STATUS_SCENE_DECODE_FAILED, "Native IMM scene importer failed");
        return IMM_WEB_STATUS_SCENE_DECODE_FAILED;
    }

    const ImmCore::vec3 background = sequence.GetBackgroundColor();
    gBackgroundColor[0] = background.x;
    gBackgroundColor[1] = background.y;
    gBackgroundColor[2] = background.z;
    capturePlaybackDocument(sequence, *store);
    sequence.Deinit(&log);
    gStore = std::move(store);
    setError(outError, IMM_WEB_STATUS_OK, "");
    return IMM_WEB_STATUS_OK;
}

extern "C" ImmWebStatus imm_web_open_scene_metadata(
    const uint8_t* source,
    size_t sourceSize,
    ImmWebError* outError)
{
    imm_web_release_scene();
    if (source == nullptr || sourceSize == 0u)
    {
        setError(outError, IMM_WEB_STATUS_INVALID_ARGUMENT, "Scene source is required");
        return IMM_WEB_STATUS_INVALID_ARGUMENT;
    }

    gSource = source;
    gSourceSize = sourceSize;
    ImmCore::piTArray<uint8_t> data;
    if (!makeSourceArray(&data))
    {
        setError(outError, IMM_WEB_STATUS_SCENE_DECODE_FAILED, "Could not initialize scene input");
        return IMM_WEB_STATUS_SCENE_DECODE_FAILED;
    }

    auto sequence = std::make_unique<ImmImporter::Sequence>();
    auto log = std::make_unique<ImmCore::piLog>();
    auto store = std::make_unique<ImmStrokeReader::StrokeStore>();
    if (!ImmImporter::ImportMetadataFromMemory(
            &data,
            sequence.get(),
            log.get(),
            ImmImporter::Drawing::ColorSpace::Gamma,
            ImmImporter::Drawing::PaintRenderingTechnique::Static,
            store.get()))
    {
        sequence->Deinit(log.get());
        setError(outError, IMM_WEB_STATUS_SCENE_DECODE_FAILED, "Native IMM metadata importer failed");
        return IMM_WEB_STATUS_SCENE_DECODE_FAILED;
    }

    const ImmCore::vec3 background = sequence->GetBackgroundColor();
    gBackgroundColor[0] = background.x;
    gBackgroundColor[1] = background.y;
    gBackgroundColor[2] = background.z;
    capturePlaybackDocument(*sequence, *store);
    gSequence = std::move(sequence);
    gLog = std::move(log);
    gStore = std::move(store);
    setError(outError, IMM_WEB_STATUS_OK, "");
    return IMM_WEB_STATUS_OK;
}

extern "C" ImmWebStatus imm_web_decode_drawing(
    uint32_t layerId,
    uint32_t drawingId,
    ImmWebError* outError)
{
    if (gSequence == nullptr || gLog == nullptr || gStore == nullptr)
    {
        setError(outError, IMM_WEB_STATUS_INVALID_ARGUMENT, "No staged scene is open");
        return IMM_WEB_STATUS_INVALID_ARGUMENT;
    }
    ImmCore::piTArray<uint8_t> data;
    if (!makeSourceArray(&data) || !ImmImporter::DecodeDrawingFromMemory(
            &data,
            gSequence.get(),
            gLog.get(),
            layerId,
            drawingId,
            ImmImporter::Drawing::ColorSpace::Gamma,
            ImmImporter::Drawing::PaintRenderingTechnique::Static,
            gStore.get()))
    {
        setError(outError, IMM_WEB_STATUS_SCENE_DECODE_FAILED, "Native IMM drawing decode failed");
        return IMM_WEB_STATUS_SCENE_DECODE_FAILED;
    }
    setError(outError, IMM_WEB_STATUS_OK, "");
    return IMM_WEB_STATUS_OK;
}

extern "C" ImmWebStatus imm_web_decode_layer_asset(
    uint32_t layerId,
    ImmWebError* outError)
{
    if (gSequence == nullptr || gLog == nullptr || gStore == nullptr)
    {
        setError(outError, IMM_WEB_STATUS_INVALID_ARGUMENT, "No staged scene is open");
        return IMM_WEB_STATUS_INVALID_ARGUMENT;
    }
    ImmCore::piTArray<uint8_t> data;
    if (!makeSourceArray(&data) || !ImmImporter::DecodeLayerAssetFromMemory(
            &data,
            gSequence.get(),
            gLog.get(),
            layerId,
            ImmImporter::Drawing::ColorSpace::Gamma,
            ImmImporter::Drawing::PaintRenderingTechnique::Static,
            gStore.get()))
    {
        setError(outError, IMM_WEB_STATUS_SCENE_DECODE_FAILED, "Native IMM layer asset decode failed");
        return IMM_WEB_STATUS_SCENE_DECODE_FAILED;
    }
    capturePlaybackDocument(*gSequence, *gStore);
    setError(outError, IMM_WEB_STATUS_OK, "");
    return IMM_WEB_STATUS_OK;
}

extern "C" ImmWebStatus imm_web_decode_open_scene_eager(ImmWebError* outError)
{
    if (gSource == nullptr || gSourceSize == 0)
    {
        setError(outError, IMM_WEB_STATUS_INVALID_ARGUMENT, "No staged scene source is open");
        return IMM_WEB_STATUS_INVALID_ARGUMENT;
    }
    return imm_web_decode_scene(gSource, gSourceSize, outError);
}

extern "C" void imm_web_release_scene(void)
{
    if (gSequence != nullptr && gLog != nullptr)
        gSequence->Deinit(gLog.get());
    gStore.reset();
    gSequence.reset();
    gLog.reset();
    gSource = nullptr;
    gSourceSize = 0;
    gBackgroundColor[0] = 0.0f;
    gBackgroundColor[1] = 0.0f;
    gBackgroundColor[2] = 0.0f;
    gTimelineLayers.clear();
    gChapters.clear();
    gSounds.clear();
    gAnimateOnStart = false;
    gDurationTicks = 0;
}

extern "C" uint32_t imm_web_get_layer_count(void)
{
    return gStore == nullptr ? 0u : static_cast<uint32_t>(gStore->GetLayerCount());
}

extern "C" uint32_t imm_web_get_background_color(float* outRgb, uint32_t floatCapacity)
{
    if (outRgb == nullptr || floatCapacity < 3u)
    {
        return 0u;
    }
    std::copy(gBackgroundColor, gBackgroundColor + 3, outRgb);
    return 3u;
}

extern "C" uint32_t imm_web_get_layer_info(uint32_t layerIndex, ImmWebLayerInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    ImmStrokeReader::StrokeLayerInfoC source{};
    if (!gStore->GetLayerInfo(static_cast<int>(layerIndex), &source))
    {
        return 0u;
    }
    std::memset(outInfo, 0, sizeof(*outInfo));
    outInfo->id = static_cast<uint32_t>(source.id);
    outInfo->type = static_cast<uint32_t>(source.type);
    outInfo->drawing_count = static_cast<uint32_t>(source.numDrawings);
    outInfo->visible = static_cast<uint32_t>(source.visible);
    outInfo->opacity = source.opacity;
    outInfo->default_spawn = static_cast<uint32_t>(source.isDefaultSpawn);
    std::memcpy(outInfo->name, source.name, sizeof(outInfo->name));
    return 1u;
}

extern "C" uint32_t imm_web_get_layer_transforms(
    uint32_t layerIndex,
    ImmWebTransform* outLocal,
    ImmWebTransform* outWorld,
    ImmWebTransform* outPivot)
{
    if (gStore == nullptr || outLocal == nullptr || outWorld == nullptr || outPivot == nullptr)
    {
        return 0u;
    }
    ImmStrokeReader::StrokeLayerTransformC local{};
    ImmStrokeReader::StrokeLayerTransformC world{};
    if (!gStore->GetLayerTransform(static_cast<int>(layerIndex), &local, &world))
    {
        return 0u;
    }
    ImmStrokeReader::StrokeLayerInfoC layer{};
    if (!gStore->GetLayerInfo(static_cast<int>(layerIndex), &layer))
    {
        return 0u;
    }
    ImmStrokeReader::StrokeLayerTransformC pivot{};
    std::copy(layer.pivotRotation, layer.pivotRotation + 4, pivot.rotation);
    pivot.scale = layer.pivotScale;
    pivot.flip = layer.pivotFlip;
    std::copy(layer.pivotTranslation, layer.pivotTranslation + 3, pivot.translation);
    *outLocal = convertTransform(local);
    *outWorld = convertTransform(world);
    *outPivot = convertTransform(pivot);
    return 1u;
}

extern "C" uint32_t imm_web_get_animation_info(uint32_t layerIndex, ImmWebAnimationInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    uint32_t frameRate = 0u;
    uint32_t frameCount = 0u;
    uint32_t repeatCount = 0u;
    if (!gStore->GetLayerAnimationInfo(static_cast<int>(layerIndex), &frameRate, &frameCount, &repeatCount))
    {
        return 0u;
    }
    *outInfo = {frameRate, frameCount, repeatCount, 0u};
    return 1u;
}

extern "C" uint32_t imm_web_get_frame_buffer(uint32_t layerIndex, uint32_t* outFrames, uint32_t frameCapacity)
{
    if (gStore == nullptr || outFrames == nullptr || frameCapacity == 0u)
    {
        return 0u;
    }
    ImmWebAnimationInfo info{};
    if (imm_web_get_animation_info(layerIndex, &info) == 0u ||
        !gStore->GetFrameBuffer(static_cast<int>(layerIndex), outFrames, static_cast<int>(frameCapacity)))
    {
        return 0u;
    }
    return std::min(frameCapacity, info.frame_count);
}

extern "C" uint32_t imm_web_get_drawing_count(uint32_t layerIndex)
{
    return gStore == nullptr ? 0u : static_cast<uint32_t>(gStore->GetDrawingCount(static_cast<int>(layerIndex)));
}

extern "C" float imm_web_get_drawing_biggest_stroke(uint32_t layerIndex, uint32_t drawingIndex)
{
    float result = 0.0f;
    if (gStore != nullptr)
    {
        gStore->GetDrawingBiggestStroke(static_cast<int>(layerIndex), static_cast<int>(drawingIndex), &result);
    }
    return result;
}

extern "C" uint32_t imm_web_get_stroke_count(uint32_t layerIndex, uint32_t drawingIndex)
{
    return gStore == nullptr ? 0u : static_cast<uint32_t>(gStore->GetStrokeCount(
        static_cast<int>(layerIndex), static_cast<int>(drawingIndex)));
}

extern "C" uint32_t imm_web_get_stroke_info(
    uint32_t layerIndex,
    uint32_t drawingIndex,
    uint32_t strokeIndex,
    ImmWebStrokeInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    ImmStrokeReader::StrokeInfoC source{};
    if (!gStore->GetStrokeInfo(
        static_cast<int>(layerIndex), static_cast<int>(drawingIndex), static_cast<int>(strokeIndex), &source))
    {
        return 0u;
    }
    *outInfo = {};
    outInfo->brush_type = static_cast<uint32_t>(source.brushType);
    outInfo->visibility_mode = static_cast<uint32_t>(source.visibilityMode);
    outInfo->point_count = static_cast<uint32_t>(source.numPoints);
    outInfo->bounds_min[0] = source.bboxMinX;
    outInfo->bounds_min[1] = source.bboxMinY;
    outInfo->bounds_min[2] = source.bboxMinZ;
    outInfo->bounds_max[0] = source.bboxMaxX;
    outInfo->bounds_max[1] = source.bboxMaxY;
    outInfo->bounds_max[2] = source.bboxMaxZ;
    return 1u;
}

extern "C" uint32_t imm_web_get_stroke_points(
    uint32_t layerIndex,
    uint32_t drawingIndex,
    uint32_t strokeIndex,
    ImmWebStrokePoint* outPoints,
    uint32_t pointCapacity)
{
    if (gStore == nullptr || outPoints == nullptr || pointCapacity == 0u)
    {
        return 0u;
    }
    ImmWebStrokeInfo info{};
    if (imm_web_get_stroke_info(layerIndex, drawingIndex, strokeIndex, &info) == 0u)
    {
        return 0u;
    }
    const uint32_t count = std::min(pointCapacity, info.point_count);
    auto points = std::make_unique<ImmStrokeReader::StrokePointC[]>(count);
    if (!gStore->GetStrokePoints(
        static_cast<int>(layerIndex), static_cast<int>(drawingIndex), static_cast<int>(strokeIndex),
        points.get(), static_cast<int>(count)))
    {
        return 0u;
    }
    for (uint32_t index = 0u; index < count; ++index)
    {
        const ImmStrokeReader::StrokePointC& source = points[index];
        ImmWebStrokePoint& target = outPoints[index];
        target.position[0] = source.px;
        target.position[1] = source.py;
        target.position[2] = source.pz;
        target.normal[0] = source.nx;
        target.normal[1] = source.ny;
        target.normal[2] = source.nz;
        target.direction[0] = source.dx;
        target.direction[1] = source.dy;
        target.direction[2] = source.dz;
        target.color[0] = source.r;
        target.color[1] = source.g;
        target.color[2] = source.b;
        target.alpha = source.alpha;
        target.width = source.width;
    }
    return count;
}

extern "C" uint32_t imm_web_get_stroke_point_times(
    uint32_t layerIndex,
    uint32_t drawingIndex,
    uint32_t strokeIndex,
    float* outTimes,
    uint32_t pointCapacity)
{
    if (gStore == nullptr || outTimes == nullptr || pointCapacity == 0u)
    {
        return 0u;
    }
    ImmWebStrokeInfo info{};
    const uint32_t count = imm_web_get_stroke_info(layerIndex, drawingIndex, strokeIndex, &info) == 0u
        ? 0u
        : std::min(pointCapacity, info.point_count);
    if (count == 0u || !gStore->GetStrokePointTimes(
        static_cast<int>(layerIndex), static_cast<int>(drawingIndex), static_cast<int>(strokeIndex),
        outTimes, static_cast<int>(count)))
    {
        return 0u;
    }
    return count;
}

extern "C" uint32_t imm_web_get_picture_info(uint32_t layerIndex, ImmWebPictureInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    ImmStrokeReader::StrokePictureInfoC source{};
    if (!gStore->GetPictureInfo(static_cast<int>(layerIndex), &source))
    {
        return 0u;
    }
    *outInfo = {
        static_cast<uint32_t>(source.layerId),
        static_cast<uint32_t>(source.contentType),
        static_cast<uint32_t>(source.isViewerLocked),
        static_cast<uint32_t>(source.width),
        static_cast<uint32_t>(source.height),
        static_cast<uint32_t>(source.hasAlpha),
        static_cast<uint32_t>(source.dataSize),
    };
    return 1u;
}

extern "C" uint32_t imm_web_get_picture_pixels(uint32_t layerIndex, uint8_t* outPixels, uint32_t byteCapacity)
{
    if (gStore == nullptr || outPixels == nullptr || byteCapacity == 0u)
    {
        return 0u;
    }
    ImmStrokeReader::StrokePictureInfoC info{};
    if (!gStore->GetPictureInfo(static_cast<int>(layerIndex), &info) ||
        !gStore->GetPicturePixels(static_cast<int>(layerIndex), outPixels, static_cast<int>(byteCapacity)))
    {
        return 0u;
    }
    return std::min(byteCapacity, static_cast<uint32_t>(info.dataSize));
}

extern "C" uint32_t imm_web_get_sound_info(uint32_t timelineLayerIndex, ImmWebSoundInfo* outInfo)
{
    if (outInfo == nullptr || timelineLayerIndex >= gTimelineLayers.size()) return 0u;
    const uint32_t layerId = gTimelineLayers[timelineLayerIndex].id;
    const auto sound = std::find_if(gSounds.begin(), gSounds.end(), [layerId](const StoredSound& candidate) {
        return candidate.info.layer_id == layerId;
    });
    if (sound == gSounds.end()) return 0u;
    *outInfo = sound->info;
    return 1u;
}

extern "C" uint32_t imm_web_get_sound_bytes(
    uint32_t timelineLayerIndex,
    uint8_t* outBytes,
    uint32_t byteCapacity)
{
    if (outBytes == nullptr || timelineLayerIndex >= gTimelineLayers.size()) return 0u;
    const uint32_t layerId = gTimelineLayers[timelineLayerIndex].id;
    const auto sound = std::find_if(gSounds.begin(), gSounds.end(), [layerId](const StoredSound& candidate) {
        return candidate.info.layer_id == layerId;
    });
    if (sound == gSounds.end() || byteCapacity < sound->bytes.size()) return 0u;
    std::copy(sound->bytes.begin(), sound->bytes.end(), outBytes);
    return static_cast<uint32_t>(sound->bytes.size());
}

extern "C" uint32_t imm_web_get_playback_info(ImmWebPlaybackInfo* outInfo)
{
    if (gStore == nullptr || outInfo == nullptr)
    {
        return 0u;
    }
    *outInfo = {
        kTicksPerSecond,
        gAnimateOnStart ? 1u : 0u,
        static_cast<uint32_t>(gTimelineLayers.size()),
        static_cast<uint32_t>(gChapters.size()),
        gDurationTicks,
        {0u, 0u},
    };
    return 1u;
}

extern "C" uint32_t imm_web_get_timeline_layer_info(uint32_t layerIndex, ImmWebTimelineLayerInfo* outInfo)
{
    if (outInfo == nullptr || layerIndex >= gTimelineLayers.size())
    {
        return 0u;
    }
    const StoredTimelineLayer& source = gTimelineLayers[layerIndex];
    *outInfo = {};
    outInfo->id = source.id;
    outInfo->parent_id = source.parentId;
    outInfo->type = source.type;
    outInfo->flags = (source.visible ? 1u : 0u) | (source.timeline ? 2u : 0u) |
        (source.spawnFloorLevel ? 4u : 0u);
    outInfo->opacity = source.opacity;
    outInfo->max_repeat_count = source.maxRepeatCount;
    outInfo->duration_ticks = source.durationTicks;
    outInfo->key_count = static_cast<uint32_t>(source.keys.size());
    outInfo->content_layer_index = source.contentLayerIndex;
    const size_t copyLength = std::min(source.name.size(), sizeof(outInfo->name) - 1u);
    std::memcpy(outInfo->name, source.name.data(), copyLength);
    return 1u;
}

extern "C" uint32_t imm_web_get_timeline_layer_transforms(
    uint32_t layerIndex,
    ImmWebTransform* outLocal,
    ImmWebTransform* outWorld,
    ImmWebTransform* outPivot)
{
    if (outLocal == nullptr || outWorld == nullptr || outPivot == nullptr || layerIndex >= gTimelineLayers.size())
    {
        return 0u;
    }
    const StoredTimelineLayer& source = gTimelineLayers[layerIndex];
    *outLocal = convertTransform(source.local);
    *outWorld = convertTransform(source.world);
    *outPivot = convertTransform(source.pivot);
    return 1u;
}

extern "C" uint32_t imm_web_get_animation_key(
    uint32_t layerIndex,
    uint32_t keyIndex,
    ImmWebAnimationKey* outKey)
{
    if (outKey == nullptr || layerIndex >= gTimelineLayers.size() || keyIndex >= gTimelineLayers[layerIndex].keys.size())
    {
        return 0u;
    }
    const StoredTimelineKey& source = gTimelineLayers[layerIndex].keys[keyIndex];
    *outKey = {};
    outKey->property = source.property;
    outKey->interpolation = source.interpolation;
    outKey->time_ticks = source.timeTicks;
    outKey->bool_value = source.value.mBool ? 1u : 0u;
    outKey->uint_value = source.value.mInt;
    outKey->float_value = source.value.mFloat;
    outKey->double_value = source.value.mDouble;
    outKey->transform_value = convertTransform(source.value.mTransform);
    return 1u;
}

extern "C" uint32_t imm_web_get_chapter_info(uint32_t chapterIndex, ImmWebChapterInfo* outInfo)
{
    if (outInfo == nullptr || chapterIndex >= gChapters.size())
    {
        return 0u;
    }
    const StoredChapter& source = gChapters[chapterIndex];
    *outInfo = {source.startTicks, source.endTicks, source.markerAction, 0u};
    return 1u;
}

extern "C" uint32_t imm_web_get_keep_alive_info(uint32_t layerIndex, ImmWebKeepAliveInfo* outInfo)
{
    if (outInfo == nullptr || layerIndex >= gTimelineLayers.size())
    {
        return 0u;
    }
    *outInfo = gTimelineLayers[layerIndex].keepAlive;
    return 1u;
}
