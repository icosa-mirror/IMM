using System;

namespace ImmPlayer.Authoring
{
    public enum ImmAuthoringImageEncoding { Rgb8 = 0, Rgba8 = 1, Png = 2, Jpeg = 3 }

    /// <summary>Immutable image bytes shared safely by document revisions and snapshots.</summary>
    public sealed class ImmAuthoringImage
    {
        private readonly byte[] _data;
        public int Width { get; }
        public int Height { get; }
        public int Channels { get; }
        public ImmAuthoringImageEncoding Encoding { get; }
        /// <summary>Encoded IMM v1 previews use BGR; new raw previews use RGB/v2.</summary>
        public bool EncodedBgr { get; }
        public int ByteCount => _data.Length;
        public byte[] CopyData() => (byte[])_data.Clone();

        private ImmAuthoringImage(int width, int height, int channels, ImmAuthoringImageEncoding encoding, bool bgr, byte[] data)
        {
            Width = width; Height = height; Channels = channels; Encoding = encoding; EncodedBgr = bgr;
            _data = (byte[])data.Clone();
        }

        public static ImmAuthoringResult<ImmAuthoringImage> Create(int width, int height, int channels,
            ImmAuthoringImageEncoding encoding, byte[] data, bool encodedBgr = false)
        {
            if (width <= 0 || height <= 0 || (channels != 3 && channels != 4) || data == null || data.Length == 0 ||
                !Enum.IsDefined(typeof(ImmAuthoringImageEncoding), encoding))
                return ImmAuthoringResult<ImmAuthoringImage>.Failure(ImmAuthoringErrorCode.InvalidArgument, "Image dimensions, channels, encoding and bytes must be valid.");
            long pixels = (long)width * height;
            if (pixels > int.MaxValue / channels)
                return ImmAuthoringResult<ImmAuthoringImage>.Failure(ImmAuthoringErrorCode.ResourceLimitExceeded, "Decoded image exceeds the supported byte range.");
            bool raw = encoding == ImmAuthoringImageEncoding.Rgb8 || encoding == ImmAuthoringImageEncoding.Rgba8;
            if (raw && (encodedBgr || channels != (encoding == ImmAuthoringImageEncoding.Rgb8 ? 3 : 4) || data.LongLength != pixels * channels))
                return ImmAuthoringResult<ImmAuthoringImage>.Failure(ImmAuthoringErrorCode.InvalidArgument, "Raw image bytes must match RGB/RGBA dimensions exactly.");
            if (encoding == ImmAuthoringImageEncoding.Jpeg &&
                (channels != 3 || data.Length < 3 || data[0] != 255 || data[1] != 216 || data[2] != 255))
                return ImmAuthoringResult<ImmAuthoringImage>.Failure(ImmAuthoringErrorCode.InvalidArgument, "Invalid JPEG image signature/channels.");
            if (encoding == ImmAuthoringImageEncoding.Png &&
                (data.Length < 8 || data[0] != 137 || data[1] != 80 || data[2] != 78 || data[3] != 71 ||
                 data[4] != 13 || data[5] != 10 || data[6] != 26 || data[7] != 10))
                return ImmAuthoringResult<ImmAuthoringImage>.Failure(ImmAuthoringErrorCode.InvalidArgument, "Invalid PNG image signature.");
            return ImmAuthoringResult<ImmAuthoringImage>.Success(new ImmAuthoringImage(width, height, channels, encoding, encodedBgr, data));
        }
    }
}
