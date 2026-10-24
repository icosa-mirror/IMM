#pragma once

#include <functional>
#include <vector>

#include "libImmCore/src/libBasics/piArray.h"
#include "libImmCore/src/libBasics/piString.h"
#include "libImmCore/src/libBasics/piTArray.h"
#include "libImmCore/src/libBasics/piLog.h"
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "libImmCore/src/libBasics/piTick.h"

#include "behaviors/keepalive.h"

namespace ImmImporter
{
    enum class AssetFormat : uint32_t
    {
        NONE = 0,
        PNG = 1,
        JPG = 2,
        WAV = 3,
        OGG = 4,
        OPUS = 5
    };


	typedef void* LayerImplementation;

	class Sequence;

	class Layer
	{
	public:
		Layer(Sequence* sq, Layer* parent, uint32_t id);
		~Layer();

		enum class Type  : uint32_t
		{
			Group,     // a group
			Paint,     // a paint layer containing brush strokes
			Effect,    // an effect layer (a blur, a color correct, etc). This is defined in a volume
			Model,     // a 3d model imported form FBX, OBJ, etc
			Picture,   // a flat picture
			Sound,     // 3D (or plain) sound source
			Reference, // a reference to an external file
			Instance ,  // an instance to an internal layer
			SpawnArea , // a view point (location + locomotion volume and other viewing properties)
			MAX
		} ;


		enum class InterpolationType : uint32_t
		{
			None,
			Linear,
			Smoothstep,
            EaseIn,
            EaseOut,
            Spline,
            Auto,
            MAX
		};


		enum class AnimAction : uint32_t
		{
			Stop,
			Play,
            Loop,
            MakeDefault,
			MAX
		};


		enum class AnimProperty : uint32_t
		{
			Visibility,				// bool
			Opacity,				// float
			Position,
			Rotation,
			Scale,
			DrawInTime,				// double
			Action,					// uint
            Loop,                   // bool
            Offset,                 // uint
            Transform,
			MAX
		};

		struct AnimValue
		{
			bool mBool;
			uint32_t mInt;
			float mFloat;
			double mDouble;
			ImmCore::trans3d mTransform;
			void init(void) { mBool=false; mInt=0; mFloat=0.0f; mDouble=0.0; mTransform = ImmCore::trans3d::identity(); }
		} ;

		typedef struct
		{
            ImmCore::piTick mTime;
			AnimValue mValue;
			InterpolationType mInterpolation;
		} AnimKey;

        // Explicit evaluation boundary for an independent container clock.
        // The owner supplies a live timeline Group, nonnegative clock values,
        // and its canonical duration/repeat. It also owns runtime save/restore;
        // this context does not clone the graph or restore evaluated state.
        // While advancing, start+elapsed must equal the evaluation timestamp;
        // a held clock retains that sum at its hold. Boundary world properties
        // are identity/visible/opacity1. Audio is disabled unless explicitly owned.
        struct EvaluationContext
        {
            Layer * mBoundary = nullptr;
            ImmCore::piTick mBoundaryStartTime = ImmCore::piTick(0);
            ImmCore::piTick mBoundaryElapsed = ImmCore::piTick(0);
            ImmCore::piTick mBoundaryDuration = ImmCore::piTick(0);
            uint32_t mBoundaryRepeatCount = 1;
            bool mPlaying = true; // Logical evaluation intent; may also be held.
            bool mHeld = false;
            bool mActionHeld = false;
            bool mActionsEnabled = true;
            bool mAudioEnabled = false; // Enabling requires independently owned/restorable voices.
            struct ConsumedAction
            {
                uint32_t layerId;
                ImmCore::piTick time;
            };
            std::vector<ConsumedAction> mConsumedActions;
            struct DefaultActionExecution
            {
                uint32_t layerId;
                ImmCore::piTick time;
                bool active;
            };
            // Inactive executed identities retain their order so restoring a
            // removed MakeDefault key can restore its effect without replay.
            std::vector<DefaultActionExecution> mDefaultActionHistory;
            uint32_t mDefaultSpawnAreaLayerId = UINT32_MAX;

            bool Contains(const Layer * layer) const;
            bool CanConsumeAction(const Layer * layer, ImmCore::piTick time) const noexcept;
            bool ConsumeAction(const Layer * layer, ImmCore::piTick time);
            bool PrepareDefaultActionCapacity(size_t additionalIdentities);
            bool RecordDefaultActionExecution(const Layer * layer, ImmCore::piTick time) noexcept;
            void ReconcileDefaultActionKeys(const Layer * layer, const std::vector<AnimKey> & keys) noexcept;
            void ResetDefaultActionHistory() noexcept;
        };

        // Evaluated/runtime state only: no keys, canonical properties, hierarchy,
        // assets or geometry ownership. Overrides and potential visibility are
        // included for exact restoration even though context evaluation does not
        // change them. The owner must exclude concurrent edits to these fields,
        // or explicitly reconcile them before restoring the capsule.
        struct PlaybackState
        {
            bool mVisible = true;
            bool mPotentiallyVisible = true;
            float mOpacity = 1.0f;
            ImmCore::trans3d mTransform = ImmCore::trans3d::identity();
            double mDrawInTime = 1.0;
            double mAnimParams[4] = {0.0, 0.7, 0.3, 0.7};
            bool mVisibilityOverrideEnabled = false;
            bool mVisibilityOverrideValue = true;
            bool mOpacityOverrideEnabled = false;
            float mOpacityOverrideValue = 1.0f;
            bool mTransformOverrideEnabled = false;
            ImmCore::trans3d mTransformOverrideValue = ImmCore::trans3d::identity();
            uint32_t mEvaluatedMaxRepeatCount = 1;
            bool mIsPlaying = false;
            bool mHasStopTime = false;
            bool mStoppedByAction = false;
            ImmCore::piTick mStartTime = ImmCore::piTick(0);
            ImmCore::piTick mStopTime = ImmCore::piTick(0);
            ImmCore::piTick mLastActiveSpanStart = ImmCore::piTick(0);
            ImmCore::piTick mLastActiveSpanClockOrigin = ImmCore::piTick(0);
            bool mHasActiveSpanClockOrigin = false;
            bool mPlaybackControlChanged = false;
            ImmCore::piTick mStartOffset = ImmCore::piTick(0);
        };
        PlaybackState CapturePlaybackState() const;
        // Shift must fit absolute piTick values. Local span start/offset do not shift.
        void RestorePlaybackState(const PlaybackState & state, ImmCore::piTick wallClockShift = ImmCore::piTick(0));
        // Preserve canonical state, source action flags, overrides and streaming
        // eligibility; reset only evaluated pose and playback clocks/span caches.
        void ResetPlaybackStateForEvaluation();



		bool Init(Type type, const wchar_t* name, bool visible, const ImmCore::trans3d & transform, const ImmCore::trans3d& pivot, float opacity, bool isTimeLine, ImmCore::piTick duration, uint32_t maxRepeatCount, uint32_t assetID, ImmCore::piLog *log, bool publishToParent = true);
		void Deinit(ImmCore::piLog *log);

		uint32_t GetID(void) const;

		LayerImplementation GetImplementation(void) const; // if not Group
		void SetImplementation(LayerImplementation implementation);

		const   Type GetType(void) const;
		const   ImmCore::piString& GetName(void) const;
		const   ImmCore::piString* GetFullName(void) const;
		bool    GetVisible(void) const;
		bool    GetCanonicalVisible(void) const;
		bool    GetPotentiallyVisible(void) const;
        bool    GetWorldVisible(const EvaluationContext * context = nullptr) const;
        bool    GetVisibilityOverrideEnabled(void) const;
        bool    GetVisibilityOverrideValue(void) const;
        bool    GetTransformOverrideEnabled(void) const;
        const ImmCore::trans3d &GetTransformOverrideValue(void) const;
		float   GetOpacity(void) const;
		float   GetCanonicalOpacity(void) const;
		bool    GetOpacityOverrideEnabled(void) const;
		float   GetOpacityOverrideValue(void) const;
        float   GetWorldOpacity(const EvaluationContext * context = nullptr) const;

		ImmCore::trans3d GetTransform(void) const;
		ImmCore::trans3d GetCanonicalTransform(void) const;
		ImmCore::trans3d GetPivot(void) const;
		bool    HasBBox(void) const;
		const ImmCore::bound3d GetBBox(void);
		bool* GetVisibleRef(void);
		float* GetOpacityRef(void);
		double GetDrawInTime(void) const;
		double GetAnimParam(int id) const;
		bool GetLayerUsesDrawin(void) const;
		bool GetPlaying(void) const;
		bool GetIsTimeline(void) const;
		Layer* GetTimeline(const EvaluationContext * context = nullptr) const;
		Layer* GetParent() const;                          // a group!
		ImmCore::trans3d GetTransformToWorld(const EvaluationContext * context = nullptr) const;
		KeepAlive * GetKeepAlive(void);
		uint32_t GetMaxRepeatCount(void) const;
        uint32_t GetEvaluatedMaxRepeatCount(void) const;
		ImmCore::piTick GetStopTime() const;
        bool HasStopTime() const;
        bool IsStoppedByAction() const;
        ImmCore::piTick GetTimelineElapsed(ImmCore::piTick now, const EvaluationContext * context = nullptr) const;
		ImmCore::piTick GetDuration() const;
        // Evaluated Offset key value, in the parent timeline's tick domain.
        ImmCore::piTick GetStartOffset() const { return mStartOffset; }
        bool HasEvaluatedVisibilitySpan() const { return mHasActiveSpanClockOrigin; }
		ImmCore::piTick GetStartTime() const;
		uint32_t GetNumChildren(void) const;                    // if group
		Layer* GetChild(uint32_t id);                           // if group
		bool PrepareChildPublication(void);
		bool PublishPreparedChild(Layer * child);
		bool InsertPreparedChild(Layer * child, uint32_t childIndex);
		bool RemovePublishedChild(Layer * child);
		bool ReorderPublishedChild(Layer * child, uint32_t childIndex);
		void SetPublishedParent(Layer * parent);
		void PublishPreparedFullName(ImmCore::piString * fullName);
        void PublishPreparedName(ImmCore::piString * name);
		uint32_t GetNumAnimKeys(AnimProperty property) const;
		const AnimKey* GetAnimKey(AnimProperty property, unsigned int index) const;
        bool GetLoaded(void) const;
		const uint32_t GetAssetID(void) const;

		// hm, do we really need these?
		void SetOpacity(float opacity);
		void SetCanonicalOpacity(float opacity);
		void SetOpacityOverride(bool enabled, float opacity);
		void SetVisible(bool visible);
		void SetCanonicalVisible(bool visible);
        void SetVisibilityOverride(bool enabled, bool visible);
		void SetPotentiallyVisible(bool visible);
		void SetStartTime(ImmCore::piTick time);
		void SetStopTime(ImmCore::piTick time, bool hasStopTime = true, bool stoppedByAction = false);
		void SetDrawInTime(double v);
		void SetTransform(const ImmCore::trans3d & t);
		void SetCanonicalTransform(const ImmCore::trans3d & t);
        void SetTransformOverride(bool enabled, const ImmCore::trans3d & t);
		void SetPlaying(bool playing);
		void    SetPosition(const ImmCore::vec3d& pos);
		void    SetRotation(const ImmCore::quatd& rot);
		void    SetScale(double scale);
        void SetMaxRepeatCount(uint32_t count);
        void SetIsTimeline(bool timeline);
        void SetDuration(ImmCore::piTick duration);
        void SetPivot(const ImmCore::trans3d& p);
		void SetLoaded(bool loaded);

        void GetLocalTimeOffsetAndVisiblity(ImmCore::piTick rootTime, ImmCore::piTick * offset, bool * visible);

        void SetStateAt(ImmCore::piTick time, ImmCore::piLog* log, EvaluationContext * context = nullptr);
        void ReevaluatePaintPlaybackAt(ImmCore::piTick time, ImmCore::piLog * log, EvaluationContext * context = nullptr);
        void ReevaluatePlaybackAt(ImmCore::piTick time, ImmCore::piLog * log, bool transportPaused,
            EvaluationContext * context = nullptr);
        void ReevaluateTransformAt(ImmCore::piTick time, ImmCore::piLog * log, EvaluationContext * context = nullptr);
        void ReevaluateOpacityAt(ImmCore::piTick time, ImmCore::piLog * log, EvaluationContext * context = nullptr);
        void ReevaluateDrawInTimeAt(ImmCore::piTick time, ImmCore::piLog * log, EvaluationContext * context = nullptr);
        void ReevaluateVisibilityAt(ImmCore::piTick time, ImmCore::piLog * log, EvaluationContext * context = nullptr);

        // sets default pass through visiblity of groups without animation keys, hides all layers that have keys
        void SetDefaultPlaybackVisibility();

        // sets all actions before time as fired, and after as not fired
        void ResetActionStatesAt(ImmCore::piTick time);

        bool AddKey(ImmCore::piTick time, AnimProperty property, const AnimValue& value, InterpolationType interpolation);
        const AnimKey* GetAnimKeyAt(AnimProperty property, ImmCore::piTick time) const;
        static bool SetAnimKey(std::vector<AnimKey> & keys, ImmCore::piTick time,
            const AnimValue & value, InterpolationType interpolation);
        static bool RemoveAnimKey(std::vector<AnimKey> & keys, ImmCore::piTick time);
        bool CopyAnimKeys(AnimProperty property, std::vector<AnimKey> & keysOut) const;
        // Call at Action publication under the playback lock. Prepared flags may
        // predate live action consumption; matching time/kind keeps live state.
        void ReconcileActionPlayback(std::vector<AnimKey> & prepared, ImmCore::piTick localTime) const noexcept;
        void ReplaceAnimKeys(AnimProperty property, std::vector<AnimKey> && keys);

		typedef std::function<bool(Layer* layer, int level, int child, bool instance)> VisitorF;

		bool Recurse(VisitorF v, int level, unsigned int child, bool instance, bool doNotRecurseCollapsedGroups, bool doNotRecurseHiddenGroups, bool doNotRecurseLockedGroups, bool doPostExec);

	private:
		bool iCalcFullName(void);
        bool CanEvaluateInContext(const EvaluationContext * context) const;


	private:
		uint32_t mID;
        bool mInitialized = false;
		Type mType;
		bool mVisible;
		bool mCanonicalVisible;
        bool mVisibilityOverrideEnabled;
        bool mVisibilityOverrideValue;
        bool mTransformOverrideEnabled;
        ImmCore::trans3d mTransformOverrideValue;
        bool mLoaded;
		bool mPotentiallyVisible;
		ImmCore::piString mName;
		ImmCore::piString mFullName;
		float mOpacity;
		float mCanonicalOpacity;
		bool mOpacityOverrideEnabled;
		float mOpacityOverrideValue;
		ImmCore::trans3d mTransform;
		ImmCore::trans3d mCanonicalTransform;
        ImmCore::trans3d mPivotTransform;

		double mDrawInTime;
        double mCanonicalDrawInTime;
		double mAnimParams[4];
        void SetStateAtImpl(ImmCore::piTick time, ImmCore::piLog * log, bool reevaluatePaint,
            AnimProperty onlyProperty = AnimProperty::MAX, EvaluationContext * context = nullptr,
            bool refreshPlayback = false, bool transportPaused = false);
        ImmCore::piTick mLastActiveSpanStart; // keep track of gapless retriggering
        ImmCore::piTick mLastActiveSpanClockOrigin;
        bool mHasActiveSpanClockOrigin;
        bool mPlaybackControlChanged;
        ImmCore::piTick mStartOffset;
		ImmCore::piTick mDuration;
		ImmCore::piTick mStartTime;
		ImmCore::piTick mStopTime;
        bool mHasStopTime;
        bool mStoppedByAction;
		bool mIsPlaying;
		bool mIsTimeline;
		uint32_t mMaxRepeatCount;
        uint32_t mEvaluatedMaxRepeatCount;
		std::vector<AnimKey> mAnimKeys[static_cast<int>(AnimProperty::MAX)];

		LayerImplementation mImplementation; // if not group
		ImmCore::piTArray<Layer*> mChildren;          // if group
		Layer* mParent;                      // can only be of type group
		Sequence* mSequence; // TODO: delete!

		KeepAlive     mKeepAlive;           // subtle/minimal procedural effects on top of animated or static content

		// for streaming
		uint32_t mAssetID;

	};
}
