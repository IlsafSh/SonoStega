#pragma once

#include "WavFile.h"
#include <QByteArray>
#include <QString>

// Implements LSB steganography for PCM WAV audio
//
// Wire protocol (bitstream layout):
//   [32 bits: payload length in bytes, little-endian uint32]
//   [payload_length * 8 bits: payload data]
//
// Each sample contributes nBits bits to the stream, stored in the sample's nBits least significant bits (MSB of the chunk first)
//
// Sequential mode:  samples are accessed in order 0, 1, 2, ...
// Keyed mode:       samples are accessed in a pseudo-random order
//                   derived from a password hash (added in next commit)
class LsbEmbedder
{
public:
    struct Result {
        bool ok = false;
        QString error;
    };

    // Embed payload into wav samples (modifies wav in-place). Sequential mode
    static Result embed(WavFile &wav, const QByteArray &payload, int nBits);

    // Extract payload from wav samples. Sequential mode
    static Result extract(const WavFile &wav, QByteArray &outPayload, int nBits);

private:
    // Bit-level helpers (MSB-first within each byte)
    static int  getBit(const uint8_t *data, size_t pos);
    static void setBit(uint8_t *data, size_t pos, int bit);
};
