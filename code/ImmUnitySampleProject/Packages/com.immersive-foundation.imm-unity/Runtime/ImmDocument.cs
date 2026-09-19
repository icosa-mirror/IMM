using System;
using UnityEngine;

namespace ImmPlayer
{
    /// <summary>
    /// Represents a loaded IMM document
    /// Provides playback control and state management
    /// </summary>
    public class ImmDocument
    {
        #region Types

        [Flags]
        public enum DocumentInfoFlags : uint
        {
            Movable = 1 << 0,
            Displayable = 1 << 1,
            Playable = 1 << 2,
            Nextable = 1 << 3,
            Prevable = 1 << 4,
            Timeable = 1 << 5,
            Soundable = 1 << 6,
            Boundable = 1 << 7,
            Grabbable = 1 << 8,
            Viewable = 1 << 9
        }

        public enum PlaybackState
        {
            Playing = 0,
            Paused = 1,
            PausedAndHidden = 2,
            Waiting = 3,
            Finished = 4
        }

        [Serializable]
        public struct DocumentStateInfo
        {
            public LoadingState Loading;
            public PlaybackState Playback;
        }

        public enum LayerType
        {
            Group = 0,
            Paint = 1,
            Effect = 2,
            Model = 3,
            Picture = 4,
            Sound = 5,
            Reference = 6,
            Instance = 7,
            SpawnArea = 8
        }

        [Serializable]
        public struct LayerInfo
        {
            public int Id;
            public LayerType Type;
            public int ParentId;
            public bool IsTimeline;
            public bool IsLoaded;
            public bool IsVisible;
            public float Opacity;
            public bool HasBounds;
            public Bounds Bounds;
            public int NumChildren;
            public int AssetId;
            public int PaintNumDrawings;
            public int PaintNumFrames;
            public int PaintNumStrokes;
            public string Name;
            public string FullName;
        }

        public struct LayerDiagnostics
        {
            public bool HasVisibilityKeys;
            public bool HasOpacityKeys;
            public bool IsVisible;
            public float Opacity;
            public bool IsWorldVisible;
            public float WorldOpacity;
            public int ParentId;
            public bool VisibilityOverrideEnabled;
            public bool VisibilityOverrideValue;
            public bool HasTransformKeys;
            public bool TransformOverrideEnabled;
        }

        [Serializable]
        public struct SpawnAreaInfo
        {
            public int Id;
            public string Name;
            public int Version;
            public SerializedSpawnArea.Type Type;
            public bool Animated;
            public SerializedSpawnArea.Volume Volume;
            public SerializedSpawnArea.Transform Transform;
            public int Locomotion;
        }

        #endregion

        #region Properties

        public int DocumentId { get; private set; }
        public string FileName { get; private set; }
        public bool IsLoaded { get; private set; }

        #endregion

        #region Constructor

        public ImmDocument(int documentId, string fileName)
        {
            DocumentId = documentId;
            FileName = fileName;
            IsLoaded = true;
        }

        #endregion

        #region Playback Control

        /// <summary>
        /// Pause playback
        /// </summary>
        public void Pause()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.Pause(DocumentId);
        }

        /// <summary>
        /// Resume playback
        /// </summary>
        public void Resume()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.Resume(DocumentId);
        }

        /// <summary>
        /// Hide the document
        /// </summary>
        public void Hide()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.Hide(DocumentId);
        }

        /// <summary>
        /// Show the document
        /// </summary>
        public void Show()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.Show(DocumentId);
        }

        /// <summary>
        /// Continue playback
        /// </summary>
        public void Continue()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.Continue(DocumentId);
        }

        /// <summary>
        /// Skip forward to next chapter
        /// </summary>
        public void SkipForward()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.SkipForward(DocumentId);
        }

        /// <summary>
        /// Skip back to previous chapter
        /// </summary>
        public void SkipBack()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.SkipBack(DocumentId);
        }

        /// <summary>
        /// Jump to a specific chapter index
        /// </summary>
        public void SetChapter(int chapterIndex)
        {
            if (!IsLoaded) return;
            if (chapterIndex < 0) return;
            ImmNativePlugin.SetChapter(DocumentId, chapterIndex);
        }

        /// <summary>
        /// Restart the document from the beginning
        /// </summary>
        public void Restart()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.Restart(DocumentId);
        }

        #endregion

        #region Chapter Navigation

        /// <summary>
        /// Get the total number of chapters
        /// </summary>
        public int GetChapterCount()
        {
            if (!IsLoaded) return 0;
            return ImmNativePlugin.GetChapterCount(DocumentId);
        }

        /// <summary>
        /// Get the current chapter index
        /// </summary>
        public int GetCurrentChapter()
        {
            if (!IsLoaded) return 0;
            return ImmNativePlugin.GetCurrentChapter(DocumentId);
        }

        /// <summary>
        /// Chapter count, per-chapter lengths and whether chapters are defined by real Play markers.
        /// </summary>
        public struct ChapterInfo
        {
            public int Count;
            /// <summary>Length of each chapter in IMM ticks (same unit as <see cref="GetPlayTime"/>).</summary>
            public long[] Lengths;
            /// <summary>True when chapters come from Play markers, false when from Stop markers.</summary>
            public bool HasPlays;
        }

        /// <summary>
        /// Read the full chapter layout. Unlike the bare count this distinguishes real
        /// Play-marker chapters from Stop-marker ones and exposes each chapter's duration.
        /// </summary>
        public bool TryGetChapterInfo(out ChapterInfo chapterInfo)
        {
            chapterInfo = default;
            if (!IsLoaded)
                return false;

            int count = ImmNativePlugin.GetChapterCount(DocumentId);
            if (count <= 0)
                return false;

            long[] lengths = new long[count];
            int resolved = ImmNativePlugin.GetChapterInfoEx(DocumentId, lengths, lengths.Length, out int hasPlays);
            if (resolved > 0 && resolved < lengths.Length)
                Array.Resize(ref lengths, resolved);

            chapterInfo = new ChapterInfo
            {
                Count = resolved > 0 ? resolved : count,
                Lengths = lengths,
                HasPlays = hasPlays != 0
            };
            return true;
        }

        #endregion

        #region Time Control

        /// <summary>
        /// Set the playback time
        /// </summary>
        public void SetTime(long timeSinceStart, long timeSinceStop)
        {
            if (!IsLoaded) return;
            ImmNativePlugin.SetTime(DocumentId, timeSinceStart, timeSinceStop);
        }

        /// <summary>
        /// Get the playback time
        /// </summary>
        public void GetTime(out long timeSinceStart, out long timeSinceStop)
        {
            timeSinceStart = 0;
            timeSinceStop = 0;
            if (!IsLoaded) return;
            ImmNativePlugin.GetTime(DocumentId, out timeSinceStart, out timeSinceStop);
        }

        /// <summary>
        /// Get the current play time
        /// </summary>
        public long GetPlayTime()
        {
            if (!IsLoaded) return 0;
            return ImmNativePlugin.GetPlayTime(DocumentId);
        }

        #endregion

        #region Document State

        /// <summary>
        /// Get the current document state
        /// </summary>
        public DocumentState GetState()
        {
            DocumentState state = default;
            if (!IsLoaded) return state;
            ImmNativePlugin.GetDocumentState(out state, DocumentId);
            return state;
        }

        public enum LoadingState
        {
            Unloaded = 0,
            Loading = 1,
            Loaded = 2,
            Unloading = 3,
            Failed = 4
        }

        /// <summary>
        /// Get extended document info flags
        /// </summary>
        public uint GetDocumentInfo()
        {
            if (!IsLoaded) return 0;
            return ImmNativePlugin.GetDocumentInfoEx(DocumentId);
        }

        public DocumentStateInfo GetStateInfo()
        {
            var state = GetState();
            return new DocumentStateInfo
            {
                Loading = (LoadingState)state.loadingState,
                Playback = (PlaybackState)state.playbackState
            };
        }

        public bool IsPlaying()
        {
            return GetStateInfo().Playback == PlaybackState.Playing;
        }

        public DocumentInfoFlags GetInfoFlags()
        {
            return (DocumentInfoFlags)GetDocumentInfo();
        }

        public bool IsSequenceReady()
        {
            return IsLoaded && ImmNativePlugin.IsSequenceReady(DocumentId);
        }

        #endregion

        #region Audio

        /// <summary>
        /// Get the document volume (0.0 to 1.0)
        /// </summary>
        public float GetVolume()
        {
            if (!IsLoaded) return 0f;
            return ImmNativePlugin.GetSound(DocumentId);
        }

        /// <summary>
        /// Set the document volume (0.0 to 1.0)
        /// </summary>
        public void SetVolume(float volume)
        {
            if (!IsLoaded) return;
            ImmNativePlugin.SetSound(DocumentId, Mathf.Clamp01(volume));
        }

        /// <summary>
        /// Whether the document carries any audio at all. Volume alone cannot tell a
        /// silent document from a muted one.
        /// </summary>
        public bool HasAudio()
        {
            if (!IsLoaded) return false;
            return ImmNativePlugin.GetDocumentHasAudio(DocumentId);
        }

        /// <summary>
        /// Abandon an in-flight load. The document stays registered but loading stops.
        /// </summary>
        public void CancelLoading()
        {
            if (!IsLoaded) return;
            ImmNativePlugin.CancelDocumentLoad(DocumentId);
        }

        /// <summary>
        /// Pause when playback reaches <paramref name="stopTicks"/> instead of pausing immediately.
        /// </summary>
        public void PauseAt(long stopTicks)
        {
            if (!IsLoaded) return;
            ImmNativePlugin.PauseAt(DocumentId, stopTicks);
        }

        /// <summary>
        /// Resume when playback reaches <paramref name="startTicks"/> instead of resuming immediately.
        /// </summary>
        public void ResumeAt(long startTicks)
        {
            if (!IsLoaded) return;
            ImmNativePlugin.ResumeAt(DocumentId, startTicks);
        }

        #endregion

        #region Transform

        /// <summary>
        /// Set the document-to-world transformation matrix
        /// </summary>
        public void SetTransform(Matrix4x4 documentToWorld)
        {
            if (!IsLoaded) return;

            float[] matrix = MatrixToFloatArray(documentToWorld);
            ImmNativePlugin.SetDocumentToWorld(DocumentId, matrix);
        }

        /// <summary>
        /// Set the document transform using Unity Transform
        /// </summary>
        public void SetTransform(Transform transform)
        {
            if (transform == null) return;
            SetTransform(transform.localToWorldMatrix);
        }

        #endregion

        #region Bounding Box

        /// <summary>
        /// Get the document's bounding box
        /// </summary>
        public Bounds GetBoundingBox()
        {
            if (!IsLoaded) return new Bounds();

            Bounds3 bounds3;
            ImmNativePlugin.GetBoundingBox(DocumentId, out bounds3);
            return bounds3.ToUnityBounds();
        }

        #endregion

        #region Spawn Areas

        /// <summary>
        /// Get the number of spawn areas in the document
        /// </summary>
        public int GetSpawnAreaCount()
        {
            if (!IsLoaded) return 0;
            return ImmNativePlugin.GetSpawnAreaCount(DocumentId);
        }

        /// <summary>
        /// Get the list of spawn area IDs
        /// </summary>
        public int[] GetSpawnAreaList()
        {
            if (!IsLoaded) return new int[0];

            int count = GetSpawnAreaCount();
            if (count == 0) return new int[0];

            int[] ids = new int[count];
            ImmNativePlugin.GetSpawnAreaList(DocumentId, count, ids);
            return ids;
        }

        /// <summary>
        /// Get the currently active spawn area ID
        /// </summary>
        public int GetActiveSpawnAreaId()
        {
            if (!IsLoaded) return -1;
            return ImmNativePlugin.GetActiveSpawnAreaId(DocumentId);
        }

        /// <summary>
        /// Get the document's authored initial/default spawn area ID.
        /// </summary>
        public int GetInitialSpawnAreaId()
        {
            if (!IsLoaded) return -1;
            return ImmNativePlugin.GetInitialSpawnAreaId(DocumentId);
        }

        /// <summary>
        /// Set the active spawn area
        /// </summary>
        public void SetActiveSpawnAreaId(int spawnAreaId)
        {
            if (!IsLoaded) return;
            ImmNativePlugin.SetActiveSpawnAreaId(DocumentId, spawnAreaId);
        }

        /// <summary>
        /// Get spawn area information
        /// </summary>
        public SerializedSpawnArea? GetSpawnAreaInfo(int spawnAreaId)
        {
            if (!IsLoaded) return null;

            SerializedSpawnArea info;
            bool success = ImmNativePlugin.GetSpawnAreaInfo(DocumentId, spawnAreaId, out info);
            return success ? (SerializedSpawnArea?)info : null;
        }

        public SpawnAreaInfo? GetSpawnAreaInfoManaged(int spawnAreaId)
        {
            var native = GetSpawnAreaInfo(spawnAreaId);
            if (native == null)
                return null;

            SerializedSpawnArea n = native.Value;
            return new SpawnAreaInfo
            {
                Id = spawnAreaId,
                Name = n.GetName(),
                Version = n.mVersion,
                Type = n.mType,
                Animated = n.mAnimated,
                Volume = n.volume,
                Transform = n.transform,
                Locomotion = n.locomotion
            };
        }

        /// <summary>
        /// Copy a spawn area's authored screenshot into a new texture. The caller owns and must
        /// destroy the returned texture.
        /// </summary>
        /// <param name="linear">Pass true when the thumbnail feeds linear-space sampling.</param>
        /// <returns>False when the spawn area has no screenshot or its pixel format is unknown.</returns>
        public bool TryGetSpawnAreaThumbnail(int spawnAreaId, out Texture2D thumbnail, bool linear = false)
        {
            thumbnail = null;
            SerializedSpawnArea? native = GetSpawnAreaInfo(spawnAreaId);
            if (native == null)
                return false;

            SerializedSpawnArea.Screenshot shot = native.Value.screenshot;
            if (shot.pData == IntPtr.Zero || shot.width <= 0 || shot.height <= 0)
                return false;

            if (!TryGetTextureFormat(shot.format, out TextureFormat format, out int bytesPerPixel))
                return false;

            var texture = new Texture2D(shot.width, shot.height, format, false, linear);
            try
            {
                texture.LoadRawTextureData(shot.pData, shot.width * shot.height * bytesPerPixel);
                texture.Apply(false, false);
            }
            catch
            {
                UnityEngine.Object.Destroy(texture);
                throw;
            }

            thumbnail = texture;
            return true;
        }

        // ImmCore::piImage formats; spawn-area screenshots are single-plane images whose
        // format describes the interleaved layout.
        private static bool TryGetTextureFormat(uint imageFormat, out TextureFormat format, out int bytesPerPixel)
        {
            switch (imageFormat)
            {
                case 2: // FORMAT_I_GREY
                    format = TextureFormat.R8;
                    bytesPerPixel = 1;
                    return true;
                case 4: // FORMAT_I_16BIT
                    format = TextureFormat.R16;
                    bytesPerPixel = 2;
                    return true;
                case 5: // FORMAT_I_RG
                    format = TextureFormat.RG16;
                    bytesPerPixel = 2;
                    return true;
                case 6: // FORMAT_I_RGB
                    format = TextureFormat.RGB24;
                    bytesPerPixel = 3;
                    return true;
                case 7: // FORMAT_I_RGBA
                    format = TextureFormat.RGBA32;
                    bytesPerPixel = 4;
                    return true;
                case 8: // FORMAT_F_GREY
                    format = TextureFormat.RFloat;
                    bytesPerPixel = 4;
                    return true;
                case 9: // FORMAT_F_RG
                    format = TextureFormat.RGFloat;
                    bytesPerPixel = 8;
                    return true;
                case 10: // FORMAT_F_RGB
                    // Unity has no 3-channel float layout (RFloat/RGFloat/RGBAFloat only), and
                    // RGB48 would reinterpret the floats as 16-bit normalised values, so this
                    // format has to be reported as unsupported rather than mapped to something wrong.
                    format = TextureFormat.RGBA32;
                    bytesPerPixel = 0;
                    return false;
                case 11: // FORMAT_F_RGBA
                    format = TextureFormat.RGBAFloat;
                    bytesPerPixel = 16;
                    return true;
                default:
                    format = TextureFormat.RGBA32;
                    bytesPerPixel = 0;
                    return false;
            }
        }

        public SpawnAreaInfo[] GetSpawnAreas()
        {
            if (!IsLoaded) return new SpawnAreaInfo[0];
            int[] ids = GetSpawnAreaList();
            if (ids.Length == 0) return new SpawnAreaInfo[0];

            SpawnAreaInfo[] areas = new SpawnAreaInfo[ids.Length];
            int count = 0;
            for (int i = 0; i < ids.Length; i++)
            {
                var info = GetSpawnAreaInfoManaged(ids[i]);
                if (info.HasValue)
                {
                    areas[count++] = info.Value;
                }
            }

            if (count == areas.Length) return areas;

            SpawnAreaInfo[] trimmed = new SpawnAreaInfo[count];
            Array.Copy(areas, trimmed, count);
            return trimmed;
        }

        /// <summary>
        /// Resolve a spawn area's world pose using a document root transform.
        /// </summary>
        public bool TryGetSpawnAreaWorldPose(int spawnAreaId, Transform documentRoot, out Pose worldPose)
        {
            worldPose = default;
            if (documentRoot == null)
                return false;

            if (!TryGetSpawnAreaPose(spawnAreaId, out SpawnAreaPose pose))
                return false;

            Vector3 localPosition;
            Quaternion localRotation;
            ConvertSpawnAreaPoseToUnity(
                pose.GetPosition(),
                pose.GetRotation(),
                pose.sca,
                out localPosition,
                out localRotation);

            Vector3 worldPosition = documentRoot.TransformPoint(localPosition);
            Quaternion worldRotation = documentRoot.rotation * localRotation;
            worldPose = new Pose(worldPosition, worldRotation);
            return true;
        }

        /// <summary>
        /// Cheap per-frame spawn-area pose query: no name marshaling, no screenshot lookup.
        /// Prefer this over <see cref="GetSpawnAreaInfoManaged"/> in Update loops.
        /// </summary>
        public bool TryGetSpawnAreaPose(int spawnAreaId, out SpawnAreaPose pose)
        {
            pose = default;
            if (!IsLoaded || spawnAreaId < 0)
                return false;
            return ImmNativePlugin.GetSpawnAreaPose(DocumentId, spawnAreaId, out pose);
        }

        /// <summary>
        /// Cheap per-frame pose query for whichever spawn area is currently active.
        /// </summary>
        public bool TryGetActiveSpawnAreaPose(out SpawnAreaPose pose)
        {
            pose = default;
            int spawnAreaId = GetActiveSpawnAreaId();
            if (spawnAreaId < 0)
                return false;
            return TryGetSpawnAreaPose(spawnAreaId, out pose);
        }

        /// <summary>
        /// Whether the timeline wants the host to re-anchor to the authored viewpoint
        /// (a Quill "MakeDefault" keyframe on a spawn-area layer crossed during playback).
        /// </summary>
        public bool GetSpawnAreaNeedsUpdate()
        {
            if (!IsLoaded)
                return false;
            return ImmNativePlugin.GetSpawnAreaNeedsUpdate(DocumentId);
        }

        /// <summary>
        /// Clear (or set) the re-anchor request. Call with false once the rig has re-anchored.
        /// </summary>
        public void SetSpawnAreaNeedsUpdate(bool state)
        {
            if (!IsLoaded)
                return;
            ImmNativePlugin.SetSpawnAreaNeedsUpdate(DocumentId, state);
        }

        /// <summary>
        /// Read the re-anchor request and clear it in one step: true means "re-anchor now",
        /// and subsequent calls return false until the timeline crosses another MakeDefault key.
        /// </summary>
        public bool ConsumeSpawnAreaNeedsUpdate()
        {
            if (!GetSpawnAreaNeedsUpdate())
                return false;
            SetSpawnAreaNeedsUpdate(false);
            return true;
        }

        /// <summary>
        /// Resolve the active spawn area's world pose using a document root transform.
        /// </summary>
        public bool TryGetActiveSpawnAreaWorldPose(Transform documentRoot, out Pose worldPose)
        {
            worldPose = default;
            int spawnAreaId = GetActiveSpawnAreaId();
            if (spawnAreaId < 0)
                return false;
            return TryGetSpawnAreaWorldPose(spawnAreaId, documentRoot, out worldPose);
        }

        /// <summary>
        /// Resolve a spawn area to a view-target pose (for camera rig roots), compensating for current head position offset.
        /// </summary>
        public bool TryGetSpawnAreaViewTargetPose(
            int spawnAreaId,
            Transform documentRoot,
            Transform currentViewTarget,
            Transform currentHead,
            bool keepHeadHeightForFloorAreas,
            out Pose targetPose)
        {
            targetPose = default;
            if (documentRoot == null || currentViewTarget == null || currentHead == null)
                return false;

            if (!TryGetSpawnAreaPose(spawnAreaId, out SpawnAreaPose pose))
                return false;

            ConvertSpawnAreaPoseToUnity(
                pose.GetPosition(),
                pose.GetRotation(),
                pose.sca,
                out Vector3 localPosition,
                out Quaternion localRotation);
            Vector3 worldPosition = documentRoot.TransformPoint(localPosition);
            Quaternion worldRotation = documentRoot.rotation * localRotation;

            Quaternion headLocalRotation = Quaternion.Inverse(currentViewTarget.rotation) * currentHead.rotation;
            Quaternion targetRotation = worldRotation * Quaternion.Inverse(headLocalRotation);

            Vector3 headLocalPosition = currentViewTarget.InverseTransformPoint(currentHead.position);
            if (keepHeadHeightForFloorAreas && pose.isFloorLevel != 0)
            {
                headLocalPosition.y = 0.0f;
            }

            Vector3 targetPosition = worldPosition - (targetRotation * headLocalPosition);
            targetPose = new Pose(targetPosition, targetRotation);

            return true;
        }

        /// <summary>
        /// Resolve the active spawn area to a view-target pose (for camera rig roots).
        /// </summary>
        public bool TryGetActiveSpawnAreaViewTargetPose(
            Transform documentRoot,
            Transform currentViewTarget,
            Transform currentHead,
            bool keepHeadHeightForFloorAreas,
            out Pose targetPose)
        {
            targetPose = default;
            int spawnAreaId = GetActiveSpawnAreaId();
            if (spawnAreaId < 0)
                return false;

            return TryGetSpawnAreaViewTargetPose(
                spawnAreaId,
                documentRoot,
                currentViewTarget,
                currentHead,
                keepHeadHeightForFloorAreas,
                out targetPose);
        }

        #endregion

        #region Layers

        /// <summary>
        /// Get the number of layers in the document
        /// </summary>
        public int GetLayerCount()
        {
            if (!IsLoaded) return 0;
            if (!ImmNativePlugin.IsSequenceReady(DocumentId))
                return 0;
            return ImmNativePlugin.GetLayerCount(DocumentId);
        }

        /// <summary>
        /// Get layer info by index (preorder traversal)
        /// </summary>
        public LayerInfoNative? GetLayerInfo(int index)
        {
            if (!IsLoaded) return null;
            if (!ImmNativePlugin.IsSequenceReady(DocumentId))
                return null;
            if (ImmNativePlugin.GetLayerInfoByIndex(DocumentId, index, out LayerInfoNative info))
                return info;
            return null;
        }

        public LayerInfo? GetLayerInfoManaged(int index)
        {
            LayerInfoNative? native = GetLayerInfo(index);
            if (native == null) return null;
            return ToManagedLayerInfo(native.Value);
        }

        /// <summary>
        /// Get all layer infos (preorder traversal)
        /// </summary>
        public LayerInfoNative[] GetLayers()
        {
            if (!IsLoaded) return new LayerInfoNative[0];
            if (!ImmNativePlugin.IsSequenceReady(DocumentId))
                return new LayerInfoNative[0];
            int count = GetLayerCount();
            if (count <= 0) return new LayerInfoNative[0];

            LayerInfoNative[] layers = new LayerInfoNative[count];
            for (int i = 0; i < count; i++)
            {
                if (ImmNativePlugin.GetLayerInfoByIndex(DocumentId, i, out LayerInfoNative info))
                {
                    layers[i] = info;
                }
            }
            return layers;
        }

        public LayerInfo[] GetLayersManaged()
        {
            LayerInfoNative[] nativeLayers = GetLayers();
            if (nativeLayers.Length == 0) return new LayerInfo[0];

            LayerInfo[] managed = new LayerInfo[nativeLayers.Length];
            for (int i = 0; i < nativeLayers.Length; i++)
            {
                managed[i] = ToManagedLayerInfo(nativeLayers[i]);
            }
            return managed;
        }

        #endregion

        #region Layer Editing

        public bool SetLayerVisible(int layerId, bool visible)
        {
            if (!IsLoaded) return false;
            if (!IsSequenceReady()) return false;
            return ImmNativePlugin.SetLayerVisible(DocumentId, layerId, visible ? 1 : 0);
        }

        public bool ClearLayerVisibilityOverride(int layerId)
        {
            if (!IsLoaded) return false;
            if (!IsSequenceReady()) return false;
            return ImmNativePlugin.ClearLayerVisibilityOverride(DocumentId, layerId);
        }

        public bool SetLayerOpacity(int layerId, float opacity)
        {
            if (!IsLoaded) return false;
            if (!IsSequenceReady()) return false;
            return ImmNativePlugin.SetLayerOpacity(DocumentId, layerId, opacity);
        }

        public bool SetLayerTransform(int layerId, Matrix4x4 layerToWorld)
        {
            if (!IsLoaded) return false;
            if (!IsSequenceReady()) return false;
            float[] matrix = MatrixToFloatArray(layerToWorld);
            return ImmNativePlugin.SetLayerTransform(DocumentId, layerId, matrix);
        }

        public bool ClearLayerTransformOverride(int layerId)
        {
            if (!IsLoaded) return false;
            if (!IsSequenceReady()) return false;
            return ImmNativePlugin.ClearLayerTransformOverride(DocumentId, layerId);
        }

        public bool SetLayerTransform(int layerId, Transform transform)
        {
            if (transform == null) return false;
            return SetLayerTransform(layerId, transform.localToWorldMatrix);
        }

        public LayerDiagnostics? GetLayerDiagnostics(int layerId)
        {
            if (!IsLoaded) return null;
            if (!IsSequenceReady()) return null;

            LayerDiagnosticsNative diag;
            if (!ImmNativePlugin.GetLayerDiagnostics(DocumentId, layerId, out diag))
                return null;

            return new LayerDiagnostics
            {
                HasVisibilityKeys = diag.hasVisibilityKeys != 0,
                HasOpacityKeys = diag.hasOpacityKeys != 0,
                IsVisible = diag.isVisible != 0,
                Opacity = diag.opacity,
                IsWorldVisible = diag.isWorldVisible != 0,
                WorldOpacity = diag.worldOpacity,
                ParentId = diag.parentId,
                VisibilityOverrideEnabled = diag.visibilityOverrideEnabled != 0,
                VisibilityOverrideValue = diag.visibilityOverrideValue != 0,
                HasTransformKeys = diag.hasTransformKeys != 0,
                TransformOverrideEnabled = diag.transformOverrideEnabled != 0
            };
        }

        #endregion

        #region Cleanup

        /// <summary>
        /// Unload the document
        /// </summary>
        public void Unload()
        {
            if (!IsLoaded) return;

            ImmNativePlugin.Unload(DocumentId);
            IsLoaded = false;
        }

        #endregion

        #region Helpers

        private static LayerInfo ToManagedLayerInfo(LayerInfoNative native)
        {
            Bounds bounds = default;
            bool hasBounds = native.hasBBox != 0;
            if (hasBounds)
            {
                bounds = native.bbox.ToUnityBounds();
            }

            return new LayerInfo
            {
                Id = native.id,
                Type = (LayerType)native.type,
                ParentId = native.parentId,
                IsTimeline = native.isTimeline != 0,
                IsLoaded = native.isLoaded != 0,
                IsVisible = native.isVisible != 0,
                Opacity = native.opacity,
                HasBounds = hasBounds,
                Bounds = bounds,
                NumChildren = native.numChildren,
                AssetId = native.assetId,
                PaintNumDrawings = native.paintNumDrawings,
                PaintNumFrames = native.paintNumFrames,
                PaintNumStrokes = native.paintNumStrokes,
                Name = native.name,
                FullName = native.fullName
            };
        }

        private static float[] MatrixToFloatArray(Matrix4x4 matrix)
        {
            float[] result = new float[16];
            for (int i = 0; i < 16; i++)
            {
                result[i] = matrix[i];
            }
            return result;
        }

        private static void ConvertSpawnAreaPoseToUnity(
            Vector3 immPosition,
            Quaternion immRotation,
            float immScale,
            out Vector3 unityPosition,
            out Quaternion unityRotation)
        {
            Matrix4x4 imm = Matrix4x4.TRS(immPosition, immRotation, Vector3.one * immScale);
            Matrix4x4 flip = Matrix4x4.Scale(new Vector3(1f, 1f, -1f));
            Matrix4x4 converted = flip * imm * flip;

            unityPosition = converted.GetColumn(3);

            Vector3 up = converted.GetColumn(1);
            Vector3 forward = converted.GetColumn(2);
            if (up.sqrMagnitude < 0.0001f)
                up = Vector3.up;
            if (forward.sqrMagnitude < 0.0001f)
                forward = Vector3.forward;

            unityRotation = Quaternion.LookRotation(forward.normalized, up.normalized);
        }

        #endregion
    }
}
