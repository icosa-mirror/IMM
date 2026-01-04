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

        #endregion
    }
}
