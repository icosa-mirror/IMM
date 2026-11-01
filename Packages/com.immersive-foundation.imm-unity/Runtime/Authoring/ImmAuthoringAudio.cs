using System;

namespace ImmPlayer.Authoring
{
    public enum ImmAuthoringAudioEncoding { Pcm16 = 0, Wav = 3, Vorbis = 4, Opus = 5 }

    /// <summary>Immutable PCM16 samples or an original encoded audio asset for offline authoring.</summary>
    public sealed class ImmAuthoringAudio
    {
        private readonly byte[] _data;
        public int SampleRate { get; }
        public int Channels { get; }
        public ImmAuthoringAudioEncoding Encoding { get; }
        /// <summary>PCM frame count; -1 for encoded assets, whose packets are not decoded here.</summary>
        public int FrameCount => Encoding == ImmAuthoringAudioEncoding.Pcm16 ? _data.Length / (Channels * 2) : -1;
        public int ByteCount => _data.Length;
        public byte[] CopyData() => (byte[])_data.Clone();

        private ImmAuthoringAudio(int sampleRate, int channels, ImmAuthoringAudioEncoding encoding, byte[] data)
        { SampleRate = sampleRate; Channels = channels; Encoding = encoding; _data = (byte[])data.Clone(); }

        public static ImmAuthoringResult<ImmAuthoringAudio> CreatePcm16(int sampleRate, int channels, byte[] data)
        {
            if (sampleRate < 8000 || sampleRate > 192000 || (channels != 1 && channels != 2 && channels != 4) ||
                data == null || data.Length == 0 || data.Length % (channels * 2) != 0)
                return ImmAuthoringResult<ImmAuthoringAudio>.Failure(ImmAuthoringErrorCode.InvalidArgument,
                    "PCM16 requires a rate of 8000-192000 Hz, 1/2/4 channels and complete nonempty sample frames.");
            return ImmAuthoringResult<ImmAuthoringAudio>.Success(new ImmAuthoringAudio(sampleRate, channels, ImmAuthoringAudioEncoding.Pcm16, data));
        }

        /// <summary>Retain encoded bytes. Metadata/container framing is verified by the native exporter; packets are not decoded.</summary>
        public static ImmAuthoringResult<ImmAuthoringAudio> CreateEncoded(int sampleRate, int channels, ImmAuthoringAudioEncoding encoding, byte[] data)
        {
            bool wave = encoding == ImmAuthoringAudioEncoding.Wav;
            bool ogg = encoding == ImmAuthoringAudioEncoding.Vorbis || encoding == ImmAuthoringAudioEncoding.Opus;
            bool signature = data != null && (wave ? data.Length >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
                data[8] == 'W' && data[9] == 'A' && data[10] == 'V' && data[11] == 'E' :
                ogg && data.Length >= 27 && data[0] == 'O' && data[1] == 'g' && data[2] == 'g' && data[3] == 'S');
            if (!signature || sampleRate < 8000 || sampleRate > 192000 || (channels != 1 && channels != 2 && channels != 4) ||
                (encoding == ImmAuthoringAudioEncoding.Opus && sampleRate != 48000))
                return ImmAuthoringResult<ImmAuthoringAudio>.Failure(ImmAuthoringErrorCode.InvalidArgument, "Encoded audio format, signature or metadata is unsupported.");
            return ImmAuthoringResult<ImmAuthoringAudio>.Success(new ImmAuthoringAudio(sampleRate, channels, encoding, data));
        }
    }
}
