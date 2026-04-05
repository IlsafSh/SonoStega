#pragma once

#include "core/io/WavFile.h"
#include <QByteArray>
#include <QString>
#include <vector>
#include <cstdint>

// Implements LSB steganography for PCM WAV audio.
//
// Wire protocol (bitstream layout):
//   [32 bits: payload length in bytes, little-endian uint32]
//   [payload_length * 8 bits: payload data]
//
// Each sample contributes nBits bits to the stream, stored in the sample's nBits least significant bits (MSB of the chunk first)
//
// Sequential mode: samples are accessed in natural order 0, 1, 2, ...
// Keyed mode: samples are accessed in a pseudo-random order produced by Fisher-Yates shuffle seeded with FNV-1a hash of the password
class LsbEmbedder
{
public:
    struct Result {
        bool ok = false;
        QString error;
    };

    // --- Sequential mode (samples 0, 1, 2, ...) ------------------------------
    static Result embed(WavFile &wav, const QByteArray &payload, int nBits);
    static Result extract(const WavFile &wav, QByteArray &outPayload, int nBits);

    // --- Keyed mode (pseudo-random sample order derived from password) --------
    static Result embedKeyed(WavFile &wav, const QByteArray &payload, int nBits, const QString &password);
    static Result extractKeyed(const WavFile &wav, QByteArray &outPayload, int nBits, const QString &password);

private:
    // Bit-level helpers (MSB-first within each byte)
    static int  getBit(const uint8_t *data, size_t pos);
    static void setBit(uint8_t *data, size_t pos, int bit);

    // Build and validate the bitstream (4-byte LE length + payload)
    // Returns the stream on success; sets errOut and returns {} on failure
    static QByteArray buildStream(const QByteArray &payload, int nBits, size_t totalSamples, QString &errOut);

    // Core loop: write stream bits into sample LSBs
    // indices == nullptr → sequential (slot i → sample i)
    // indices != nullptr → keyed (slot i → sample (*indices)[i])
    static void embedCore(std::vector<int32_t> &samples, const uint8_t *stream, size_t totalBits, int nBits, const std::vector<uint32_t> *indices);

    // Core loop: read sample LSBs into output bytes, starting at bitOffset in the sample stream
    static void extractCore(const std::vector<int32_t>  &samples, uint8_t *outData, size_t &bitOffset, size_t bitCount, int nBits, const std::vector<uint32_t> *indices);

    // Shared extract logic used by both sequential and keyed modes
    static Result doExtract(const WavFile &wav, QByteArray &outPayload, int nBits, const std::vector<uint32_t> *indices);

    // Keyed-mode helpers
    static std::vector<uint32_t> keyedIndices(size_t n, uint64_t seed);
    static uint64_t passwordSeed(const QString &password);
};
