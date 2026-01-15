//
// Miniaudio-based sound engine backend for cross-platform stereo audio playback
//
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <malloc.h>
#include <vector>
#include <mutex>
#include <cstring>

#include "piSoundEngineMiniaudio.h"
#include "../libBasics/piTypes.h"
#include "../libBasics/piLog.h"

namespace ImmCore {

// Maximum number of simultaneous sounds
static const int kMaxSounds = 64;

struct SoundData
{
    bool inUse = false;
    ma_decoder decoder;
    bool decoderInitialized = false;

    // For raw PCM data
    float* pcmData = nullptr;
    uint64_t pcmLength = 0;  // in frames
    uint64_t pcmPosition = 0;
    int pcmChannels = 0;
    int pcmSampleRate = 0;

    bool isPlaying = false;
    bool isPaused = false;
    bool isLooping = false;
    float volume = 1.0f;
    piSoundEngine::SoundType type = piSoundEngine::SoundType::Flat;

    // Position/orientation for positional audio (basic support)
    double position[3] = {0, 0, 0};
    double direction[3] = {0, 0, 1};
    double up[3] = {0, 1, 0};
};

struct iSoundEngineBackendMiniaudio
{
    ma_device device;
    ma_device_config deviceConfig;
    bool deviceInitialized = false;

    SoundData sounds[kMaxSounds];
    std::mutex soundsMutex;

    piLog* log = nullptr;
    piSoundEngine* engine = nullptr;

    // Listener transform
    trans3d listenerToWorld;
};

// Audio callback - mixes all active sounds
static void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    iSoundEngineBackendMiniaudio* backend = (iSoundEngineBackendMiniaudio*)pDevice->pUserData;
    float* output = (float*)pOutput;

    // Clear output buffer
    memset(output, 0, frameCount * pDevice->playback.channels * sizeof(float));

    std::lock_guard<std::mutex> lock(backend->soundsMutex);

    for (int i = 0; i < kMaxSounds; i++)
    {
        SoundData& sound = backend->sounds[i];
        if (!sound.inUse || !sound.isPlaying || sound.isPaused)
            continue;

        if (sound.decoderInitialized)
        {
            // Decode from file
            float tempBuffer[4096];
            ma_uint64 framesToRead = frameCount;
            ma_uint64 framesRead = 0;

            ma_result result = ma_decoder_read_pcm_frames(&sound.decoder, tempBuffer, framesToRead, &framesRead);

            // Mix into output
            ma_uint32 channels = pDevice->playback.channels;
            for (ma_uint64 f = 0; f < framesRead; f++)
            {
                for (ma_uint32 c = 0; c < channels && c < 2; c++)
                {
                    output[f * channels + c] += tempBuffer[f * 2 + (c % 2)] * sound.volume;
                }
            }

            // Handle looping or stop
            if (framesRead < framesToRead)
            {
                if (sound.isLooping)
                {
                    ma_decoder_seek_to_pcm_frame(&sound.decoder, 0);
                }
                else
                {
                    sound.isPlaying = false;
                }
            }
        }
        else if (sound.pcmData)
        {
            // Play from PCM buffer
            ma_uint32 channels = pDevice->playback.channels;
            ma_uint64 framesToPlay = frameCount;
            ma_uint64 framesRemaining = sound.pcmLength - sound.pcmPosition;

            if (framesToPlay > framesRemaining)
                framesToPlay = framesRemaining;

            for (ma_uint64 f = 0; f < framesToPlay; f++)
            {
                for (ma_uint32 c = 0; c < channels && c < (ma_uint32)sound.pcmChannels; c++)
                {
                    output[f * channels + c] += sound.pcmData[(sound.pcmPosition + f) * sound.pcmChannels + c] * sound.volume;
                }
            }

            sound.pcmPosition += framesToPlay;

            // Handle looping or stop
            if (sound.pcmPosition >= sound.pcmLength)
            {
                if (sound.isLooping)
                {
                    sound.pcmPosition = 0;
                }
                else
                {
                    sound.isPlaying = false;
                }
            }
        }
    }

    // Clamp output
    for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; i++)
    {
        if (output[i] > 1.0f) output[i] = 1.0f;
        if (output[i] < -1.0f) output[i] = -1.0f;
    }

    (void)pInput; // Unused
}

class piSoundEngineMiniaudio : public piSoundEngine
{
public:
    piSoundEngineMiniaudio(iSoundEngineBackendMiniaudio* bk) : mBk(bk) {}

    int AddSound(int numChannels, uint64_t length, const void* buffer, bool makePositional) override
    {
        // Assume 48kHz float32 PCM
        return AddSound(48000, 32, numChannels, length, buffer, makePositional);
    }

    int AddSound(void(*callback)(float* channelBuffer, size_t numSamples, size_t numChannels, void* userData), void* userData) override
    {
        // Callback-based sounds not implemented in this simple backend
        return -1;
    }

    int AddSound(const wchar_t* filename, bool makePositional) override
    {
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);

        int slot = findFreeSlot();
        if (slot < 0) return -1;

        SoundData& sound = mBk->sounds[slot];

        // Convert wchar_t to char for miniaudio
        char filenameUtf8[1024];
        wcstombs(filenameUtf8, filename, sizeof(filenameUtf8));

        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);
        if (ma_decoder_init_file(filenameUtf8, &config, &sound.decoder) != MA_SUCCESS)
        {
            if (mBk->log) mBk->log->Printf(LT_ERROR, L"[Miniaudio] Failed to load: %s", filename);
            return -1;
        }

        sound.inUse = true;
        sound.decoderInitialized = true;
        sound.type = makePositional ? SoundType::Positional : SoundType::Flat;
        sound.volume = 1.0f;
        sound.isPlaying = false;
        sound.isPaused = false;
        sound.isLooping = false;

        return slot;
    }

    int AddSound(const int frequency, int precision, int numChannels, uint64_t length, const void* buffer, bool makePositional) override
    {
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);

        int slot = findFreeSlot();
        if (slot < 0) return -1;

        SoundData& sound = mBk->sounds[slot];

        // Convert to float if needed and store
        uint64_t numFrames = length / numChannels / (precision / 8);
        sound.pcmData = (float*)malloc(numFrames * numChannels * sizeof(float));
        if (!sound.pcmData) return -1;

        if (precision == 32)
        {
            memcpy(sound.pcmData, buffer, numFrames * numChannels * sizeof(float));
        }
        else if (precision == 16)
        {
            const int16_t* src = (const int16_t*)buffer;
            for (uint64_t i = 0; i < numFrames * numChannels; i++)
            {
                sound.pcmData[i] = src[i] / 32768.0f;
            }
        }

        sound.inUse = true;
        sound.pcmLength = numFrames;
        sound.pcmPosition = 0;
        sound.pcmChannels = numChannels;
        sound.pcmSampleRate = frequency;
        sound.type = makePositional ? SoundType::Positional : SoundType::Flat;
        sound.volume = 1.0f;
        sound.isPlaying = false;
        sound.isPaused = false;
        sound.isLooping = false;

        return slot;
    }

    void DelSound(int id) override
    {
        if (id < 0 || id >= kMaxSounds) return;

        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        SoundData& sound = mBk->sounds[id];

        if (sound.decoderInitialized)
        {
            ma_decoder_uninit(&sound.decoder);
            sound.decoderInitialized = false;
        }
        if (sound.pcmData)
        {
            free(sound.pcmData);
            sound.pcmData = nullptr;
        }
        sound.inUse = false;
    }

    bool Play(int id, uint64_t microSecondOffset) override
    {
        if (id < 0 || id >= kMaxSounds) return false;

        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        SoundData& sound = mBk->sounds[id];
        if (!sound.inUse) return false;

        if (sound.decoderInitialized)
        {
            // Seek to offset
            uint64_t frameOffset = (microSecondOffset * 48000) / 1000000;
            ma_decoder_seek_to_pcm_frame(&sound.decoder, frameOffset);
        }
        else if (sound.pcmData)
        {
            uint64_t frameOffset = (microSecondOffset * sound.pcmSampleRate) / 1000000;
            sound.pcmPosition = frameOffset;
        }

        sound.isPlaying = true;
        sound.isPaused = false;
        return true;
    }

    bool Stop(int id) override
    {
        if (id < 0 || id >= kMaxSounds) return false;

        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        SoundData& sound = mBk->sounds[id];
        if (!sound.inUse) return false;

        sound.isPlaying = false;
        sound.isPaused = false;

        // Reset position
        if (sound.decoderInitialized)
            ma_decoder_seek_to_pcm_frame(&sound.decoder, 0);
        else
            sound.pcmPosition = 0;

        return true;
    }

    bool Pause(int id) override
    {
        if (id < 0 || id >= kMaxSounds) return false;

        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        SoundData& sound = mBk->sounds[id];
        if (!sound.inUse) return false;

        sound.isPaused = true;
        return true;
    }

    bool Resume(int id) override
    {
        if (id < 0 || id >= kMaxSounds) return false;

        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        SoundData& sound = mBk->sounds[id];
        if (!sound.inUse) return false;

        sound.isPaused = false;
        return true;
    }

    void PauseAllSounds() override
    {
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        for (int i = 0; i < kMaxSounds; i++)
        {
            if (mBk->sounds[i].inUse && mBk->sounds[i].isPlaying)
                mBk->sounds[i].isPaused = true;
        }
    }

    void ResumeAllSounds() override
    {
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        for (int i = 0; i < kMaxSounds; i++)
        {
            if (mBk->sounds[i].inUse && mBk->sounds[i].isPlaying)
                mBk->sounds[i].isPaused = false;
        }
    }

    void SetListener(const trans3d& listenerToWorld) override
    {
        mBk->listenerToWorld = listenerToWorld;
    }

    void SetPosition(int id, const double* pos) override
    {
        if (id < 0 || id >= kMaxSounds) return;
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        SoundData& sound = mBk->sounds[id];
        if (!sound.inUse) return;
        memcpy(sound.position, pos, 3 * sizeof(double));
    }

    void SetOrientation(int id, const double* dir, const double* up) override
    {
        if (id < 0 || id >= kMaxSounds) return;
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        SoundData& sound = mBk->sounds[id];
        if (!sound.inUse) return;
        memcpy(sound.direction, dir, 3 * sizeof(double));
        memcpy(sound.up, up, 3 * sizeof(double));
    }

    void SetPositionOrientation(int id, const double* pos, const double* dir, const double* up) override
    {
        SetPosition(id, pos);
        SetOrientation(id, dir, up);
    }

    PlaybackState GetPlaybackState(int id) override
    {
        if (id < 0 || id >= kMaxSounds) return PlaybackState::Stopped;
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        SoundData& sound = mBk->sounds[id];
        if (!sound.inUse) return PlaybackState::Stopped;
        if (sound.isPaused) return PlaybackState::Paused;
        if (sound.isPlaying) return PlaybackState::Playing;
        return PlaybackState::Stopped;
    }

    float GetVolume(int id) const override
    {
        if (id < 0 || id >= kMaxSounds) return 0.0f;
        return mBk->sounds[id].volume;
    }

    bool SetVolume(int id, float volume) override
    {
        if (id < 0 || id >= kMaxSounds) return false;
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        mBk->sounds[id].volume = volume;
        return true;
    }

    SoundType GetType(int id) const override
    {
        if (id < 0 || id >= kMaxSounds) return SoundType::Flat;
        return mBk->sounds[id].type;
    }

    bool ConvertType(int id, SoundType type) override
    {
        if (id < 0 || id >= kMaxSounds) return false;
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        mBk->sounds[id].type = type;
        return true;
    }

    bool CanConvertType(int id, SoundType type) const override
    {
        return true; // We support flat conversion
    }

    bool GetLooping(int id) const override
    {
        if (id < 0 || id >= kMaxSounds) return false;
        return mBk->sounds[id].isLooping;
    }

    void SetLooping(int id, bool looping) override
    {
        if (id < 0 || id >= kMaxSounds) return;
        std::lock_guard<std::mutex> lock(mBk->soundsMutex);
        mBk->sounds[id].isLooping = looping;
    }

    void SetAttenMode(int id, AttenuationType attenMode) override {}
    void SetAttenMinMax(int id, double attenMin, double attenMax) override {}
    void SetModifierType(int id, ModifierType modifType) override {}
    void SetModifierCone(int id, double angleInner, double angleBand, double attenOut) override {}
    void SetModifierFrustum(int id, double angleInnerX, double angleInnerY, double angleBand, double attenOut) override {}
    float ComputeModifiers(int id) override { return 1.0f; }

private:
    int findFreeSlot()
    {
        for (int i = 0; i < kMaxSounds; i++)
        {
            if (!mBk->sounds[i].inUse)
                return i;
        }
        return -1;
    }

    iSoundEngineBackendMiniaudio* mBk;
};

// Backend implementation

piSoundEngineBackendMiniaudio::piSoundEngineBackendMiniaudio() : piSoundEngineBackend()
{
    mData = nullptr;
}

piSoundEngineBackendMiniaudio::~piSoundEngineBackendMiniaudio()
{
}

bool piSoundEngineBackendMiniaudio::doInit(piLog* log)
{
    iSoundEngineBackendMiniaudio* me = new iSoundEngineBackendMiniaudio();
    if (!me) return false;

    me->log = log;
    mData = me;

    // Create the sound engine
    me->engine = new piSoundEngineMiniaudio(me);

    return true;
}

void piSoundEngineBackendMiniaudio::doDeinit()
{
    iSoundEngineBackendMiniaudio* me = (iSoundEngineBackendMiniaudio*)mData;

    if (me->deviceInitialized)
    {
        ma_device_uninit(&me->device);
        me->deviceInitialized = false;
    }

    // Clean up all sounds
    for (int i = 0; i < kMaxSounds; i++)
    {
        SoundData& sound = me->sounds[i];
        if (sound.decoderInitialized)
        {
            ma_decoder_uninit(&sound.decoder);
        }
        if (sound.pcmData)
        {
            free(sound.pcmData);
        }
    }

    delete me->engine;
    delete me;
}

int piSoundEngineBackendMiniaudio::GetNumDevices()
{
    return 1; // Default device only for simplicity
}

const wchar_t* piSoundEngineBackendMiniaudio::GetDeviceName(int id) const
{
    return L"Default Audio Device";
}

int piSoundEngineBackendMiniaudio::GetDeviceFromGUID(void* deviceGUID)
{
    return 0;
}

int piSoundEngineBackendMiniaudio::GetDeviceFromName(const wchar_t* name)
{
    return 0;
}

bool piSoundEngineBackendMiniaudio::Init(void* hwnd, int deviceID, const Configuration* config)
{
    iSoundEngineBackendMiniaudio* me = (iSoundEngineBackendMiniaudio*)mData;

    me->deviceConfig = ma_device_config_init(ma_device_type_playback);
    me->deviceConfig.playback.format = ma_format_f32;
    me->deviceConfig.playback.channels = 2;
    me->deviceConfig.sampleRate = config ? config->mSampleRate : 48000;
    me->deviceConfig.dataCallback = audioCallback;
    me->deviceConfig.pUserData = me;

    if (ma_device_init(NULL, &me->deviceConfig, &me->device) != MA_SUCCESS)
    {
        if (me->log) me->log->Printf(LT_ERROR, L"[Miniaudio] Failed to initialize audio device");
        return false;
    }

    me->deviceInitialized = true;

    if (ma_device_start(&me->device) != MA_SUCCESS)
    {
        if (me->log) me->log->Printf(LT_ERROR, L"[Miniaudio] Failed to start audio device");
        ma_device_uninit(&me->device);
        me->deviceInitialized = false;
        return false;
    }

    if (me->log) me->log->Printf(LT_DEBUG, L"[Miniaudio] Audio device initialized: %d Hz, %d channels",
                                  me->device.sampleRate, me->device.playback.channels);

    return true;
}

void piSoundEngineBackendMiniaudio::Deinit()
{
    iSoundEngineBackendMiniaudio* me = (iSoundEngineBackendMiniaudio*)mData;

    if (me->deviceInitialized)
    {
        ma_device_uninit(&me->device);
        me->deviceInitialized = false;
    }
}

void piSoundEngineBackendMiniaudio::Tick()
{
    // Nothing to do - audio runs in callback
}

piSoundEngine* piSoundEngineBackendMiniaudio::GetEngine()
{
    iSoundEngineBackendMiniaudio* me = (iSoundEngineBackendMiniaudio*)mData;
    return me->engine;
}

bool piSoundEngineBackendMiniaudio::ResizeMixBuffers(int const& mixSamples, int const& spatialSamples)
{
    return true; // Not needed for miniaudio
}

} // namespace ImmCore
