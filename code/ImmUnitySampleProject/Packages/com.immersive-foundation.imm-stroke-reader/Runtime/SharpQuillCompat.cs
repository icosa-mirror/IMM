using System;
using System.IO;
using System.Runtime.InteropServices;
using ImmPlayer;
using SharpQuill;
using UnityEngine;
using Color = SharpQuill.Color;
using Quaternion = SharpQuill.Quaternion;
using Transform = SharpQuill.Transform;
using Vector3 = UnityEngine.Vector3;

namespace ImmStrokeReader
{
    public static class SharpQuillCompat
    {
        private const string LogPrefix = "[IMM2QUILL_20260209A] ";

        public static Sequence ReadImmAsSequence(string path, bool includePictures = false, int chapterIndex = -1)
        {
            if (string.IsNullOrEmpty(path))
            {
                return null;
            }

            if (!EnsureInitialized())
            {
                return null;
            }

            if (!TryLoadDocument(path, out int docId))
            {
                return null;
            }

            try
            {
                if (chapterIndex >= 0)
                {
                    int chapterCount = ImmPlayer.ImmStrokeReader.StrokeReader_GetChapterCount(docId);
                    if (chapterIndex >= chapterCount)
                    {
                        Debug.LogWarning($"{LogPrefix}Requested chapter {chapterIndex} is out of range (chapterCount={chapterCount}), falling back to chapter 0");
                        chapterIndex = 0;
                    }
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_SetChapter(docId, chapterIndex))
                    {
                        Debug.LogWarning($"{LogPrefix}StrokeReader_SetChapter({chapterIndex}) failed, using default chapter");
                    }
                }
                return ConvertDocument(docId, includePictures, chapterIndex);
            }
            finally
            {
                ImmPlayer.ImmStrokeReader.StrokeReader_Unload(docId);
            }
        }

        /// <summary>
        /// Returns the number of chapters in an IMM file, or 0 on failure.
        /// Uses a fast path that reads only the scene graph header without loading layer assets.
        /// </summary>
        public static int GetImmChapterCount(string path)
        {
            if (string.IsNullOrEmpty(path))
            {
                return 0;
            }

            if (!EnsureInitialized())
            {
                return 0;
            }

            return ImmPlayer.ImmStrokeReader.StrokeReader_GetChapterCountFromFile(path);
        }

        private static bool EnsureInitialized()
        {
            if (ImmPlayer.ImmStrokeReader.StrokeReader_IsInitialized())
            {
                return true;
            }
            return ImmPlayer.ImmStrokeReader.StrokeReader_Init(null) == 0;
        }

        private static bool TryLoadDocument(string path, out int docId)
        {
            docId = ImmPlayer.ImmStrokeReader.StrokeReader_LoadFromFile(path);
            if (docId >= 0)
            {
                return true;
            }

            Debug.LogWarning($"{LogPrefix}StrokeReader_LoadFromFile failed for '{path}', trying memory fallback");

            byte[] immBytes;
            try
            {
                immBytes = File.ReadAllBytes(path);
            }
            catch
            {
                return false;
            }

            if (immBytes == null || immBytes.Length == 0)
            {
                return false;
            }

            IntPtr data = Marshal.AllocHGlobal(immBytes.Length);
            try
            {
                Marshal.Copy(immBytes, 0, data, immBytes.Length);
                docId = ImmPlayer.ImmStrokeReader.StrokeReader_LoadFromMemory(data, immBytes.Length);
                if (docId >= 0)
                {
                    Debug.Log($"{LogPrefix}Memory fallback succeeded for '{path}'");
                }
                return docId >= 0;
            }
            finally
            {
                Marshal.FreeHGlobal(data);
            }
        }

        public static bool WriteImmAsQuillProject(string immPath, string outputFolder, bool includePictures = false)
        {
            Sequence sequence = ReadImmAsSequence(immPath, includePictures);
            if (sequence == null)
            {
                return false;
            }

            QuillSequenceWriter.Write(sequence, outputFolder);
            return true;
        }

        private static Sequence ConvertDocument(int docId, bool includePictures, int chapterIndex = -1)
        {
            Sequence sequence = Sequence.CreateDefault();
            if (sequence.RootLayer == null)
            {
                return sequence;
            }

            uint maxStrokeId = 0;
            int layerCount = ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerCount(docId);
            for (int layerIdx = 0; layerIdx < layerCount; layerIdx++)
            {
                if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerInfo(docId, layerIdx, out StrokeLayerInfo info))
                {
                    continue;
                }

                bool isPicture = info.type == 4;
                if (isPicture && !includePictures)
                {
                    continue;
                }

                Layer layer = isPicture
                    ? ConvertPictureLayer(docId, layerIdx, info)
                    : ConvertPaintLayer(docId, layerIdx, info, ref maxStrokeId, chapterIndex);

                if (layer != null)
                {
                    sequence.RootLayer.Children.Add(layer);
                }
            }

            sequence.LastStrokeId = maxStrokeId;
            return sequence;
        }

        private static LayerPaint ConvertPaintLayer(int docId, int layerIdx, StrokeLayerInfo info, ref uint maxStrokeId, int chapterIndex = -1)
        {
            LayerPaint layer = new LayerPaint(info.name);
            ApplyCommonLayerProperties(layer, docId, layerIdx, info);

            // Get animation info
            int frameRate = 24;
            int numFrames = 0;
            int maxRepeatCount = 0;
            bool hasAnimationInfo = ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerAnimationInfo(docId, layerIdx, out frameRate, out numFrames, out maxRepeatCount);
            
            if (hasAnimationInfo)
            {
                layer.Framerate = frameRate;
                layer.MaxRepeatCount = maxRepeatCount;
            }

            // Get frame buffer (maps timeline frames to drawing indices)
            int[] frameBuffer = null;
            if (numFrames > 0)
            {
                frameBuffer = new int[numFrames];
                int framesRead = ImmPlayer.ImmStrokeReader.StrokeReader_GetFrameBuffer(docId, layerIdx, frameBuffer, numFrames);
                if (framesRead != numFrames)
                {
                    Debug.LogWarning($"{LogPrefix}Frame buffer mismatch: expected {numFrames}, got {framesRead}");
                    frameBuffer = null;
                }
            }

            // Load all drawings
            int drawingCount = ImmPlayer.ImmStrokeReader.StrokeReader_GetDrawingCount(docId, layerIdx);
            for (int drawingIdx = 0; drawingIdx < drawingCount; drawingIdx++)
            {
                Drawing drawing = new Drawing();
                int strokeCount = ImmPlayer.ImmStrokeReader.StrokeReader_GetStrokeCount(docId, layerIdx, drawingIdx);

                for (int strokeIdx = 0; strokeIdx < strokeCount; strokeIdx++)
                {
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetStrokeInfo(docId, layerIdx, drawingIdx, strokeIdx, out StrokeInfo strokeInfo))
                    {
                        continue;
                    }

                    if (strokeInfo.numPoints <= 0)
                    {
                        continue;
                    }

                    StrokePoint[] points = new StrokePoint[strokeInfo.numPoints];
                    if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetStrokePoints(docId, layerIdx, drawingIdx, strokeIdx, points, points.Length))
                    {
                        continue;
                    }

                    uint strokeId = (uint)strokeIdx;
                    if (strokeId > maxStrokeId)
                    {
                        maxStrokeId = strokeId;
                    }

                    Stroke stroke = new Stroke
                    {
                        Id = strokeId,
                        BrushType = ConvertBrushType(strokeInfo.brushType),
                        DisableRotationalOpacity = strokeInfo.visibilityMode == 1,
                        BoundingBox = new BoundingBox(
                            strokeInfo.bboxMinX,
                            strokeInfo.bboxMinY,
                            strokeInfo.bboxMinZ,
                            strokeInfo.bboxMaxX,
                            strokeInfo.bboxMaxY,
                            strokeInfo.bboxMaxZ)
                    };

                    for (int p = 0; p < points.Length; p++)
                    {
                        StrokePoint pt = points[p];
                        stroke.Vertices.Add(new Vertex(
                            new SharpQuill.Vector3(pt.px, pt.py, pt.pz),
                            new SharpQuill.Vector3(pt.nx, pt.ny, pt.nz),
                            new SharpQuill.Vector3(pt.dx, pt.dy, pt.dz),
                            new Color(pt.r, pt.g, pt.b),
                            pt.alpha,
                            pt.width));
                    }

                    drawing.Data.Strokes.Add(stroke);
                }

                drawing.UpdateBoundingBox(true);
                layer.Drawings.Add(drawing);
            }

            // Build frame buffer using original mapping
            if (frameBuffer != null && numFrames > 0)
            {
                for (int i = 0; i < numFrames; i++)
                {
                    int drawingIndex = frameBuffer[i];
                    // Clamp to valid range
                    if (drawingIndex < 0 || drawingIndex >= layer.Drawings.Count)
                    {
                        drawingIndex = 0;
                    }
                    layer.Frames.Add(drawingIndex);
                }
            }
            else if (layer.Drawings.Count > 0)
            {
                // Fallback: 1:1 mapping if no frame buffer available
                for (int i = 0; i < layer.Drawings.Count; i++)
                {
                    layer.Frames.Add(i);
                }
            }
            else
            {
                // No drawings at all - add empty one
                layer.Drawings.Add(new Drawing());
                layer.Frames.Add(0);
            }

            // For chapter-specific loading, override Frames to point at the single
            // drawing that corresponds to the requested chapter's start frame.
            if (chapterIndex >= 0 && layer.Drawings.Count > 0)
            {
                int drawingIndex = ImmPlayer.ImmStrokeReader.StrokeReader_GetDrawingIndexForChapter(docId, layerIdx, chapterIndex);
                if (drawingIndex < 0 || drawingIndex >= layer.Drawings.Count)
                    drawingIndex = 0;
                layer.Frames.Clear();
                layer.Frames.Add(drawingIndex);
            }

            return layer;
        }

        private static LayerPicture ConvertPictureLayer(int docId, int layerIdx, StrokeLayerInfo info)
        {
            if (!ImmPlayer.ImmStrokeReader.StrokeReader_GetPictureInfo(docId, layerIdx, out StrokePictureInfo pictureInfo))
            {
                return null;
            }

            if (pictureInfo.width <= 0 || pictureInfo.height <= 0 || pictureInfo.dataSize <= 0)
            {
                return null;
            }

            byte[] pixels = new byte[pictureInfo.dataSize];
            IntPtr buffer = Marshal.AllocHGlobal(pictureInfo.dataSize);
            try
            {
                int bytesRead = ImmPlayer.ImmStrokeReader.StrokeReader_GetPicturePixelData(docId, layerIdx, buffer, pictureInfo.dataSize);
                if (bytesRead <= 0)
                {
                    return null;
                }

                Marshal.Copy(buffer, pixels, 0, pictureInfo.dataSize);
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }

            LayerPicture layer = new LayerPicture(info.name)
            {
                PictureType = ConvertPictureType(pictureInfo.contentType),
                ViewerLocked = pictureInfo.isViewerLocked != 0,
                Data = new PictureData
                {
                    Width = pictureInfo.width,
                    Height = pictureInfo.height,
                    HasAlpha = pictureInfo.hasAlpha != 0,
                    Pixels = pixels
                }
            };

            ApplyCommonLayerProperties(layer, docId, layerIdx, info);
            return layer;
        }

        private static void ApplyCommonLayerProperties(Layer layer, int docId, int layerIdx, StrokeLayerInfo info)
        {
            layer.Visible = info.visible != 0;
            layer.Opacity = info.opacity;
            layer.Pivot = ConvertPivot(info);

            // All IMM layers are placed as direct children of the sequence root (hierarchy is flattened).
            // Use the world transform so that ancestor transforms are correctly included.
            if (ImmPlayer.ImmStrokeReader.StrokeReader_GetLayerTransform(docId, layerIdx, out StrokeLayerTransform _, out StrokeLayerTransform world))
            {
                layer.Transform = ConvertTransform(world);
            }
            else
            {
                layer.Transform = Transform.Identity;
            }
        }

        private static Transform ConvertTransform(StrokeLayerTransform source)
        {
            float rotW = source.rotW;
            if (source.rotX == 0f && source.rotY == 0f && source.rotZ == 0f && source.rotW == 0f)
            {
                rotW = 1f;
            }

            float scale = source.scale;
            if (scale == 0f || float.IsNaN(scale) || float.IsInfinity(scale))
            {
                scale = 1f;
            }

            return new Transform
            {
                Rotation = new Quaternion(source.rotX, source.rotY, source.rotZ, rotW),
                Scale = scale,
                Flip = ConvertFlip(source.flip),
                Translation = new SharpQuill.Vector3(source.transX, source.transY, source.transZ)
            };
        }

        private static Transform ConvertPivot(StrokeLayerInfo source)
        {
            float rotW = source.pivotRotW;
            if (source.pivotRotX == 0f && source.pivotRotY == 0f && source.pivotRotZ == 0f && source.pivotRotW == 0f)
            {
                rotW = 1f;
            }

            float scale = source.pivotScale;
            if (scale == 0f || float.IsNaN(scale) || float.IsInfinity(scale))
            {
                scale = 1f;
            }

            return new Transform
            {
                Rotation = new Quaternion(source.pivotRotX, source.pivotRotY, source.pivotRotZ, rotW),
                Scale = scale,
                Flip = ConvertFlip(source.pivotFlip),
                Translation = new SharpQuill.Vector3(source.pivotTransX, source.pivotTransY, source.pivotTransZ)
            };
        }

        private static BrushType ConvertBrushType(int immBrushType)
        {
            switch (immBrushType)
            {
                case 1:
                    return BrushType.Ribbon;
                case 2:
                    return BrushType.Cylinder;
                case 3:
                    return BrushType.Ellipse;
                case 4:
                    return BrushType.Cube;
                default:
                    return BrushType.Cylinder;
            }
        }

        private static PictureType ConvertPictureType(int contentType)
        {
            switch (contentType)
            {
                case 0:
                    return PictureType.TwoD;
                case 1:
                    return PictureType.ThreeSixty_Equirect_Mono;
                case 2:
                    return PictureType.ThreeSixty_Equirect_Stereo;
                default:
                    return PictureType.Unknown;
            }
        }

        private static string ConvertFlip(int flip)
        {
            switch (flip)
            {
                case 1:
                    return "X";
                case 2:
                    return "Y";
                case 3:
                    return "Z";
                default:
                    return "N";
            }
        }
    }
}
