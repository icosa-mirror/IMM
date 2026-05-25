#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "piSoundEngineAVFoundation.h"
#include "../../libBasics/piLog.h"
#include "../../libBasics/piString.h"

#include <ogg/ogg.h>
#include <opus.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vector>

namespace ImmCore
{

struct AVFSound
{
    AVAudioPlayer *player = nil;
    NSString *tempPath = nil;
    piSoundEngine::SoundType type = piSoundEngine::SoundType::Flat;
    bool looping = false;
    bool paused = false;
    bool stopped = true;
    float volume = 1.0f;
    int lastLoggedPlaybackState = -1;
    bool loggedPlaybackProgress = false;
};

struct AVFBackend
{
    piLog *log = nullptr;
    piSoundEngine *engine = nullptr;
    NSMutableArray *sounds = nil;
    NSString *tempDirectory = nil;
    uint64_t soundsDestroyed = 0;
    uint64_t tempFilesRemoved = 0;
    uint64_t tempFileRemoveFailures = 0;
};

static bool iWriteWavFile(NSString *path, int frequency, int precision, int numChannels, uint64_t dataSize, const void *buffer)
{
    if (!path || !buffer || frequency <= 0 || (precision != 16 && precision != 32) || numChannels <= 0)
    {
        return false;
    }

    NSMutableData *data = [NSMutableData dataWithCapacity:(NSUInteger)(44u + dataSize)];
    const uint32_t riffSize = (uint32_t)(36u + dataSize);
    const uint16_t audioFormat = (precision == 32) ? 3 : 1;
    const uint16_t channels = (uint16_t)numChannels;
    const uint32_t sampleRate = (uint32_t)frequency;
    const uint16_t bitsPerSample = (uint16_t)precision;
    const uint16_t blockAlign = (uint16_t)((numChannels * precision) / 8);
    const uint32_t byteRate = sampleRate * blockAlign;
    const uint32_t subchunkSize = (uint32_t)dataSize;

    [data appendBytes:"RIFF" length:4];
    [data appendBytes:&riffSize length:4];
    [data appendBytes:"WAVE" length:4];
    [data appendBytes:"fmt " length:4];
    const uint32_t fmtSize = 16;
    [data appendBytes:&fmtSize length:4];
    [data appendBytes:&audioFormat length:2];
    [data appendBytes:&channels length:2];
    [data appendBytes:&sampleRate length:4];
    [data appendBytes:&byteRate length:4];
    [data appendBytes:&blockAlign length:2];
    [data appendBytes:&bitsPerSample length:2];
    [data appendBytes:"data" length:4];
    [data appendBytes:&subchunkSize length:4];
    [data appendBytes:buffer length:(NSUInteger)dataSize];
    return [data writeToFile:path atomically:NO] == YES;
}

static uint16_t iReadLE16(const unsigned char *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8u);
}

static bool iDecodeOggOpusToPCM(const void *buffer, uint64_t length, std::vector<int16_t> &pcm, int *channelsOut, int *sampleRateOut)
{
    if (!buffer || length == 0 || !channelsOut || !sampleRateOut)
    {
        return false;
    }

    ogg_sync_state sync;
    ogg_sync_init(&sync);
    char *oggBuffer = ogg_sync_buffer(&sync, (long)length);
    memcpy(oggBuffer, buffer, (size_t)length);
    ogg_sync_wrote(&sync, (long)length);

    ogg_stream_state stream;
    bool streamInitialized = false;
    OpusDecoder *decoder = nullptr;
    int channels = 0;
    int preSkip = 0;
    int packetIndex = 0;
    bool ok = true;

    ogg_page page;
    while (ok && ogg_sync_pageout(&sync, &page) == 1)
    {
        if (!streamInitialized)
        {
            if (ogg_stream_init(&stream, ogg_page_serialno(&page)) != 0)
            {
                ok = false;
                break;
            }
            streamInitialized = true;
        }

        if (ogg_stream_pagein(&stream, &page) != 0)
        {
            ok = false;
            break;
        }

        ogg_packet packet;
        while (ok && ogg_stream_packetout(&stream, &packet) == 1)
        {
            if (packetIndex == 0)
            {
                if (packet.bytes < 19 || memcmp(packet.packet, "OpusHead", 8) != 0)
                {
                    ok = false;
                    break;
                }
                channels = packet.packet[9];
                preSkip = iReadLE16(packet.packet + 10);
                const int mappingFamily = packet.packet[18];
                if (channels < 1 || channels > 2 || mappingFamily != 0)
                {
                    ok = false;
                    break;
                }
                int opusError = OPUS_OK;
                decoder = opus_decoder_create(48000, channels, &opusError);
                if (!decoder || opusError != OPUS_OK)
                {
                    ok = false;
                    break;
                }
            }
            else if (packetIndex == 1)
            {
                if (packet.bytes < 8 || memcmp(packet.packet, "OpusTags", 8) != 0)
                {
                    ok = false;
                    break;
                }
            }
            else if (decoder)
            {
                int16_t decoded[5760 * 2];
                const int samples = opus_decode(decoder, packet.packet, (opus_int32)packet.bytes, decoded, 5760, 0);
                if (samples < 0)
                {
                    ok = false;
                    break;
                }

                int startSample = 0;
                if (preSkip > 0)
                {
                    startSample = preSkip > samples ? samples : preSkip;
                    preSkip -= startSample;
                }
                const int samplesToCopy = samples - startSample;
                if (samplesToCopy > 0)
                {
                    const int16_t *src = decoded + startSample * channels;
                    pcm.insert(pcm.end(), src, src + samplesToCopy * channels);
                }
            }
            ++packetIndex;
        }
    }

    if (decoder)
    {
        opus_decoder_destroy(decoder);
    }
    if (streamInitialized)
    {
        ogg_stream_clear(&stream);
    }
    ogg_sync_clear(&sync);

    if (!ok || packetIndex < 3 || pcm.empty() || channels < 1)
    {
        pcm.clear();
        return false;
    }

    *channelsOut = channels;
    *sampleRateOut = 48000;
    return true;
}

static bool iDecodeOggOpusToWavFile(NSString *path, const void *buffer, uint64_t length, int *channelsOut)
{
    std::vector<int16_t> pcm;
    int channels = 0;
    int sampleRate = 0;
    if (!iDecodeOggOpusToPCM(buffer, length, pcm, &channels, &sampleRate))
    {
        return false;
    }
    if (channelsOut)
    {
        *channelsOut = channels;
    }
    return iWriteWavFile(path, sampleRate, 16, channels, (uint64_t)(pcm.size() * sizeof(int16_t)), pcm.data());
}

static NSString *iCreateTempPath(AVFBackend *backend, NSString *extension)
{
    NSString *directory = backend && backend->tempDirectory ? backend->tempDirectory : NSTemporaryDirectory();
    NSString *name = [[NSUUID UUID].UUIDString stringByAppendingPathExtension:extension ?: @"dat"];
    return [directory stringByAppendingPathComponent:name];
}

static int iStoreSound(AVFBackend *backend, AVAudioPlayer *player, NSString *tempPath, bool makePositional)
{
    if (!backend || !backend->sounds || !player)
    {
        return -1;
    }

    AVFSound *sound = new AVFSound();
    sound->player = [player retain];
    sound->tempPath = [tempPath retain];
    sound->type = makePositional ? piSoundEngine::SoundType::Positional : piSoundEngine::SoundType::Flat;
    NSValue *value = [NSValue valueWithPointer:sound];
    [backend->sounds addObject:value];
    return (int)[backend->sounds count] - 1;
}

static AVFSound *iGetSound(AVFBackend *backend, int id)
{
    if (!backend || !backend->sounds || id < 0 || id >= (int)[backend->sounds count])
    {
        return nullptr;
    }
    return (AVFSound *)[[backend->sounds objectAtIndex:(NSUInteger)id] pointerValue];
}

static const wchar_t *iPlaybackStateName(piSoundEngine::PlaybackState state)
{
    switch ((int)state)
    {
    case (int)piSoundEngine::PlaybackState::PlayingStarting: return L"playing-starting";
    case (int)piSoundEngine::PlaybackState::Playing: return L"playing";
    case (int)piSoundEngine::PlaybackState::PausingStarted: return L"pausing";
    case (int)piSoundEngine::PlaybackState::Paused: return L"paused-or-stopping";
    case (int)piSoundEngine::PlaybackState::Stopped: return L"stopped";
    default: return L"unknown";
    }
}

static void iLogPlaybackState(AVFBackend *backend, int id, AVFSound *sound, piSoundEngine::PlaybackState state)
{
    if (!backend || !backend->log || !sound) return;

    const int numericState = (int)state;
    if (sound->lastLoggedPlaybackState == numericState) return;
    sound->lastLoggedPlaybackState = numericState;

    const double currentTime = sound->player ? (double)sound->player.currentTime : 0.0;
    const double duration = sound->player ? (double)sound->player.duration : 0.0;
    backend->log->Printf(LT_MESSAGE,
                         L"AVFoundation audio Playback state: id=%d state=%ls timeSec=%.3f durationSec=%.3f",
                         id,
                         iPlaybackStateName(state),
                         currentTime,
                         duration);
}

static void iLogPlaybackProgress(AVFBackend *backend, int id, AVFSound *sound)
{
    if (!backend || !backend->log || !sound || !sound->player || sound->loggedPlaybackProgress) return;

    double thresholdSec = 0.25;
    const char *thresholdEnv = getenv("IMM_AVFOUNDATION_AUDIO_PROGRESS_THRESHOLD_SEC");
    if (thresholdEnv && thresholdEnv[0] != '\0')
    {
        double parsed = 0.0;
        if (sscanf(thresholdEnv, "%lf", &parsed) == 1 && parsed >= 0.0)
        {
            thresholdSec = parsed;
        }
    }

    const double currentTime = (double)sound->player.currentTime;
    if (currentTime < thresholdSec) return;

    sound->loggedPlaybackProgress = true;
    backend->log->Printf(LT_MESSAGE,
                         L"AVFoundation audio Playback progress: id=%d timeSec=%.3f thresholdSec=%.3f durationSec=%.3f",
                         id,
                         currentTime,
                         thresholdSec,
                         (double)sound->player.duration);
}

static bool iRemoveTempFile(AVFBackend *backend, NSString *path, const wchar_t *context)
{
    if (!path)
    {
        return true;
    }

    NSError *error = nil;
    if ([[NSFileManager defaultManager] removeItemAtPath:path error:&error])
    {
        if (backend)
        {
            ++backend->tempFilesRemoved;
        }
        return true;
    }

    if (![[NSFileManager defaultManager] fileExistsAtPath:path])
    {
        return true;
    }

    if (backend)
    {
        ++backend->tempFileRemoveFailures;
        if (backend->log)
        {
            const char *pathUtf8 = [path fileSystemRepresentation];
            const char *message = error ? [[error localizedDescription] UTF8String] : "unknown";
            backend->log->Printf(LT_ERROR,
                                 L"AVFoundation audio temp file removal failed: context=%ls path=%hs error=%hs",
                                 context ? context : L"unknown",
                                 pathUtf8 ? pathUtf8 : "",
                                 message ? message : "unknown");
        }
    }
    return false;
}

static void iDestroySound(AVFBackend *backend, AVFSound *sound)
{
    if (!sound)
    {
        return;
    }
    [sound->player stop];
    [sound->player release];
    if (sound->tempPath)
    {
        iRemoveTempFile(backend, sound->tempPath, L"destroy-sound");
        [sound->tempPath release];
    }
    if (backend)
    {
        ++backend->soundsDestroyed;
    }
    delete sound;
}

class piSoundEngineAVFoundation final : public piSoundEngine
{
public:
    explicit piSoundEngineAVFoundation(AVFBackend *backend) : mBackend(backend) {}

    int AddSound(int numChannels, uint64_t length, const void *buffer, bool makePositional) override
    {
        NSString *path = iCreateTempPath(mBackend, @"wav");
        int decodedChannels = 0;

        if (!iDecodeOggOpusToWavFile(path, buffer, length, &decodedChannels))
        {
            if (mBackend && mBackend->log)
            {
                mBackend->log->Printf(LT_ERROR, L"AVFoundation audio Ogg Opus decode failed: declaredChannels=%d bytes=%llu", numChannels, (unsigned long long)length);
            }
            iRemoveTempFile(mBackend, path, L"ogg-decode-failure");
            return -1;
        }

        NSError *error = nil;
        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:path] error:&error];
        if (!player)
        {
            if (mBackend && mBackend->log)
            {
                mBackend->log->Printf(LT_ERROR, L"AVFoundation audio player creation failed for decoded Ogg Opus temp WAV");
            }
            iRemoveTempFile(mBackend, path, L"ogg-player-creation-failure");
            return -1;
        }
        [player prepareToPlay];
        const int id = iStoreSound(mBackend, player, path, makePositional);
        if (id < 0)
        {
            if (mBackend && mBackend->log)
            {
                mBackend->log->Printf(LT_ERROR, L"AVFoundation audio sound storage failed for decoded Ogg Opus temp WAV");
            }
            iRemoveTempFile(mBackend, path, L"ogg-store-failure");
            [player release];
            return -1;
        }
        if (mBackend && mBackend->log)
        {
            mBackend->log->Printf(LT_MESSAGE, L"Decoded Ogg Opus sound to PCM temp WAV for AVFoundation: id=%d channels=%d bytes=%llu", id, decodedChannels, (unsigned long long)length);
        }
        [player release];
        return id;
    }

    int AddSound(void (*)(float *, size_t, size_t, void *), void *) override
    {
        return -1;
    }

    int AddSound(const wchar_t *filename, bool makePositional) override
    {
        if (!filename)
        {
            return -1;
        }
        char pathUtf8[4096] = {};
        const size_t len = wcstombs(pathUtf8, filename, sizeof(pathUtf8) - 1);
        if (len == (size_t)-1)
        {
            return -1;
        }
        pathUtf8[len] = 0;
        NSString *path = [NSString stringWithUTF8String:pathUtf8];
        NSError *error = nil;
        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:path] error:&error];
        if (!player)
        {
            return -1;
        }
        [player prepareToPlay];
        const int id = iStoreSound(mBackend, player, nil, makePositional);
        [player release];
        return id;
    }

    int AddSound(const int frequency, int precision, int numChannels, uint64_t length, const void *buffer, bool makePositional) override
    {
        NSString *path = iCreateTempPath(mBackend, @"wav");
        if (!iWriteWavFile(path, frequency, precision, numChannels, length, buffer))
        {
            if (mBackend && mBackend->log)
            {
                mBackend->log->Printf(LT_ERROR,
                                      L"AVFoundation audio temp WAV write failed: frequency=%d precision=%d channels=%d bytes=%llu",
                                      frequency,
                                      precision,
                                      numChannels,
                                      (unsigned long long)length);
            }
            iRemoveTempFile(mBackend, path, L"pcm-write-failure");
            return -1;
        }
        NSError *error = nil;
        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:[NSURL fileURLWithPath:path] error:&error];
        if (!player)
        {
            if (mBackend && mBackend->log)
            {
                const char *message = error ? [[error localizedDescription] UTF8String] : "unknown";
                mBackend->log->Printf(LT_ERROR, L"AVFoundation audio player creation failed for PCM temp WAV: error=%hs", message ? message : "unknown");
            }
            iRemoveTempFile(mBackend, path, L"pcm-player-creation-failure");
            return -1;
        }
        [player prepareToPlay];
        const int id = iStoreSound(mBackend, player, path, makePositional);
        if (id < 0)
        {
            if (mBackend && mBackend->log)
            {
                mBackend->log->Printf(LT_ERROR, L"AVFoundation audio sound storage failed for PCM temp WAV");
            }
            iRemoveTempFile(mBackend, path, L"pcm-store-failure");
            [player release];
            return -1;
        }
        [player release];
        return id;
    }

    void DelSound(int id) override
    {
        AVFSound *sound = iGetSound(mBackend, id);
        if (sound)
        {
            iDestroySound(mBackend, sound);
            [mBackend->sounds replaceObjectAtIndex:(NSUInteger)id withObject:[NSValue valueWithPointer:nullptr]];
        }
    }

    bool Play(int id, uint64_t microSecondOffset) override
    {
        AVFSound *sound = iGetSound(mBackend, id);
        if (!sound)
        {
            return false;
        }
        if (!sound->player)
        {
            if (mBackend && mBackend->log)
            {
                mBackend->log->Printf(LT_WARNING, L"AVFoundation audio Play rejected: id=%d offsetUs=%llu reason=no-player", id, (unsigned long long)microSecondOffset);
            }
            return false;
        }
        sound->player.currentTime = (NSTimeInterval)((double)microSecondOffset / 1000000.0);
        sound->player.numberOfLoops = sound->looping ? -1 : 0;
        sound->player.volume = sound->volume;
        sound->paused = false;
        sound->stopped = false;
        const bool accepted = [sound->player play] == YES;
        if (mBackend && mBackend->log)
        {
            if (accepted)
            {
                mBackend->log->Printf(LT_MESSAGE,
                                      L"AVFoundation audio Play accepted: id=%d offsetUs=%llu",
                                      id,
                                      (unsigned long long)microSecondOffset);
            }
            else
            {
                mBackend->log->Printf(LT_WARNING,
                                      L"AVFoundation audio Play rejected: id=%d offsetUs=%llu reason=play-returned-false",
                                      id,
                                      (unsigned long long)microSecondOffset);
            }
        }
        return accepted;
    }

    bool Stop(int id) override
    {
        AVFSound *sound = iGetSound(mBackend, id);
        if (!sound) return false;
        if (mBackend && mBackend->log)
        {
            mBackend->log->Printf(LT_MESSAGE, L"AVFoundation audio Stop: id=%d", id);
        }
        if (!sound->player) return false;
        [sound->player stop];
        sound->player.currentTime = 0.0;
        sound->stopped = true;
        sound->paused = false;
        return true;
    }

    bool Pause(int id) override
    {
        AVFSound *sound = iGetSound(mBackend, id);
        if (!sound) return false;
        if (mBackend && mBackend->log)
        {
            mBackend->log->Printf(LT_MESSAGE, L"AVFoundation audio Pause: id=%d", id);
        }
        if (!sound->player) return false;
        [sound->player pause];
        sound->paused = true;
        return true;
    }

    bool Resume(int id) override
    {
        AVFSound *sound = iGetSound(mBackend, id);
        if (!sound) return false;
        if (mBackend && mBackend->log)
        {
            mBackend->log->Printf(LT_MESSAGE, L"AVFoundation audio Resume: id=%d", id);
        }
        if (!sound->player) return false;
        sound->paused = false;
        sound->stopped = false;
        return [sound->player play] == YES;
    }

    PlaybackState GetPlaybackState(int id) override
    {
        AVFSound *sound = iGetSound(mBackend, id);
        if (!sound) return PlaybackState::Stopped;

        PlaybackState state = PlaybackState::Stopped;
        if (sound->stopped)
        {
            state = PlaybackState::Stopped;
        }
        else if (sound->paused)
        {
            state = PlaybackState::Paused;
        }
        else if (!sound->player)
        {
            state = PlaybackState::Stopped;
        }
        else
        {
            state = sound->player.playing ? PlaybackState::Playing : PlaybackState::Stopped;
        }

        iLogPlaybackState(mBackend, id, sound, state);
        if (state == PlaybackState::Playing)
        {
            iLogPlaybackProgress(mBackend, id, sound);
        }
        return state;
    }

    void PauseAllSounds(void) override
    {
        for (NSUInteger i = 0; i < [mBackend->sounds count]; ++i)
        {
            Pause((int)i);
        }
    }

    void ResumeAllSounds(void) override
    {
        for (NSUInteger i = 0; i < [mBackend->sounds count]; ++i)
        {
            Resume((int)i);
        }
    }

    void SetListener(const trans3d &) override {}
    float GetVolume(int id) const override { AVFSound *sound = iGetSound(mBackend, id); return sound ? sound->volume : 0.0f; }
    bool SetVolume(int id, float volume) override
    {
        AVFSound *sound = iGetSound(mBackend, id);
        if (!sound) return false;
        sound->volume = volume;
        if (sound->player) sound->player.volume = volume;
        return true;
    }
    bool GetLooping(int id) const override { AVFSound *sound = iGetSound(mBackend, id); return sound ? sound->looping : false; }
    void SetLooping(int id, bool looping) override
    {
        AVFSound *sound = iGetSound(mBackend, id);
        if (!sound) return;
        sound->looping = looping;
        if (sound->player) sound->player.numberOfLoops = looping ? -1 : 0;
    }
    SoundType GetType(int id) const override { AVFSound *sound = iGetSound(mBackend, id); return sound ? sound->type : SoundType::Flat; }
    bool ConvertType(int id, SoundType type) override { AVFSound *sound = iGetSound(mBackend, id); if (!sound) return false; sound->type = type; return type != SoundType::Ambisonic; }
    bool CanConvertType(int, SoundType type) const override { return type != SoundType::Ambisonic; }
    void SetAttenMode(int, AttenuationType) override {}
    void SetAttenMinMax(int, double, double) override {}
    void SetPosition(int, const double *) override {}
    void SetOrientation(int, const double *, const double *) override {}
    void SetPositionOrientation(int, const double *, const double *, const double *) override {}
    void SetModifierType(int, ModifierType) override {}
    void SetModifierCone(int, double, double, double) override {}
    void SetModifierFrustum(int, double, double, double, double) override {}
    float ComputeModifiers(int) override { return 1.0f; }

private:
    AVFBackend *mBackend;
};

piSoundEngineBackendAVFoundation::piSoundEngineBackendAVFoundation() = default;
piSoundEngineBackendAVFoundation::~piSoundEngineBackendAVFoundation() = default;

bool piSoundEngineBackendAVFoundation::doInit(piLog *log)
{
    AVFBackend *backend = new AVFBackend();
    backend->log = log;
    backend->sounds = [[NSMutableArray alloc] init];
    backend->engine = new piSoundEngineAVFoundation(backend);
    mData = backend;
    return true;
}

void piSoundEngineBackendAVFoundation::doDeinit(void)
{
    AVFBackend *backend = (AVFBackend *)mData;
    if (!backend) return;
    if (backend->sounds)
    {
        for (NSValue *value in backend->sounds)
        {
            iDestroySound(backend, (AVFSound *)[value pointerValue]);
        }
        [backend->sounds release];
    }
    if (backend->log)
    {
        backend->log->Printf(LT_MESSAGE,
                             L"AVFoundation audio Deinit complete: soundsDestroyed=%llu tempFilesRemoved=%llu tempFileRemoveFailures=%llu",
                             (unsigned long long)backend->soundsDestroyed,
                             (unsigned long long)backend->tempFilesRemoved,
                             (unsigned long long)backend->tempFileRemoveFailures);
    }
    [backend->tempDirectory release];
    delete backend->engine;
    delete backend;
    mData = nullptr;
}

bool piSoundEngineBackendAVFoundation::Init(void *, int, const Configuration *config)
{
    AVFBackend *backend = (AVFBackend *)mData;
    if (!backend) return false;
    if (config && config->mTempPath)
    {
        backend->tempDirectory = [[NSString stringWithUTF8String:config->mTempPath] retain];
    }
    else
    {
        backend->tempDirectory = [NSTemporaryDirectory() retain];
    }
    [[NSFileManager defaultManager] createDirectoryAtPath:backend->tempDirectory withIntermediateDirectories:YES attributes:nil error:nil];
    return true;
}

void piSoundEngineBackendAVFoundation::Deinit(void)
{
    AVFBackend *backend = (AVFBackend *)mData;
    if (!backend || !backend->sounds) return;
    for (NSValue *value in backend->sounds)
    {
        AVFSound *sound = (AVFSound *)[value pointerValue];
        if (sound && sound->player)
        {
            [sound->player stop];
        }
    }
}

int piSoundEngineBackendAVFoundation::GetNumDevices(void) { return 1; }
const wchar_t *piSoundEngineBackendAVFoundation::GetDeviceName(int) const { return L"Default macOS audio output"; }
int piSoundEngineBackendAVFoundation::GetDeviceFromGUID(void *) { return 0; }
int piSoundEngineBackendAVFoundation::GetDeviceFromName(const wchar_t *) { return 0; }
bool piSoundEngineBackendAVFoundation::ResizeMixBuffers(int const &, int const &) { return true; }
void piSoundEngineBackendAVFoundation::Tick(void) {}
piSoundEngine *piSoundEngineBackendAVFoundation::GetEngine(void)
{
    AVFBackend *backend = (AVFBackend *)mData;
    return backend ? backend->engine : nullptr;
}

} // namespace ImmCore
