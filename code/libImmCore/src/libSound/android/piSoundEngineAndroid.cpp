//
// Branched off piLibs (Copyright (c) 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include "piSoundEngineAndroid.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "../../libBasics/piLog.h"
#include "../../libBasics/piTypes.h"

namespace ImmCore
{
    namespace
    {
        static bool DecodeOpusToPcm(const char *tempDir, const uint8_t *data, size_t size,
                                    int &outRate, int &outChannels, std::vector<int16_t> &outPcm,
                                    piLog *log)
        {
            if (!tempDir || !*tempDir || !data || size == 0)
                return false;
            if (log)
                log->Printf(LT_MESSAGE, L"Opus decode: start size=%llu", static_cast<unsigned long long>(size));

            std::string dir = tempDir;
            if (!dir.empty() && (dir.back() == '/' || dir.back() == '\\'))
                dir.pop_back();

            std::string pattern = dir + "/imm_audio_XXXXXX";
            std::vector<char> path(pattern.begin(), pattern.end());
            path.push_back('\0');

            int fd = mkstemp(path.data());
            if (fd < 0)
            {
                if (log)
                    log->Printf(LT_WARNING, L"Opus decode: could not create temp file");
                return false;
            }

            size_t written = 0;
            while (written < size)
            {
                const ssize_t w = write(fd, data + written, size - written);
                if (w <= 0)
                    break;
                written += static_cast<size_t>(w);
            }
            lseek(fd, 0, SEEK_SET);

            AMediaExtractor *extractor = AMediaExtractor_new();
            if (!extractor)
            {
                close(fd);
                unlink(path.data());
                return false;
            }
            media_status_t status = AMediaExtractor_setDataSourceFd(extractor, fd, 0, static_cast<off64_t>(size));
            if (status != AMEDIA_OK)
            {
                if (log)
                    log->Printf(LT_WARNING, L"Opus decode: setDataSource failed (%d)", int(status));
                AMediaExtractor_delete(extractor);
                close(fd);
                unlink(path.data());
                return false;
            }

            int audioTrack = -1;
            AMediaFormat *trackFormat = nullptr;
            const char *mime = nullptr;
            const int trackCount = AMediaExtractor_getTrackCount(extractor);
            if (log)
                log->Printf(LT_MESSAGE, L"Opus decode: track count=%d", trackCount);
            for (int i = 0; i < trackCount; ++i)
            {
                AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, i);
                const char *trackMime = nullptr;
                if (format && AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &trackMime))
                {
                    if (trackMime && strncmp(trackMime, "audio/", 6) == 0)
                    {
                        audioTrack = i;
                        trackFormat = format;
                        mime = trackMime;
                        if (log)
                            log->Printf(LT_MESSAGE, L"Opus decode: using audio track %d", audioTrack);
                        break;
                    }
                }
                if (format)
                    AMediaFormat_delete(format);
            }

            if (audioTrack < 0 || !trackFormat || !mime)
            {
                if (log)
                    log->Printf(LT_WARNING, L"Opus decode: no audio track found (tracks=%d)", trackCount);
                if (trackFormat)
                    AMediaFormat_delete(trackFormat);
                AMediaExtractor_delete(extractor);
                close(fd);
                unlink(path.data());
                return false;
            }

            AMediaExtractor_selectTrack(extractor, audioTrack);

            int32_t sampleRate = 0;
            int32_t channels = 0;
            AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate);
            AMediaFormat_getInt32(trackFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);

            AMediaCodec *codec = AMediaCodec_createDecoderByType(mime);
            if (!codec)
            {
                if (log)
                    log->Printf(LT_WARNING, L"Opus decode: no decoder for mime");
                AMediaFormat_delete(trackFormat);
                AMediaExtractor_delete(extractor);
                close(fd);
                unlink(path.data());
                return false;
            }

            status = AMediaCodec_configure(codec, trackFormat, nullptr, nullptr, 0);
            if (status != AMEDIA_OK)
            {
                if (log)
                    log->Printf(LT_WARNING, L"Opus decode: codec configure failed (%d)", int(status));
                AMediaCodec_delete(codec);
                AMediaFormat_delete(trackFormat);
                AMediaExtractor_delete(extractor);
                close(fd);
                unlink(path.data());
                return false;
            }

            AMediaFormat_delete(trackFormat);
            AMediaCodec_start(codec);

            bool sawInputEOS = false;
            bool sawOutputEOS = false;
            outRate = sampleRate > 0 ? sampleRate : 48000;
            outChannels = channels > 0 ? channels : 2;
            outPcm.clear();

            int emptyLoops = 0;
            while (!sawOutputEOS && emptyLoops < 2000)
            {
                if (!sawInputEOS)
                {
                    const ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec, 2000);
                    if (inputIndex >= 0)
                    {
                        size_t inputSize = 0;
                        uint8_t *inputBuffer = AMediaCodec_getInputBuffer(codec, inputIndex, &inputSize);
                        const ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, inputBuffer, inputSize);
                        if (sampleSize < 0)
                        {
                            AMediaCodec_queueInputBuffer(codec, inputIndex, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                            sawInputEOS = true;
                        }
                        else
                        {
                            const int64_t pts = AMediaExtractor_getSampleTime(extractor);
                            AMediaCodec_queueInputBuffer(codec, inputIndex, 0, sampleSize, pts, 0);
                            AMediaExtractor_advance(extractor);
                        }
                    }
                }

                AMediaCodecBufferInfo info;
                const ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, 2000);
                if (outputIndex >= 0)
                {
                    size_t outputSize = 0;
                    uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(codec, outputIndex, &outputSize);
                    if (info.size > 0 && outputBuffer)
                    {
                        const uint8_t *dataPtr = outputBuffer + info.offset;
                        const size_t sampleCount = info.size / sizeof(int16_t);
                        const int16_t *src = reinterpret_cast<const int16_t*>(dataPtr);
                        outPcm.insert(outPcm.end(), src, src + sampleCount);
                    }
                    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
                        sawOutputEOS = true;
                    AMediaCodec_releaseOutputBuffer(codec, outputIndex, false);
                    emptyLoops = 0;
                }
                else if (outputIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
                {
                    AMediaFormat *outFormat = AMediaCodec_getOutputFormat(codec);
                    if (outFormat)
                    {
                        int32_t fmtRate = 0;
                        int32_t fmtChannels = 0;
                        if (AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &fmtRate))
                            outRate = fmtRate;
                        if (AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &fmtChannels))
                            outChannels = fmtChannels;
                        AMediaFormat_delete(outFormat);
                    }
                    emptyLoops = 0;
                }
                else
                {
                    emptyLoops++;
                }
            }

            AMediaCodec_stop(codec);
            AMediaCodec_delete(codec);
            AMediaExtractor_delete(extractor);
            close(fd);
            unlink(path.data());

            if (outPcm.empty() && log)
                log->Printf(LT_WARNING, L"Opus decode: no PCM output produced");
            return !outPcm.empty();
        }
    }

    struct AndroidSound
    {
        std::vector<int16_t> mData;
        int mNumChannels = 0;
        int mRate = 0;
        uint64_t mFrames = 0;
        double mCursor = 0.0;
        bool mLooping = false;
        bool mPlaying = false;
        bool mPaused = false;
        float mVolume = 1.0f;
        piSoundEngine::SoundType mType = piSoundEngine::SoundType::Flat;
    };

    class piSoundEngineAndroid final : public piSoundEngine
    {
    public:
        piSoundEngineAndroid(piLog *log, int outputRate, std::string tempDir)
            : mLog(log)
            , mOutputRate(outputRate)
            , mTempDir(std::move(tempDir))
        {
        }

        int AddSound(int numChannels, uint64_t length, const void *buffer, bool makePositional) override
        {
            if (numChannels <= 0)
                return -1;
            if (!buffer || length == 0)
                return -1;

            std::vector<int16_t> decoded;
            int decodedRate = mOutputRate;
            int decodedChannels = numChannels;
            if (!DecodeOpusToPcm(mTempDir.c_str(), reinterpret_cast<const uint8_t*>(buffer),
                                 static_cast<size_t>(length), decodedRate, decodedChannels, decoded, mLog))
            {
                if (mLog)
                    mLog->Printf(LT_WARNING, L"Failed to decode OPUS audio on Android; using silent placeholder");

                AndroidSound silent;
                silent.mNumChannels = numChannels;
                silent.mRate = mOutputRate;
                silent.mFrames = 0;
                silent.mCursor = 0.0;
                silent.mLooping = false;
                silent.mPlaying = false;
                silent.mPaused = false;
                silent.mVolume = 0.0f;
                if (makePositional)
                    silent.mType = SoundType::Positional;
                else if (numChannels > 2)
                    silent.mType = SoundType::Ambisonic;
                else
                    silent.mType = SoundType::Flat;

                std::lock_guard<std::mutex> lock(mMutex);
                int id = -1;
                if (!mFreeIds.empty())
                {
                    id = mFreeIds.back();
                    mFreeIds.pop_back();
                    mSounds[id] = std::move(silent);
                }
                else
                {
                    id = static_cast<int>(mSounds.size());
                    mSounds.push_back(std::move(silent));
                }
                return id;
            }

            AndroidSound sound;
            sound.mNumChannels = decodedChannels;
            sound.mRate = decodedRate > 0 ? decodedRate : mOutputRate;
            sound.mFrames = decodedChannels > 0 ? (decoded.size() / decodedChannels) : 0;
            sound.mData = std::move(decoded);
            sound.mCursor = 0.0;
            sound.mLooping = false;
            sound.mPlaying = false;
            sound.mPaused = false;
            sound.mVolume = 1.0f;
            if (makePositional)
                sound.mType = SoundType::Positional;
            else if (decodedChannels > 2)
                sound.mType = SoundType::Ambisonic;
            else
                sound.mType = SoundType::Flat;

            std::lock_guard<std::mutex> lock(mMutex);
            int id = -1;
            if (!mFreeIds.empty())
            {
                id = mFreeIds.back();
                mFreeIds.pop_back();
                mSounds[id] = std::move(sound);
            }
            else
            {
                id = static_cast<int>(mSounds.size());
                mSounds.push_back(std::move(sound));
            }
            return id;
        }

        int AddSound(void(*callback)(float* channelBuffer, size_t numSamples, size_t numChannels, void* userData), void * userData) override
        {
            return -1;
        }

        int AddSound(const wchar_t *filename, bool makePositional) override
        {
            return -1;
        }

        int AddSound(const int frequency, int precision, int numChannels, uint64_t length, const void *buffer, bool makePositional) override
        {
            if (buffer == nullptr || length == 0 || numChannels <= 0)
                return -1;
            if (precision != 16 && precision != 8)
                return -1;

            AndroidSound sound;
            sound.mNumChannels = numChannels;
            sound.mRate = frequency > 0 ? frequency : mOutputRate;
            sound.mFrames = length / (numChannels * (precision / 8));
            sound.mData.resize(sound.mFrames * numChannels);

            if (precision == 16)
            {
                const int16_t *src = reinterpret_cast<const int16_t*>(buffer);
                std::copy(src, src + sound.mData.size(), sound.mData.begin());
            }
            else
            {
                const uint8_t *src = reinterpret_cast<const uint8_t*>(buffer);
                for (size_t i = 0; i < sound.mData.size(); ++i)
                {
                    sound.mData[i] = static_cast<int16_t>((static_cast<int>(src[i]) - 128) << 8);
                }
            }

            if (makePositional)
                sound.mType = SoundType::Positional;
            else if (numChannels > 2)
                sound.mType = SoundType::Ambisonic;
            else
                sound.mType = SoundType::Flat;

            std::lock_guard<std::mutex> lock(mMutex);
            int id = -1;
            if (!mFreeIds.empty())
            {
                id = mFreeIds.back();
                mFreeIds.pop_back();
                mSounds[id] = std::move(sound);
            }
            else
            {
                id = static_cast<int>(mSounds.size());
                mSounds.push_back(std::move(sound));
            }
            return id;
        }

        void DelSound(int id) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return;
            mSounds[id] = AndroidSound();
            mFreeIds.push_back(id);
        }

        bool Play(int id, uint64_t offset) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return false;
            AndroidSound &sound = mSounds[id];
            if (sound.mData.empty())
                return false;
            const double offsetFrames = (sound.mRate > 0) ? (static_cast<double>(offset) * sound.mRate / 1000000.0) : 0.0;
            sound.mCursor = std::max(0.0, std::min(offsetFrames, static_cast<double>(sound.mFrames)));
            sound.mPlaying = true;
            sound.mPaused = false;
            return true;
        }

        bool Stop(int id) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return false;
            AndroidSound &sound = mSounds[id];
            sound.mPlaying = false;
            sound.mPaused = false;
            sound.mCursor = 0.0;
            return true;
        }

        bool Pause(int id) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return false;
            AndroidSound &sound = mSounds[id];
            if (!sound.mPlaying)
                return false;
            sound.mPaused = true;
            return true;
        }

        bool Resume(int id) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return false;
            AndroidSound &sound = mSounds[id];
            if (!sound.mPlaying)
                return false;
            sound.mPaused = false;
            return true;
        }

        void PauseAllSounds(void) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto &sound : mSounds)
            {
                if (sound.mPlaying)
                    sound.mPaused = true;
            }
        }

        void ResumeAllSounds(void) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto &sound : mSounds)
            {
                if (sound.mPlaying)
                    sound.mPaused = false;
            }
        }

        void SetListener(const trans3d &listenerToWorld) override {}
        void SetPosition(int id, const double *pos) override {}
        void SetOrientation(int id, const double *dir, const double *up) override {}
        void SetPositionOrientation(int id, const double *pos, const double *dir, const double *up) override {}

        PlaybackState GetPlaybackState(int id) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return PlaybackState::Stopped;
            const AndroidSound &sound = mSounds[id];
            if (!sound.mPlaying)
                return PlaybackState::Stopped;
            return sound.mPaused ? PlaybackState::Paused : PlaybackState::Playing;
        }

        float GetVolume(int id) const override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return 0.0f;
            return mSounds[id].mVolume;
        }

        bool SetVolume(int id, float volume) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return false;
            mSounds[id].mVolume = volume;
            return true;
        }

        SoundType GetType(int id) const override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return SoundType::Flat;
            return mSounds[id].mType;
        }

        bool ConvertType(int id, SoundType type) override { return false; }
        bool CanConvertType(int id, SoundType type) const override { return false; }

        bool GetLooping(int id) const override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return false;
            return mSounds[id].mLooping;
        }

        void SetLooping(int id, bool looping) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (id < 0 || id >= static_cast<int>(mSounds.size()))
                return;
            mSounds[id].mLooping = looping;
        }

        void SetAttenMode(int id, AttenuationType attenMode) override {}
        void SetAttenMinMax(int id, double attenMin, double attenMax) override {}
        void SetModifierType(int id, ModifierType modifType) override {}
        void SetModifierCone(int id, double angleInner, double angleBand, double attenOut) override {}
        void SetModifierFrustum(int id, double angleInnerX, double angleInnerY, double angleBand, double attenOut) override {}
        float ComputeModifiers(int id) override { return 1.0f; }

        void Mix(float *mixBuffer, int frames)
        {
            std::fill(mixBuffer, mixBuffer + frames * 2, 0.0f);

            std::lock_guard<std::mutex> lock(mMutex);
            for (auto &sound : mSounds)
            {
                if (!sound.mPlaying || sound.mPaused || sound.mData.empty() || sound.mFrames == 0)
                    continue;

                const double step = (sound.mRate > 0) ? (static_cast<double>(sound.mRate) / static_cast<double>(mOutputRate)) : 1.0;
                double cursor = sound.mCursor;

                for (int i = 0; i < frames; ++i)
                {
                    if (cursor >= static_cast<double>(sound.mFrames))
                    {
                        if (sound.mLooping)
                        {
                            cursor = std::fmod(cursor, static_cast<double>(sound.mFrames));
                        }
                        else
                        {
                            sound.mPlaying = false;
                            break;
                        }
                    }

                    const uint64_t idx = static_cast<uint64_t>(cursor);
                    const float frac = static_cast<float>(cursor - static_cast<double>(idx));
                    const uint64_t idxNext = (idx + 1 < sound.mFrames) ? (idx + 1) : idx;

                    auto sampleAt = [&](int ch, uint64_t frame) -> float
                    {
                        const uint64_t offset = frame * sound.mNumChannels + static_cast<uint64_t>(ch);
                        return static_cast<float>(sound.mData[offset]) / 32768.0f;
                    };

                    float left = 0.0f;
                    float right = 0.0f;
                    if (sound.mNumChannels == 1)
                    {
                        const float s0 = sampleAt(0, idx);
                        const float s1 = sampleAt(0, idxNext);
                        const float s = s0 + (s1 - s0) * frac;
                        left = s;
                        right = s;
                    }
                    else
                    {
                        const int chL = 0;
                        const int chR = (sound.mNumChannels > 1) ? 1 : 0;
                        const float l0 = sampleAt(chL, idx);
                        const float l1 = sampleAt(chL, idxNext);
                        const float r0 = sampleAt(chR, idx);
                        const float r1 = sampleAt(chR, idxNext);
                        left = l0 + (l1 - l0) * frac;
                        right = r0 + (r1 - r0) * frac;
                    }

                    mixBuffer[2 * i] += left * sound.mVolume;
                    mixBuffer[2 * i + 1] += right * sound.mVolume;

                    cursor += step;
                }

                sound.mCursor = cursor;
            }

            if (mTestToneEnabled)
            {
                const double twoPi = 6.283185307179586;
                const double step = twoPi * mTestToneHz / static_cast<double>(mOutputRate);
                for (int i = 0; i < frames; ++i)
                {
                    const float s = static_cast<float>(std::sin(mTestTonePhase) * mTestToneAmp);
                    mixBuffer[2 * i] += s;
                    mixBuffer[2 * i + 1] += s;
                    mTestTonePhase += step;
                    if (mTestTonePhase >= twoPi)
                        mTestTonePhase -= twoPi;
                }
            }
        }

    private:
        piLog *mLog = nullptr;
        int mOutputRate = 48000;
        std::string mTempDir;
        mutable std::mutex mMutex;
        std::vector<AndroidSound> mSounds;
        std::vector<int> mFreeIds;
        bool mTestToneEnabled = false;
        double mTestTonePhase = 0.0;
        double mTestToneHz = 440.0;
        float mTestToneAmp = 0.1f;
    };

    struct AndroidAudioBackend
    {
        piLog *log = nullptr;
        int sampleRate = 48000;
        int framesPerBuffer = 512;
        std::atomic<bool> running{false};
        std::string tempDir;

        SLObjectItf engineObject = nullptr;
        SLEngineItf engineEngine = nullptr;
        SLObjectItf outputMixObject = nullptr;
        SLObjectItf playerObject = nullptr;
        SLPlayItf playItf = nullptr;
        SLAndroidSimpleBufferQueueItf bufferQueue = nullptr;

        std::vector<int16_t> buffers[2];
        std::vector<float> mixBuffer;
        int bufferIndex = 0;

        piSoundEngineAndroid *engine = nullptr;
    };

    static void BufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
    {
        AndroidAudioBackend *backend = reinterpret_cast<AndroidAudioBackend*>(context);
        if (!backend || !backend->running.load())
            return;

        backend->bufferIndex = (backend->bufferIndex + 1) % 2;
        std::vector<int16_t> &buffer = backend->buffers[backend->bufferIndex];
        backend->mixBuffer.resize(backend->framesPerBuffer * 2);
        backend->engine->Mix(backend->mixBuffer.data(), backend->framesPerBuffer);

        for (int i = 0; i < backend->framesPerBuffer * 2; ++i)
        {
            float v = backend->mixBuffer[i];
            v = std::max(-1.0f, std::min(1.0f, v));
            buffer[i] = static_cast<int16_t>(v * 32767.0f);
        }

        (*backend->bufferQueue)->Enqueue(backend->bufferQueue, buffer.data(),
            buffer.size() * sizeof(int16_t));
    }

    piSoundEngineBackendAndroid::piSoundEngineBackendAndroid() = default;
    piSoundEngineBackendAndroid::~piSoundEngineBackendAndroid() = default;

    bool piSoundEngineBackendAndroid::doInit(piLog *log)
    {
        auto *backend = new AndroidAudioBackend();
        if (!backend)
            return false;
        backend->log = log;
        mData = backend;
        return true;
    }

    void piSoundEngineBackendAndroid::doDeinit(void)
    {
        auto *backend = reinterpret_cast<AndroidAudioBackend*>(mData);
        delete backend;
        mData = nullptr;
    }

    bool piSoundEngineBackendAndroid::Init(void *hwnd, int deviceID, const Configuration* config)
    {
        auto *backend = reinterpret_cast<AndroidAudioBackend*>(mData);
        if (!backend)
            return false;

        if (config)
        {
            if (config->mSampleRate > 0)
                backend->sampleRate = config->mSampleRate;
            if (config->mBufferSize > 0)
                backend->framesPerBuffer = config->mBufferSize;
            if (config->mTempPath)
                backend->tempDir = config->mTempPath;
        }

        backend->engine = new piSoundEngineAndroid(backend->log, backend->sampleRate, backend->tempDir);
        if (!backend->engine)
            return false;

        SLresult result = slCreateEngine(&backend->engineObject, 0, nullptr, 0, nullptr, nullptr);
        if (result != SL_RESULT_SUCCESS)
            return false;
        result = (*backend->engineObject)->Realize(backend->engineObject, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS)
            return false;
        result = (*backend->engineObject)->GetInterface(backend->engineObject, SL_IID_ENGINE, &backend->engineEngine);
        if (result != SL_RESULT_SUCCESS)
            return false;

        result = (*backend->engineEngine)->CreateOutputMix(backend->engineEngine, &backend->outputMixObject, 0, nullptr, nullptr);
        if (result != SL_RESULT_SUCCESS)
            return false;
        result = (*backend->outputMixObject)->Realize(backend->outputMixObject, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS)
            return false;

        SLDataLocator_AndroidSimpleBufferQueue locBufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
        SLDataFormat_PCM formatPcm = {
            SL_DATAFORMAT_PCM,
            2,
            static_cast<SLuint32>(backend->sampleRate * 1000),
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN
        };
        SLDataSource audioSrc = {&locBufq, &formatPcm};

        SLDataLocator_OutputMix locOutmix = {SL_DATALOCATOR_OUTPUTMIX, backend->outputMixObject};
        SLDataSink audioSnk = {&locOutmix, nullptr};

        const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
        const SLboolean req[] = {SL_BOOLEAN_TRUE};
        result = (*backend->engineEngine)->CreateAudioPlayer(backend->engineEngine, &backend->playerObject, &audioSrc, &audioSnk, 1, ids, req);
        if (result != SL_RESULT_SUCCESS)
            return false;
        result = (*backend->playerObject)->Realize(backend->playerObject, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS)
            return false;
        result = (*backend->playerObject)->GetInterface(backend->playerObject, SL_IID_PLAY, &backend->playItf);
        if (result != SL_RESULT_SUCCESS)
            return false;
        result = (*backend->playerObject)->GetInterface(backend->playerObject, SL_IID_BUFFERQUEUE, &backend->bufferQueue);
        if (result != SL_RESULT_SUCCESS)
            return false;

        backend->buffers[0].assign(backend->framesPerBuffer * 2, 0);
        backend->buffers[1].assign(backend->framesPerBuffer * 2, 0);
        backend->mixBuffer.assign(backend->framesPerBuffer * 2, 0.0f);

        result = (*backend->bufferQueue)->RegisterCallback(backend->bufferQueue, BufferQueueCallback, backend);
        if (result != SL_RESULT_SUCCESS)
            return false;

        backend->running.store(true);
        result = (*backend->playItf)->SetPlayState(backend->playItf, SL_PLAYSTATE_PLAYING);
        if (result != SL_RESULT_SUCCESS)
            return false;

        (*backend->bufferQueue)->Enqueue(backend->bufferQueue, backend->buffers[0].data(),
            backend->buffers[0].size() * sizeof(int16_t));
        (*backend->bufferQueue)->Enqueue(backend->bufferQueue, backend->buffers[1].data(),
            backend->buffers[1].size() * sizeof(int16_t));

        return true;
    }

    void piSoundEngineBackendAndroid::Deinit(void)
    {
        auto *backend = reinterpret_cast<AndroidAudioBackend*>(mData);
        if (!backend)
            return;

        backend->running.store(false);

        if (backend->playItf)
            (*backend->playItf)->SetPlayState(backend->playItf, SL_PLAYSTATE_STOPPED);
        if (backend->bufferQueue)
            (*backend->bufferQueue)->Clear(backend->bufferQueue);
        if (backend->playerObject)
        {
            (*backend->playerObject)->Destroy(backend->playerObject);
            backend->playerObject = nullptr;
            backend->playItf = nullptr;
            backend->bufferQueue = nullptr;
        }
        if (backend->outputMixObject)
        {
            (*backend->outputMixObject)->Destroy(backend->outputMixObject);
            backend->outputMixObject = nullptr;
        }
        if (backend->engineObject)
        {
            (*backend->engineObject)->Destroy(backend->engineObject);
            backend->engineObject = nullptr;
            backend->engineEngine = nullptr;
        }

        delete backend->engine;
        backend->engine = nullptr;
    }

    int piSoundEngineBackendAndroid::GetNumDevices(void)
    {
        return 1;
    }

    const wchar_t *piSoundEngineBackendAndroid::GetDeviceName(int id) const
    {
        return L"Android";
    }

    int piSoundEngineBackendAndroid::GetDeviceFromGUID(void *deviceGUID)
    {
        return 0;
    }

    int piSoundEngineBackendAndroid::GetDeviceFromName(const wchar_t *name)
    {
        return 0;
    }

    bool piSoundEngineBackendAndroid::ResizeMixBuffers(int const& mixSamples, int const& spatialSamples)
    {
        return true;
    }

    void piSoundEngineBackendAndroid::Tick(void)
    {
    }

    piSoundEngine *piSoundEngineBackendAndroid::GetEngine(void)
    {
        auto *backend = reinterpret_cast<AndroidAudioBackend*>(mData);
        return backend ? backend->engine : nullptr;
    }
}
