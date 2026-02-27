#pragma once

#include <QString>
#include <vector>
#include <cstdint>

// Reads and writes uncompressed PCM WAV files (8-bit, 16-bit, 24-bit)
// Samples are stored as int32_t in their native range:
//   8-bit  unsigned: 0 … 255
//   16-bit signed:   -32768 … 32767
//   24-bit signed:   -8388608 … 8388607
class WavFile
{
public:
    struct Header {
        uint16_t audioFormat   = 1;      // always 1 (PCM)
        uint16_t numChannels   = 0;
        uint32_t sampleRate    = 0;
        uint16_t bitsPerSample = 0;
        uint32_t numSamples    = 0;      // total interleaved samples
    };

    // Load WAV from file. Returns false on error; see errorString()
    bool load(const QString &path);

    // Save WAV to file using current header and sample data
    bool save(const QString &path) const;

    bool isValid() const { return m_valid; }
    QString errorString() const { return m_error; }

    const Header &header() const { return m_header; }

    std::vector<int32_t> &samples() { return m_samples; }
    const std::vector<int32_t> &samples() const { return m_samples; }

    // Maximum payload size in bytes when using nBits LSBs per sample
    // Accounts for 4 bytes (32 bits) reserved for the embedded length header
    uint64_t capacityBytes(int nBits) const;

    // Total duration in seconds
    double durationSeconds() const;

    // Human-readable format description, e.g. "44100 Hz, stereo, 16-bit PCM"
    QString formatDescription() const;

private:
    bool parseSamples(const uint8_t *data, uint32_t size);

    Header               m_header;
    std::vector<int32_t> m_samples;
    QString              m_error;
    bool                 m_valid = false;
    // > 0 only for WAVE_FORMAT_EXTENSIBLE with padded container (e.g. 24 valid bits in 32-bit container)
    uint8_t              m_containerBytesPerSample = 0;
};
