#pragma once

#include "../piSoundEngineBackend.h"

namespace ImmCore
{

class piSoundEngineBackendAVFoundation final : public piSoundEngineBackend
{
public:
    piSoundEngineBackendAVFoundation();
    ~piSoundEngineBackendAVFoundation() override;

    bool Init(void *hwnd, int deviceID, const Configuration *config) override;
    void Deinit(void) override;
    int GetNumDevices(void) override;
    const wchar_t *GetDeviceName(int id) const override;
    int GetDeviceFromGUID(void *deviceGUID) override;
    int GetDeviceFromName(const wchar_t *name) override;
    bool ResizeMixBuffers(int const &mixSamples, int const &spatialSamples) override;
    void Tick(void) override;
    piSoundEngine *GetEngine(void) override;

protected:
    bool doInit(piLog *log) override;
    void doDeinit(void) override;

private:
    void *mData = nullptr;
};

} // namespace ImmCore
