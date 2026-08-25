#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// A minimal RIFF/WAVE reader for the placeholder sample path: 16-bit PCM,
// mono or stereo (stereo is downmixed), any sample rate. Pure — bytes in,
// samples out — so the suite can prove it against synthetic files, refuse
// truncated ones, and never crash on garbage. Deliberately NOT a general
// audio importer: compressed formats, 24/32-bit, extensible headers and
// multi-channel all refuse cleanly and the caller falls back to the synth.
// ---------------------------------------------------------------------------
namespace BreakerWave
{
    struct FParsedWave
    {
        TArray<int16> Samples;   // mono
        int32 SampleRate = 0;
        bool IsValid() const { return SampleRate > 0 && Samples.Num() > 0; }
    };

    inline uint32 ReadU32(const uint8* Data, int32 Offset)
    {
        return static_cast<uint32>(Data[Offset]) | (static_cast<uint32>(Data[Offset + 1]) << 8)
            | (static_cast<uint32>(Data[Offset + 2]) << 16) | (static_cast<uint32>(Data[Offset + 3]) << 24);
    }
    inline uint16 ReadU16(const uint8* Data, int32 Offset)
    {
        return static_cast<uint16>(Data[Offset]) | (static_cast<uint16>(Data[Offset + 1]) << 8);
    }

    inline FParsedWave ParseWav(const TArray<uint8>& Bytes)
    {
        FParsedWave Out;
        if (Bytes.Num() < 44) return Out;
        const uint8* D = Bytes.GetData();
        if (FMemory::Memcmp(D, "RIFF", 4) != 0 || FMemory::Memcmp(D + 8, "WAVE", 4) != 0) return Out;

        // Walk the chunks: fmt then data, in whatever order and with
        // whatever strangers (LIST, fact) sit between them.
        int32 Offset = 12;
        uint16 Format = 0, Channels = 0, BitsPerSample = 0;
        uint32 SampleRate = 0;
        int32 DataOffset = -1, DataSize = 0;
        while (Offset + 8 <= Bytes.Num())
        {
            const uint32 ChunkSize = ReadU32(D, Offset + 4);
            if (FMemory::Memcmp(D + Offset, "fmt ", 4) == 0 && Offset + 8 + 16 <= Bytes.Num())
            {
                Format = ReadU16(D, Offset + 8);
                Channels = ReadU16(D, Offset + 10);
                SampleRate = ReadU32(D, Offset + 12);
                BitsPerSample = ReadU16(D, Offset + 22);
            }
            else if (FMemory::Memcmp(D + Offset, "data", 4) == 0)
            {
                DataOffset = Offset + 8;
                DataSize = static_cast<int32>(ChunkSize);
            }
            // Chunks are word-aligned; a truncated final chunk clamps.
            Offset += 8 + static_cast<int32>(ChunkSize) + (ChunkSize & 1u);
        }

        if (Format != 1 /*PCM*/ || BitsPerSample != 16 || SampleRate == 0) return Out;
        if (Channels != 1 && Channels != 2) return Out;
        if (DataOffset < 0) return Out;
        DataSize = FMath::Min(DataSize, Bytes.Num() - DataOffset);
        const int32 FrameBytes = Channels * 2;
        const int32 Frames = DataSize / FrameBytes;
        if (Frames <= 0) return Out;

        Out.SampleRate = static_cast<int32>(SampleRate);
        Out.Samples.SetNumUninitialized(Frames);
        for (int32 Frame = 0; Frame < Frames; ++Frame)
        {
            const int32 Base = DataOffset + Frame * FrameBytes;
            const int16 Left = static_cast<int16>(ReadU16(D, Base));
            if (Channels == 1)
            {
                Out.Samples[Frame] = Left;
            }
            else
            {
                const int16 Right = static_cast<int16>(ReadU16(D, Base + 2));
                Out.Samples[Frame] = static_cast<int16>((static_cast<int32>(Left) + Right) / 2);
            }
        }
        return Out;
    }
}
