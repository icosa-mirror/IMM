#pragma once

#include <chrono>
#include <mutex>

#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libRender/piRenderer.h"
#include "libImmCore/src/libSound/piSound.h"
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libBasics/piString.h"
#include "libImmCore/src/libBasics/piTick.h"
#include "libImmCore/src/libBasics/piTimer.h"
#include "libImmCore/src/libBasics/piMutex.h"
#include "libImmCore/src/libBasics/piPool.h"

#include "layerRenderers/layerRendererPaint/layerRendererPaint.h"

#include "document.h"
#include "libImmImporter/src/document/layer.h"

namespace ImmPlayer {

    class Player
    {
    public:
        Player();
        ~Player();

        struct Configuration
        {
            ImmImporter::Drawing::ColorSpace  colorSpace;
            int         multisamplingLevel;
            DepthBuffer depthBuffer;
            ClipSpaceDepth clipDepth;
            ClipSpaceDepth projectionMatrix;
            bool           frontIsCCW;
            ImmImporter::Drawing::PaintRenderingTechnique paintRenderingTechnique = Drawing::Static;
        };

        bool Init(ImmCore::piRenderer* renderer, ImmCore::piSoundEngine* sound, ImmCore::piLog* log, ImmCore::piTimer *timer, const Configuration * configuration);
        void Deinit(void);

        // call this only once per frame
        void GlobalWork( bool enabled, uint32_t microsecondsBudget);

        // call this ones per frame and per camera
        void GlobalRender(const ImmCore::trans3d & vr_to_head, const ImmCore::trans3d & world_to_head, const ImmCore::mat4x4 & projection, const StereoMode & stereoMode);

        //=== this are the only functions that can be called form the render thread. Everything else should always be called from the main thread ===
        // call this ones per frame and per camera, if in mono or two pass stereo
        void RenderMono(const ImmCore::ivec2 & pixelResolutionIncludingSupersampling, int eyeID);
        // call this ones per frame and per camera, if in single pass stereo
        void RenderStereoMultiPass(const ImmCore::ivec2 & pixelResolutionIncludingSupersampling, int eyeID, const ImmCore::mat4x4d & vr_to_eye, const ImmCore::mat4x4 & projection);
        // call this ones per frame and per camera, if in single pass stereo
        void RenderStereoSinglePass(const ImmCore::ivec2 & pixelResolutionIncludingSupersampling, const ImmCore::mat4x4d & vr_to_lEye, const ImmCore::mat4x4 & lProjection, const ImmCore::mat4x4d & vr_to_rEye, const ImmCore::mat4x4 & rProjection);
        //===============================


        struct PlayerInfo
        {
            struct
            {
                float mRed;
                float mGreen;
                float mBlue;
            }mBackgrundColor;
        };
        int  Load(const wchar_t* name);
        int  Load(const uint8_t* data, uint64_t size, const wchar_t* name);
        bool IsDocumentActive(int id) const;

        void Unload(int id);
        void UnloadAll();
        void UnloadAllSync();

        void SetDocumentToWorld(int id, const ImmCore::trans3d & m);

        int GetChapterCount(int docId);
        int GetCurrentChapter(int docId);

        int  GetSpawnAreaCount(int docId);
        int  GetSpawnArea(int docId);
        int  GetInitialSpawnArea(int docId);
        void SetSpawnArea(int docId, int spawnAreaId);
        bool GetSpawnAreaNeedsUpdate(int docId);
        void SetSpawnAreaNeedsUpdate(int docId, bool state);
        const ImmCore::piImage* GetSpawnAreaScreenshot(int docId, int spawnAreaId);
        bool GetSpawnAreaInfo(Document::SpawnAreaInfo & info, int docId, int spawnAreaId);

        // playback control
        void SetTime(int id, ImmCore::piTick timeSinceStart, ImmCore::piTick timeSinceStop);
        void GetTime(int id, ImmCore::piTick* timeSinceStart, ImmCore::piTick* timeSinceStop);

        void Pause(int id);        // pause
        void Pause(int id, uint64_t stopTicks); // pause at stop tick
        void Resume(int id);       // resume
        void Resume(int id, uint64_t startTicks);
        void Hide(int id);        // pause
        void Show(int id);       // resume
        void SkipForward(int id);       // jump to next chapter
        void SkipBack(int id);         // jump to prev chapter
        void SetChapter(int id, int chapterIndex); // jump to chapter index
        void Restart(int id);      // restart the whole comic
        void Continue(int id);       // continue from a stop

        bool GetHasAudio(int id);
        float GetDocumentVolume(int id) const ; // get master volume in [0.0f, 1.0f]
        void SetDocumentVolume(int id, float volume); // set master volume in [0.0f, 1.0f]

        ImmCore::bound3d GetDocumentBBox(int id) const; // get bounding box from root layer

        // layer edits (runtime)
        bool SetLayerVisible(int docId, int layerId, bool visible);
        bool ClearLayerVisibilityOverride(int docId, int layerId);
        bool SetLayerOpacity(int docId, int layerId, float opacity);
        bool SetLayerTransform(int docId, int layerId, const ImmCore::trans3d & transform);
        bool ClearLayerTransformOverride(int docId, int layerId);

        struct LayerDiagnostics
        {
            int hasVisibilityKeys = 0;
            int hasOpacityKeys = 0;
            int isVisible = 0;
            float opacity = 0.0f;
            int isWorldVisible = 0;
            float worldOpacity = 0.0f;
            int parentId = -1;
            int visibilityOverrideEnabled = 0;
            int visibilityOverrideValue = 0;
            int hasTransformKeys = 0;
            int transformOverrideEnabled = 0;
        };

        bool GetLayerDiagnostics(int docId, int layerId, LayerDiagnostics & outDiag) const;

        struct PerformanceInfo
        {
            PerformanceInfo & operator=(const PerformanceInfo& info)
            {
                paintRenderingStrategy.InitCopy(&info.paintRenderingStrategy);
                cpuLoadTimeMS = info.cpuLoadTimeMS;
                numDrawCalls = info.numDrawCalls;
                numDrawCallsCulled = info.numDrawCallsCulled;
                numPaintDrawCalls = info.numPaintDrawCalls;
                numPictureDrawCalls = info.numPictureDrawCalls;
                numPicture2DDrawCalls = info.numPicture2DDrawCalls;
                numPicture360DrawCalls = info.numPicture360DrawCalls;
                numPicture360EquirectDrawCalls = info.numPicture360EquirectDrawCalls;
                numPicture360CubemapDrawCalls = info.numPicture360CubemapDrawCalls;
                numModelDrawCalls = info.numModelDrawCalls;
                numTriangles = info.numTriangles;
                numTrianglesCulled = info.numTrianglesCulled;
                validationTimeFrame = info.validationTimeFrame;
                gpuTimeAverageMs = info.gpuTimeAverageMs;
                return *this;
            }

            ImmCore::piString paintRenderingStrategy;
            int cpuLoadTimeMS = 0;
            int numDrawCalls = 0;
            int numDrawCallsCulled = 0;
            int numPaintDrawCalls = 0;
            int numPictureDrawCalls = 0;
            int numPicture2DDrawCalls = 0;
            int numPicture360DrawCalls = 0;
            int numPicture360EquirectDrawCalls = 0;
            int numPicture360CubemapDrawCalls = 0;
            int numModelDrawCalls = 0;
            int numTriangles = 0;
            int numTrianglesCulled = 0;
            uint64_t validationTimeFrame = 0;
            float gpuTimeAverageMs = 0;

            float totalGPUTimeAcrossFrames = 0;
            int numFramesMeasured = 0;
        };

        // Stats for this frame populated after Render*() method.
        const PerformanceInfo & GetPerformanceInfoForFrame() { return mCurrentPerfInfo; }
        int GetLoadTimeInMs();

        enum class LoadingState : int
        {
            Unloaded = 0,
            Loading = 1,
            Loaded = 2,
            Unloading = 3,
            Failed = 4
        };

        enum class PlaybackState : int
        {
            Playing = 0,		 // still, animation, comic:   music is on , animations are on  (if any),     rendering
            Paused = 1,			 // still, animation, comic:   music is off, animations are off (if any),     rendering
            PausedAndHidden = 2, // still, animation, comic:   music is off, animations are off (if any), NOT rendering
            Waiting = 3,		 //                   comic:   music is on,  animations are on          ,     rendering
            Finished = 4		 //                   comic:   ....
        };

        struct DocumentState
        {
            LoadingState  mLoadingState = LoadingState::Unloaded;
            PlaybackState mPlaybackState = PlaybackState::Playing;
        };

        struct LayerInfo
        {
            int id = -1;
            int type = 0;
            int parentId = -1;
            int isTimeline = 0;
            int isLoaded = 0;
            int isVisible = 0;
            float opacity = 0.0f;
            int hasBBox = 0;
            ImmCore::bound3 bbox;
            int numChildren = 0;
            int assetId = -1;
            int paintNumDrawings = 0;
            int paintNumFrames = 0;
            int paintNumStrokes = 0;
            wchar_t name[128];
            wchar_t fullName[256];
        };

        enum class DocumentType : int
        {
            Still = 0,
            Animated = 1,
            Comic = 2
        };

        // TODO: this doesn't scale...
        enum class DocumentInfoFlags : uint32_t
        {
            MOVABLE = 1 << 0,
            DISPLAYABLE = 1 << 1,
            PLAYABLE = 1 << 2,
            NEXTABLE = 1 << 3,
            PREVABLE = 1 << 4,
            TIMEABLE = 1 << 5,
            SOUNDABLE = 1 << 6,
            BOUNDABLE = 1 << 7,
            GRABBABLE = 1 << 8,
            VIEWABLE = 1 << 9,
        };

        void GetDocumentState(DocumentState & state, int id) const;
        uint32_t GetDocumentInfoEx(int id) const;
        void GetPlayerInfo(PlayerInfo & info) const;
        void CancelLoading(int id);
        int GetLayerCount(int docId) const;
        bool GetLayerInfoByIndex(int docId, int index, LayerInfo & info) const;
        bool IsSequenceReady(int docId) const;

        void EnablePerformanceMeasurement(bool enabled) { mEnablePerformanceMeasurement = enabled; }
		void GetChapterInfo(size_t& numChapters, ImmCore::piTArray<ImmCore::piTick>& chapterLengths, bool& hasPlays, int id);

    private:
        void iGlobalWorkLayer(Layer* la, float masterVolum);
        void iDisplayPreRenderLayer(Layer* la, const ImmCore::trans3d & parentLocation, float parentOpacity, const ImmCore::trans3d & worldToViewer);
        void iUnloadNotInTimeline(Layer* root, ImmCore::piTick now);
        ImmCore::mat4x4 iConvertProjectionMatrix(const ImmCore::mat4x4 & mat);


        void PopulateDisplayRenderPerfInfo();

        //-----------------------------------------------------------------------------------------------------
        // DATA
        //-----------------------------------------------------------------------------------------------------

        typedef struct
        {
            float mTime;
            int mFrameID;
            int mDummy1;
            int mDummy2;
        } FrameState;	// slot 0

        struct
        {
            ImmCore::mat4x4  mProjection;
            ImmCore::trans3d mWorldToHead;
            ImmCore::trans3d mHeadToWorld;
            ImmCore::trans3d mVRToHead;

            ImmCore::vec3d   mCamPos; // in world
            ImmCore::vec3d   mCamDir; // in world
        }mViewerInfo;

        typedef struct
        {
            struct
            {
                //mat4x4 mMatrix_Prj;
                //mat4x4 mHeadToEye; // in viewer space (viewer to eye)
                //mat4x4 mMatrix_CamPrj;
                //mat4x4 mInvMatrix_Prj;
                //mat4x4 mEyeToHead;
                //mat4x4 mInvMatrix_CamPrj;
                ImmCore::mat4x4 mViewerToEye_Prj;
            } mEye[2];
            ImmCore::vec2 mResolution;
            uint32_t mEyeIndex;
        } DisplayRenderState; // slot 4


                              // totally useless except for the Fallback StereoMode :(
        typedef struct
        {
            uint32_t mID;
            uint32_t dummy1;
            uint32_t dummy2;
            uint32_t dummy3;
        }PassState;		// slot 5

        typedef struct
        {
            uint64_t mBlueNoiseTexture;
        }GlobalResourcesState;

        ImmCore::piRenderer* mRenderer;
        ImmCore::piSoundEngine* mSoundEngine;
        ImmCore::piLog* mLog;
        ImmCore::piTimer *mTimer;
        uint32_t mFrame;
        uint64_t mValidationTimeFrame;
        bool     mAnyDocToRender;

        FrameState mFrameState;
        DisplayRenderState mDisplayRenderState;
        PassState    mPassState;

        ImmCore::piBuffer mFrameStateShaderConstans;
        ImmCore::piBuffer mDisplayStateShaderConstans;
        ImmCore::piBuffer mLayerStateShaderConstans;
        ImmCore::piBuffer mPassStateShaderConstans;
        ImmCore::piBuffer mGlobalResourcesConstans;
        ImmCore::piTexture mBlueNoise;

        ImmCore::piRasterState mRasterState = nullptr;
        ImmCore::piBlendState  mBlendState = nullptr;
        ImmCore::piDepthState  mDepthState = nullptr;
        ImmCore::piDepthState  mBackgroundDepthState = nullptr;

        // state
        int        mSeed;
        Drawing::ColorSpace mColorSpace;
        DepthBuffer mDetphBufferMode;
        ClipSpaceDepth mClipDepthMode;
        ClipSpaceDepth mProjectionMatricesMode;
        ImmCore::piPool mDocuments;
        ImmCore::piTArray<bool> mSynced;

        struct Command
        {
            Document::Command mCommand;
            int mTarget;
        };
        ImmCore::piTArray<Command> mCommandList;
        //=========================

        LayerRendererPaint * mLayerPaintRender = nullptr;
        LayerRendererPicture mLayerRenderPicture;
        LayerRendererSound mLayerRenderSound;
        LayerRendererModel mLayerRenderModel;

        bool mEnabled;
        std::mutex   mMutex; // to synch the main thread and the render thread. TODO: remove it - use double buffered rendering
        ImmCore::piTick     mTime;

        #if defined(RENDER_BUDGET) || defined(MEASURE_GPU_TIME)
        uint32_t mMicrosecondsLastFrame;
        #endif

        #ifdef RENDER_BUDGET
        uint32_t mMicrosecondsBudget;
        int     mDeltaCapTrigger;
        #endif
        int     mDeltaCap;

        PerformanceInfo mCurrentPerfInfo;
        std::chrono::system_clock::time_point mCPULoadStartTimeMS;

        bool mEnablePerformanceMeasurement = false;

        ImmImporter::Drawing::PaintRenderingTechnique mPaintRenderingTechnique;
    };

}
