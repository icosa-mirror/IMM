//
// Branched off piLibs (Copyright (c) 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#pragma once

#include "../piSoundEngineBackend.h"

namespace ImmCore
{
    class piSoundEngineBackendAndroid final : public piSoundEngineBackend
    {
    public:
        piSoundEngineBackendAndroid();
        ~piSoundEngineBackendAndroid() override;

        bool           Init(void *hwnd, int deviceID, const Configuration* config) override;
        void           Deinit(void) override;
        int            GetNumDevices(void) override;
        const wchar_t *GetDeviceName(int id) const override;
        int            GetDeviceFromGUID(void *deviceGUID) override;
        int            GetDeviceFromName(const wchar_t *name) override;
        bool           ResizeMixBuffers(int const& mixSamples, int const& spatialSamples) override;
        void           Tick(void) override;
        piSoundEngine *GetEngine(void) override;

    private:
        bool doInit(piLog *log) override;
        void doDeinit(void) override;

    private:
        void *mData = nullptr;
    };
}
