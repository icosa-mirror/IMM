using System;

namespace ImmPlayer.Authoring
{
    /// <summary>Immutable signed, interleaved PCM16 little-endian audio for offline authoring.</summary>
    public sealed class ImmAuthoringAudio
    {
        private readonly byte[] _data;
        public int SampleRate { get; }
        public int Channels { get; }
        public int FrameCount => _data.Length / (Channels * 2);
        public int ByteCount => _data.Length;
        public byte[] CopyData() => (byte[])_data.Clone();

        private ImmAuthoringAudio(int sampleRate, int channels, byte[] data)
        { SampleRate = sampleRate; Channels = channels; _data = (byte[])data.Clone(); }

        public static ImmAuthoringResult<ImmAuthoringAudio> CreatePcm16(int sampleRate, int channels, byte[] data)
        {
            if (sampleRate < 8000 || sampleRate > 192000 || (channels != 1 && channels != 2 && channels != 4) ||
                data == null || data.Length == 0 || data.Length % (channels * 2) != 0)
                return ImmAuthoringResult<ImmAuthoringAudio>.Failure(ImmAuthoringErrorCode.InvalidArgument,
                    "PCM16 requires a rate of 8000-192000 Hz, 1/2/4 channels and complete nonempty sample frames.");
            return ImmAuthoringResult<ImmAuthoringAudio>.Success(new ImmAuthoringAudio(sampleRate, channels, data));
        }
    }
}
