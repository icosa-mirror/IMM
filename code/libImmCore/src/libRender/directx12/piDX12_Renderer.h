#pragma once
#include "../piRenderer.h"
#include "piDX12_CommandContext.h"
#include <memory>
#include <stdexcept>

namespace ImmCore
{
// In-progress host-only backend. Not registered in piRenderer::Create until
// required scene operations are implemented and validated. Missing operations
// fail explicitly; they must not silently report a rendered scene.
class piRendererDX12 final : public piRenderer
{
public:
    piRendererDX12();
    ~piRendererDX12() override;
    bool InitializeExternal(ID3D12Device* device, ID3D12CommandQueue* queue);
    HRESULT BeginFrame();
    HRESULT EndFrame(uint64_t* completion);
    HRESULT WaitForFrame(uint64_t completion);
    struct ExternalTarget
    {
        ID3D12Resource* color = nullptr;
        ID3D12Resource* depth = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
        DXGI_FORMAT colorFormat = DXGI_FORMAT_UNKNOWN;
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
        D3D12_RESOURCE_STATES colorState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES depthState = D3D12_RESOURCE_STATE_COMMON;
    };
    // One attachment set per frame. Transitions are restored by EndFrame.
    HRESULT SetExternalTarget(const ExternalTarget& target);
    // Borrowed GPU resource; the consuming draw must retain it until completion.
    ID3D12Resource* BufferResource(piBuffer buffer) const;
    bool Initialize(int id, const void **hwnd, int num, bool disableVSync, bool disableErrors, piReporter *reporter, bool createDevice, void *device) override;
    void Deinitialize() override;
    bool SupportsFeature(RendererFeature feature) override;
    API GetAPI() override;
    void Report() override;
    void SetActiveWindow(int id) override { (void)id; Unsupported("SetActiveWindow"); }
    void Enable() override { Unsupported("Enable"); }
    void Disable() override { Unsupported("Disable"); }
    void SwapBuffers() override { Unsupported("SwapBuffers"); }
    void * GetContext() override;
    void StartPerformanceMeasure() override { Unsupported("StartPerformanceMeasure"); }
    void EndPerformanceMeasure() override { Unsupported("EndPerformanceMeasure"); }
    uint64_t GetPerformanceMeasure() override { Unsupported("GetPerformanceMeasure"); }
    piRTarget CreateRenderTarget(piTexture vtex0, piTexture vtex1, piTexture vtex2, piTexture vtex3, piTexture zbuf) override { (void)vtex0; (void)vtex1; (void)vtex2; (void)vtex3; (void)zbuf; Unsupported("CreateRenderTarget"); }
    void DestroyRenderTarget(piRTarget obj) override { (void)obj; Unsupported("DestroyRenderTarget"); }
    bool SetRenderTarget(piRTarget obj) override { (void)obj; Unsupported("SetRenderTarget"); }
    void RenderTargetSampleLocations(piRTarget vdst, const float *locations) override { (void)vdst; (void)locations; Unsupported("RenderTargetSampleLocations"); }
    void BlitRenderTarget(piRTarget dst, piRTarget src, bool color, bool depth) override { (void)dst; (void)src; (void)color; (void)depth; Unsupported("BlitRenderTarget"); }
    void SetWriteMask(bool c0, bool c1, bool c2, bool c3, bool z) override;
    void SetShadingSamples(int shadingSamples) override { (void)shadingSamples; Unsupported("SetShadingSamples"); }
    void RenderTargetGetDefaultSampleLocation(piRTarget vdst, const int id, float *location) override { (void)vdst; (void)id; (void)location; Unsupported("RenderTargetGetDefaultSampleLocation"); }
    void Clear(const float *color0, const float *color1, const float *color2, const float *color3, const bool depth0) override;
    void SetState(piState state, bool value) override { (void)state; (void)value; Unsupported("SetState"); }
    void SetBlending(int buf, BlendEquation equRGB, BlendOperations srcRGB, BlendOperations dstRGB, BlendEquation equALP, BlendOperations srcALP, BlendOperations dstALP) override { (void)buf; (void)equRGB; (void)srcRGB; (void)dstRGB; (void)equALP; (void)srcALP; (void)dstALP; Unsupported("SetBlending"); }
    void SetViewport(int id, const int *vp) override;
    void SetViewports(int num, const float *viewports) override;
    void GetViewports(int *num, float *viewports) override;
    piRasterState CreateRasterState(bool wireframe, bool frontIsCounterClockWise, CullMode cullMode, bool depthClamp, bool multiSample) override { (void)wireframe; (void)frontIsCounterClockWise; (void)cullMode; (void)depthClamp; (void)multiSample; Unsupported("CreateRasterState"); }
    void SetRasterState(const piRasterState vme) override { (void)vme; Unsupported("SetRasterState"); }
    void DestroyRasterState(piRasterState vme) override { (void)vme; Unsupported("DestroyRasterState"); }
    piBlendState CreateBlendState(bool alphaToCoverage, bool enabled0) override { (void)alphaToCoverage; (void)enabled0; Unsupported("CreateBlendState"); }
    void SetBlendState(const piBlendState vme) override { (void)vme; Unsupported("SetBlendState"); }
    void DestroyBlendState(piBlendState vme) override { (void)vme; Unsupported("DestroyBlendState"); }
    piDepthState CreateDepthState(bool alphaToCoverage, bool lessEqual) override { (void)alphaToCoverage; (void)lessEqual; Unsupported("CreateDepthState"); }
    void SetDepthState(const piDepthState vme) override { (void)vme; Unsupported("SetDepthState"); }
    void DestroyDepthState(piDepthState vme) override { (void)vme; Unsupported("DestroyDepthState"); }
    piTexture CreateTexture(const wchar_t *key, const TextureInfo *info, bool compress, TextureFilter filter, TextureWrap wrap, float aniso, const void *buffer) override { (void)key; (void)info; (void)compress; (void)filter; (void)wrap; (void)aniso; (void)buffer; Unsupported("CreateTexture"); }
    piTexture CreateTexture2(const wchar_t *key, const TextureInfo *info, bool compress, TextureFilter filter, TextureWrap wrap1, float aniso, const void *buffer, int bindUsage) override { (void)key; (void)info; (void)compress; (void)filter; (void)wrap1; (void)aniso; (void)buffer; (void)bindUsage; Unsupported("CreateTexture2"); }
    void DestroyTexture(piTexture obj) override { (void)obj; Unsupported("DestroyTexture"); }
    void ClearTexture(piTexture vme, int level, const void *data) override { (void)vme; (void)level; (void)data; Unsupported("ClearTexture"); }
    void UpdateTexture(piTexture me, int x0, int y0, int z0, int xres, int yres, int zres, const void *buffer) override { (void)me; (void)x0; (void)y0; (void)z0; (void)xres; (void)yres; (void)zres; (void)buffer; Unsupported("UpdateTexture"); }
    void GetTextureRes(piTexture me, int *res) override { (void)me; (void)res; Unsupported("GetTextureRes"); }
    void GetTextureFormat(piTexture me, Format *format) override { (void)me; (void)format; Unsupported("GetTextureFormat"); }
    void GetTextureContent(piTexture me, void *data, const Format fmt) override { (void)me; (void)data; (void)fmt; Unsupported("GetTextureContent"); }
    void GetTextureContent(piTexture vme, void *data, int x, int y, int z, int xres, int yres, int zres) override { (void)vme; (void)data; (void)x; (void)y; (void)z; (void)xres; (void)yres; (void)zres; Unsupported("GetTextureContent"); }
    void GetTextureInfo(piTexture me, TextureInfo *info) override { (void)me; (void)info; Unsupported("GetTextureInfo"); }
    void GetTextureSampling(piTexture vme, TextureFilter *rfilter, TextureWrap *rwrap) override { (void)vme; (void)rfilter; (void)rwrap; Unsupported("GetTextureSampling"); }
    void ComputeMipmaps(piTexture me) override { (void)me; Unsupported("ComputeMipmaps"); }
    void AttachTextures(int num, piTexture vt0, piTexture vt1, piTexture vt2, piTexture vt3, piTexture vt4, piTexture vt5, piTexture vt6, piTexture vt7, piTexture vt8, piTexture vt9, piTexture vt10, piTexture vt11, piTexture vt12, piTexture vt13, piTexture vt14, piTexture vt15) override { (void)num; (void)vt0; (void)vt1; (void)vt2; (void)vt3; (void)vt4; (void)vt5; (void)vt6; (void)vt7; (void)vt8; (void)vt9; (void)vt10; (void)vt11; (void)vt12; (void)vt13; (void)vt14; (void)vt15; Unsupported("AttachTextures"); }
    void AttachTextures(int num, piTexture *vt, int offset) override { (void)num; (void)vt; (void)offset; Unsupported("AttachTextures"); }
    void DettachTextures() override { Unsupported("DettachTextures"); }
    piTexture CreateTextureFromID(unsigned int id, TextureFilter filter) override { (void)id; (void)filter; Unsupported("CreateTextureFromID"); }
    void MakeResident(piTexture vme) override { (void)vme; Unsupported("MakeResident"); }
    void MakeNonResident(piTexture vme) override { (void)vme; Unsupported("MakeNonResident"); }
    uint64_t GetTextureHandle(piTexture vme) override { (void)vme; Unsupported("GetTextureHandle"); }
    piSampler CreateSampler(TextureFilter filter, TextureWrap wrap, float anisotropy) override { (void)filter; (void)wrap; (void)anisotropy; Unsupported("CreateSampler"); }
    void DestroySampler(piSampler obj) override { (void)obj; Unsupported("DestroySampler"); }
    void AttachSamplers(int num, piSampler vt0, piSampler vt1, piSampler vt2, piSampler vt3, piSampler vt4, piSampler vt5, piSampler vt6, piSampler vt7) override { (void)num; (void)vt0; (void)vt1; (void)vt2; (void)vt3; (void)vt4; (void)vt5; (void)vt6; (void)vt7; Unsupported("AttachSamplers"); }
    void DettachSamplers() override { Unsupported("DettachSamplers"); }
    void AttachImage(int unit, piTexture texture, int level, bool layered, int layer, Format format) override { (void)unit; (void)texture; (void)level; (void)layered; (void)layer; (void)format; Unsupported("AttachImage"); }
    piShader CreateShader(const piShaderOptions *options, const char *vs, const char *cs, const char *es, const char *gs, const char *fs, char *error) override;
    piShader CreateShaderBinary(const piShaderOptions *options, const uint8_t *vs, const int vs_len, const uint8_t *cs, const int cs_len, const uint8_t *es, const int es_len, const uint8_t *gs, const int gs_len, const uint8_t *fs, const int fs_len, char *error) override;
    void DestroyShader(piShader obj) override;
    void AttachShader(piShader obj) override;
    void DettachShader() override;
    piShader CreateCompute(const piShaderOptions *options, const char *cs, char *error) override { (void)options; (void)cs; (void)error; Unsupported("CreateCompute"); }
    void SetShaderConstant4F(const unsigned int pos, const float *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant4F"); }
    void SetShaderConstant3F(const unsigned int pos, const float *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant3F"); }
    void SetShaderConstant2F(const unsigned int pos, const float *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant2F"); }
    void SetShaderConstant1F(const unsigned int pos, const float *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant1F"); }
    void SetShaderConstant1I(const unsigned int pos, const int *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant1I"); }
    void SetShaderConstant1UI(const unsigned int pos, const unsigned int *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant1UI"); }
    void SetShaderConstant2UI(const unsigned int pos, const unsigned int *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant2UI"); }
    void SetShaderConstant3UI(const unsigned int pos, const unsigned int *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant3UI"); }
    void SetShaderConstant4UI(const unsigned int pos, const unsigned int *value, int num) override { (void)pos; (void)value; (void)num; Unsupported("SetShaderConstant4UI"); }
    void SetShaderConstantMat4F(const unsigned int pos, const float *value, int num, bool transpose) override { (void)pos; (void)value; (void)num; (void)transpose; Unsupported("SetShaderConstantMat4F"); }
    void SetShaderConstantSampler(const unsigned int pos, int unit) override { (void)pos; (void)unit; Unsupported("SetShaderConstantSampler"); }
    void AttachShaderConstants(piBuffer obj, int unit) override;
    void AttachShaderBuffer(piBuffer obj, int unit) override;
    void DettachShaderBuffer(int unit) override;
    void AttachAtomicsBuffer(piBuffer obj, int unit) override { (void)obj; (void)unit; Unsupported("AttachAtomicsBuffer"); }
    void DettachAtomicsBuffer(int unit) override { (void)unit; Unsupported("DettachAtomicsBuffer"); }
    piBuffer CreateBuffer(const void *data, unsigned int amount, BufferType mode, BufferUse use) override;
    piBuffer CreateStructuredBuffer(const void *data, unsigned int numElements, unsigned int elementSize, BufferType mode, BufferUse use) override;
    piBuffer CreateBufferMapped_Start(void **ptr, unsigned int amount, BufferUse use) override;
    void CreateBufferMapped_End(piBuffer vme) override;
    void DestroyBuffer(piBuffer arg0) override;
    void UpdateBuffer(piBuffer obj, const void *data, int offset, int len, bool invalidate) override;
    void AttachPixelPackBuffer(piBuffer obj) override { (void)obj; Unsupported("AttachPixelPackBuffer"); }
    void DettachPixelPackBuffer() override { Unsupported("DettachPixelPackBuffer"); }
    piVertexArray CreateVertexArray(int numStreams, piBuffer vb0, const piRArrayLayout *streamLayout0, piBuffer vb1, const piRArrayLayout *streamLayout1, piBuffer eb, const IndexArrayFormat ebFormat) override { (void)numStreams; (void)vb0; (void)streamLayout0; (void)vb1; (void)streamLayout1; (void)eb; (void)ebFormat; Unsupported("CreateVertexArray"); }
    void DestroyVertexArray(piVertexArray obj) override;
    void AttachVertexArray(piVertexArray obj) override;
    void DettachVertexArray() override;
    piVertexArray CreateVertexArray2(int numStreams, piBuffer vb0, const ArrayLayout2 *streamLayout0, piBuffer vb1, const ArrayLayout2 *streamLayout1, const void *shaderBinary, size_t shaderBinarySize, piBuffer ib, const IndexArrayFormat ebFormat) override;
    void AttachVertexArray2(piVertexArray vme) override;
    void DestroyVertexArray2(piVertexArray vme) override;
    piQuery CreateQuery(piRenderer::QueryType type) override { (void)type; Unsupported("CreateQuery"); }
    void DestroyQuery(piQuery vme) override { (void)vme; Unsupported("DestroyQuery"); }
    void BeginQuery(piQuery vme) override { (void)vme; Unsupported("BeginQuery"); }
    void EndQuery(piQuery vme) override { (void)vme; Unsupported("EndQuery"); }
    uint64_t GetQueryResult(piQuery vme) override { (void)vme; Unsupported("GetQueryResult"); }
    void DrawPrimitiveIndexed(PrimitiveType pt, uint32_t num, uint32_t numInstances, uint32_t baseVertex, uint32_t baseInstance, uint32_t baseIndex) override;
    void DrawPrimitiveIndirect(PrimitiveType pt, piBuffer cmds, uint32_t offset, uint32_t num) override { (void)pt; (void)cmds; (void)offset; (void)num; Unsupported("DrawPrimitiveIndirect"); }
    void DrawPrimitiveNotIndexed(PrimitiveType pt, int first, int num, int numInstances) override;
    void DrawPrimitiveNotIndexedMultiple(PrimitiveType pt, const int *firsts, const int *counts, int num) override { (void)pt; (void)firsts; (void)counts; (void)num; Unsupported("DrawPrimitiveNotIndexedMultiple"); }
    void DrawPrimitiveNotIndexedIndirect(PrimitiveType pt, piBuffer cmds, int num) override { (void)pt; (void)cmds; (void)num; Unsupported("DrawPrimitiveNotIndexedIndirect"); }
    void DettachIndirectBuffer() override { Unsupported("DettachIndirectBuffer"); }
    void DrawUnitCube_XYZ_NOR(int numInstanced) override { (void)numInstanced; Unsupported("DrawUnitCube_XYZ_NOR"); }
    void DrawUnitCube_XYZ(int numInstanced) override { (void)numInstanced; Unsupported("DrawUnitCube_XYZ"); }
    void DrawUnitQuad_XY(int numInstanced) override { (void)numInstanced; Unsupported("DrawUnitQuad_XY"); }
    void ExecuteCompute(int ngx, int ngy, int ngz, int gsx, int gsy, int gsz) override { (void)ngx; (void)ngy; (void)ngz; (void)gsx; (void)gsy; (void)gsz; Unsupported("ExecuteCompute"); }
    void CreateSyncObject(piBuffer &buffer) override { (void)buffer; Unsupported("CreateSyncObject"); }
    bool CheckSyncObject(piBuffer &buffer) override { (void)buffer; Unsupported("CheckSyncObject"); }
    void SetPointSize(bool mode, float size) override { (void)mode; (void)size; Unsupported("SetPointSize"); }
    void SetLineWidth(float size) override { (void)size; Unsupported("SetLineWidth"); }
    void PolygonOffset(bool mode, bool wireframe, float a, float b) override { (void)mode; (void)wireframe; (void)a; (void)b; Unsupported("PolygonOffset"); }
    void RenderMemoryBarrier(BarrierType type) override { (void)type; Unsupported("RenderMemoryBarrier"); }
private:
    [[noreturn]] static void Unsupported(const char* operation)
    {
        throw std::logic_error(operation);
    }
    void PrepareDraw(PrimitiveType primitive);
    struct State;
    std::unique_ptr<State> m;
};
}
