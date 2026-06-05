//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015.
// See THIRD_PARTY_LICENSES.txt
//
#pragma once

#include "../piRenderer.h"

namespace ImmCore {

struct piVulkanState;

struct piVulkanExternalDevice
{
    void *instance;
    void *physicalDevice;
    void *device;
    void *graphicsQueue;
    uint32_t graphicsQueueFamilyIndex;
};

class piRendererVulkan : public piRenderer
{
public:
    piRendererVulkan();
    ~piRendererVulkan() override;

    bool Initialize(int id, const void **hwnd, int num, bool disableVSync, bool disableErrors, piReporter *reporter, bool createDevice, void *device) override;
    void Deinitialize(void) override;
    bool SupportsFeature(RendererFeature feature) override;
    API GetAPI(void) override;
    void Report(void) override;
    void SetActiveWindow(int id) override;
    void Enable(void) override;
    void Disable(void) override;
    void SwapBuffers(void) override;
    void *GetContext(void) override;
    bool BeginExternalImageFrame(void *image, uint32_t vkFormat, int width, int height, int arrayLayers);
    bool BeginExternalImageFrame(void *image, void *imageView, uint32_t vkFormat, int width, int height);
    void EndExternalImageFrame(void);

    void StartPerformanceMeasure(void) override;
    void EndPerformanceMeasure(void) override;
    uint64_t GetPerformanceMeasure(void) override;

    piRTarget CreateRenderTarget(piTexture vtex0, piTexture vtex1, piTexture vtex2, piTexture vtex3, piTexture zbuf) override;
    void DestroyRenderTarget(piRTarget obj) override;
    bool SetRenderTarget(piRTarget obj) override;
    void RenderTargetSampleLocations(piRTarget vdst, const float *locations) override;
    void BlitRenderTarget(piRTarget dst, piRTarget src, bool color, bool depth) override;
    void SetWriteMask(bool c0, bool c1, bool c2, bool c3, bool z) override;
    void SetShadingSamples(int shadingSamples) override;
    void RenderTargetGetDefaultSampleLocation(piRTarget vdst, const int id, float *location) override;

    void Clear(const float *color0, const float *color1, const float *color2, const float *color3, const bool depth0) override;
    void SetState(piState state, bool value) override;
    void SetBlending(int buf, BlendEquation equRGB, BlendOperations srcRGB, BlendOperations dstRGB, BlendEquation equALP, BlendOperations srcALP, BlendOperations dstALP) override;
    void SetViewport(int id, const int *vp) override;
    void SetViewports(int num, const float *viewports) override;
    void GetViewports(int *num, float *viewports) override;

    piRasterState CreateRasterState(bool wireframe, bool frontIsCounterClockWise, CullMode cullMode, bool depthClamp, bool multiSample) override;
    void SetRasterState(const piRasterState vme) override;
    void DestroyRasterState(piRasterState vme) override;
    piBlendState CreateBlendState(bool alphaToCoverage, bool enabled0) override;
    void SetBlendState(const piBlendState vme) override;
    void DestroyBlendState(piBlendState vme) override;
    piDepthState CreateDepthState(bool alphaToCoverage, bool lessEqual) override;
    void SetDepthState(const piDepthState vme) override;
    void DestroyDepthState(piDepthState vme) override;

    piTexture CreateTexture(const wchar_t *key, const TextureInfo *info, bool compress, TextureFilter filter, TextureWrap wrap, float aniso, const void *buffer) override;
    piTexture CreateTexture2(const wchar_t *key, const TextureInfo *info, bool compress, TextureFilter filter, TextureWrap wrap1, float aniso, const void *buffer, int bindUsage) override;
    void DestroyTexture(piTexture obj) override;
    void ClearTexture(piTexture vme, int level, const void *data) override;
    void UpdateTexture(piTexture me, int x0, int y0, int z0, int xres, int yres, int zres, const void *buffer) override;
    void GetTextureRes(piTexture me, int *res) override;
    void GetTextureFormat(piTexture me, Format *format) override;
    void GetTextureContent(piTexture me, void *data, const Format fmt) override;
    void GetTextureContent(piTexture vme, void *data, int x, int y, int z, int xres, int yres, int zres) override;
    void GetTextureInfo(piTexture me, TextureInfo *info) override;
    void GetTextureSampling(piTexture vme, TextureFilter *rfilter, TextureWrap *rwrap) override;
    void ComputeMipmaps(piTexture me) override;
    void AttachTextures(int num, piTexture vt0, piTexture vt1=0, piTexture vt2=0, piTexture vt3=0, piTexture vt4=0, piTexture vt5=0, piTexture vt6=0, piTexture vt7=0, piTexture vt8=0, piTexture vt9=0, piTexture vt10=0, piTexture vt11=0, piTexture vt12=0, piTexture vt13=0, piTexture vt14=0, piTexture vt15=0) override;
    void AttachTextures(int num, piTexture *vt, int offset) override;
    void DettachTextures(void) override;
    piTexture CreateTextureFromID(unsigned int id, TextureFilter filter) override;
    void MakeResident(piTexture vme) override;
    void MakeNonResident(piTexture vme) override;
    uint64_t GetTextureHandle(piTexture vme) override;

    piSampler CreateSampler(TextureFilter filter, TextureWrap wrap, float anisotropy) override;
    void DestroySampler(piSampler obj) override;
    void AttachSamplers(int num, piSampler vt0, piSampler vt1=0, piSampler vt2=0, piSampler vt3=0, piSampler vt4=0, piSampler vt5=0, piSampler vt6=0, piSampler vt7=0) override;
    void DettachSamplers(void) override;
    void AttachImage(int unit, piTexture texture, int level, bool layered, int layer, Format format) override;

    piShader CreateShader(const piShaderOptions *options, const char *vs, const char *cs, const char *es, const char *gs, const char *fs, char *error) override;
    piShader CreateShaderBinary(const piShaderOptions *options, const uint8_t *vs, const int vs_len, const uint8_t *cs, const int cs_len, const uint8_t *es, const int es_len, const uint8_t *gs, const int gs_len, const uint8_t *fs, const int fs_len, char *error) override;
    void DestroyShader(piShader obj) override;
    void AttachShader(piShader obj) override;
    void DettachShader(void) override;
    piShader CreateCompute(const piShaderOptions *options, const char *cs, char *error) override;

    void SetShaderConstant4F(const unsigned int pos, const float *value, int num) override;
    void SetShaderConstant3F(const unsigned int pos, const float *value, int num) override;
    void SetShaderConstant2F(const unsigned int pos, const float *value, int num) override;
    void SetShaderConstant1F(const unsigned int pos, const float *value, int num) override;
    void SetShaderConstant1I(const unsigned int pos, const int *value, int num) override;
    void SetShaderConstant1UI(const unsigned int pos, const unsigned int *value, int num) override;
    void SetShaderConstant2UI(const unsigned int pos, const unsigned int *value, int num) override;
    void SetShaderConstant3UI(const unsigned int pos, const unsigned int *value, int num) override;
    void SetShaderConstant4UI(const unsigned int pos, const unsigned int *value, int num) override;
    void SetShaderConstantMat4F(const unsigned int pos, const float *value, int num, bool transpose) override;
    void SetShaderConstantSampler(const unsigned int pos, int unit) override;
    void AttachShaderConstants(piBuffer obj, int unit) override;
    void AttachShaderBuffer(piBuffer obj, int unit) override;
    void DettachShaderBuffer(int unit) override;
    void AttachAtomicsBuffer(piBuffer obj, int unit) override;
    void DettachAtomicsBuffer(int unit) override;

    piBuffer CreateBuffer(const void *data, unsigned int amount, BufferType mode, BufferUse use) override;
    piBuffer CreateStructuredBuffer(const void *data, unsigned int numElements, unsigned int elementSize, BufferType mode, BufferUse use) override;
    piBuffer CreateBufferMapped_Start(void **ptr, unsigned int amount, BufferUse use) override;
    void CreateBufferMapped_End(piBuffer vme) override;
    void DestroyBuffer(piBuffer obj) override;
    void UpdateBuffer(piBuffer obj, const void *data, int offset, int len, bool invalidate=false) override;
    void AttachPixelPackBuffer(piBuffer obj) override;
    void DettachPixelPackBuffer(void) override;

    piVertexArray CreateVertexArray(int numStreams, piBuffer vb0, const piRArrayLayout *streamLayout0, piBuffer vb1, const piRArrayLayout *streamLayout1, piBuffer eb, const IndexArrayFormat ebFormat) override;
    void DestroyVertexArray(piVertexArray obj) override;
    void AttachVertexArray(piVertexArray obj) override;
    void DettachVertexArray(void) override;
    piVertexArray CreateVertexArray2(int numStreams, piBuffer vb0, const ArrayLayout2 *streamLayout0, piBuffer vb1, const ArrayLayout2 *streamLayout1, const void *shaderBinary, size_t shaderBinarySize, piBuffer ib, const IndexArrayFormat ebFormat) override;
    void AttachVertexArray2(piVertexArray vme) override;
    void DestroyVertexArray2(piVertexArray vme) override;

    piQuery CreateQuery(piRenderer::QueryType type) override;
    void DestroyQuery(piQuery vme) override;
    void BeginQuery(piQuery vme) override;
    void EndQuery(piQuery vme) override;
    uint64_t GetQueryResult(piQuery vme) override;

    void DrawPrimitiveIndexed(PrimitiveType pt, uint32_t num, uint32_t numInstances, uint32_t baseVertex, uint32_t baseInstance, uint32_t baseIndex) override;
    void DrawPrimitiveIndirect(PrimitiveType pt, piBuffer cmds, uint32_t offset, uint32_t num) override;
    void DrawPrimitiveNotIndexed(PrimitiveType pt, int first, int num, int numInstances) override;
    void DrawPrimitiveNotIndexedMultiple(PrimitiveType pt, const int *firsts, const int *counts, int num) override;
    void DrawPrimitiveNotIndexedIndirect(PrimitiveType pt, piBuffer cmds, int num) override;
    void DettachIndirectBuffer(void) override;
    void DrawUnitCube_XYZ_NOR(int numInstanced) override;
    void DrawUnitCube_XYZ(int numInstanced) override;
    void DrawUnitQuad_XY(int numInstanced) override;
    void ExecuteCompute(int ngx, int ngy, int ngz, int gsx, int gsy, int gsz) override;
    void CreateSyncObject(piBuffer &buffer) override;
    bool CheckSyncObject(piBuffer &buffer) override;
    void SetPointSize(bool mode, float size) override;
    void SetLineWidth(float size) override;
    void PolygonOffset(bool mode, bool wireframe, float a, float b) override;
    void RenderMemoryBarrier(BarrierType type) override;

private:
    bool BeginExternalImageFrameWithView(void *image, void *imageView, uint32_t vkFormat, int width, int height, int arrayLayers, bool ownsImageView);

    piVulkanState *mState;
    piReporter *mReporter;
};

} // namespace ImmCore
