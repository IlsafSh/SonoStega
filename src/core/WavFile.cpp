#include "WavFile.h"

#include <QFile>
#include <cstring>

// ---------------------------------------------------------------------------
// Little-endian helpers (WAV/RIFF is always little-endian)
// ---------------------------------------------------------------------------
static uint16_t u16le(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t u32le(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0])        |
           (static_cast<uint32_t>(p[1]) << 8)  |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static void wu16le(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void wu32le(uint8_t *p, uint32_t v)
{
    p[0] =  v        & 0xFF;
    p[1] = (v >> 8)  & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

// ---------------------------------------------------------------------------
// WavFile::load
// ---------------------------------------------------------------------------
bool WavFile::load(const QString &path)
{
    m_valid = false;
    m_samples.clear();
    m_error.clear();
    m_containerBytesPerSample = 0;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = "Не удалось открыть файл: " + file.errorString();
        return false;
    }
    const QByteArray raw = file.readAll();
    file.close();

    const auto *buf = reinterpret_cast<const uint8_t *>(raw.constData());
    const qint64 total = raw.size();

    // ---- RIFF/WAVE signature -----------------------------------------------
    if (total < 12
        || std::memcmp(buf, "RIFF", 4) != 0
        || std::memcmp(buf + 8, "WAVE", 4) != 0)
    {
        m_error = "Файл не является корректным WAV (нет подписи RIFF/WAVE)";
        return false;
    }

    // ---- Walk chunks --------------------------------------------------------
    qint64 pos = 12;
    bool fmtFound  = false;
    bool dataFound = false;

    while (pos + 8 <= total) {
        char id[5] = {};
        std::memcpy(id, buf + pos, 4);
        const uint32_t chunkSize = u32le(buf + pos + 4);
        pos += 8;

        if (std::memcmp(id, "fmt ", 4) == 0) {
            // ---- fmt chunk --------------------------------------------------
            if (chunkSize < 16 || pos + 16 > total) {
                m_error = "Повреждён fmt-чанк";
                return false;
            }
            m_header.audioFormat   = u16le(buf + pos);
            m_header.numChannels   = u16le(buf + pos + 2);
            m_header.sampleRate    = u32le(buf + pos + 4);
            // byteRate  [8..11] — not stored, recalculated on save
            // blockAlign[12..13] — not stored, recalculated on save
            m_header.bitsPerSample = u16le(buf + pos + 14);

            if (m_header.audioFormat == 0xFFFE) {
                // WAVE_FORMAT_EXTENSIBLE: extended fmt chunk with SubFormat GUID
                // Layout after standard 16 bytes: cbSize(2) + wValidBitsPerSample(2) +
                //   dwChannelMask(4) + SubFormat GUID(16)  → needs at least 40 bytes total
                if (chunkSize < 40 || pos + 40 > total) {
                    m_error = "Повреждён fmt-чанк WAVE_FORMAT_EXTENSIBLE";
                    return false;
                }
                // PCM SubFormat GUID: {00000001-0000-0010-8000-00aa00389b71}
                static const uint8_t kPcmGuid[16] = {
                    0x01,0x00,0x00,0x00, 0x00,0x00,0x10,0x00,
                    0x80,0x00,0x00,0xAA, 0x00,0x38,0x9B,0x71
                };
                if (std::memcmp(buf + pos + 24, kPcmGuid, 16) != 0) {
                    m_error = "WAVE_FORMAT_EXTENSIBLE: поддерживается только SubFormat PCM";
                    return false;
                }
                uint16_t validBits = u16le(buf + pos + 18);
                if (validBits != 0 && validBits != m_header.bitsPerSample) {
                    // Container is wider than valid data (e.g. 24 valid bits in 32-bit container)
                    m_containerBytesPerSample = static_cast<uint8_t>(m_header.bitsPerSample / 8);
                    m_header.bitsPerSample = validBits;
                }
                m_header.audioFormat = 1;  // treat as plain PCM from here on
            } else if (m_header.audioFormat != 1) {
                m_error = QString("Поддерживается только PCM (audioFormat=1), получено: %1")
                              .arg(m_header.audioFormat);
                return false;
            }

            if (m_header.bitsPerSample != 8
                && m_header.bitsPerSample != 16
                && m_header.bitsPerSample != 24)
            {
                m_error = QString("Неподдерживаемая битность: %1 бит (ожидается 8, 16 или 24)")
                              .arg(m_header.bitsPerSample);
                return false;
            }
            if (m_header.numChannels == 0) {
                m_error = "Недопустимое число каналов: 0";
                return false;
            }
            fmtFound = true;

        } else if (std::memcmp(id, "data", 4) == 0) {
            // ---- data chunk -------------------------------------------------
            if (!fmtFound) {
                m_error = "data-чанк обнаружен раньше fmt-чанка";
                return false;
            }
            const uint32_t available = static_cast<uint32_t>(
                std::min<qint64>(chunkSize, total - pos));

            if (!parseSamples(buf + pos, available))
                return false;

            dataFound = true;
            break; // We have everything we need
        }
        // Unknown chunk — skip (pad to even boundary per RIFF spec)
        pos += chunkSize;
        if (chunkSize & 1) ++pos;
    }

    if (!fmtFound || !dataFound) {
        m_error = "Не найден один из обязательных чанков (fmt или data)";
        return false;
    }

    m_valid = true;
    return true;
}

// ---------------------------------------------------------------------------
// WavFile::parseSamples  (called from load)
// ---------------------------------------------------------------------------
bool WavFile::parseSamples(const uint8_t *data, uint32_t size)
{
    const int bps = m_header.bitsPerSample;   // valid bits per sample
    // Container bytes: may be wider for WAVE_FORMAT_EXTENSIBLE (e.g. 24-bit in 32-bit slot)
    const int cbs = m_containerBytesPerSample > 0 ? m_containerBytesPerSample : (bps / 8);
    const uint32_t n = size / cbs;

    m_header.numSamples = n;
    m_samples.resize(n);

    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t *p = data + static_cast<size_t>(i) * cbs;
        int32_t v = 0;

        if (cbs == 4) {
            // 32-bit container, left-justified: valid bits occupy the top bps bits
            int32_t raw = static_cast<int32_t>(u32le(p));
            v = raw >> (32 - bps);  // arithmetic shift preserves sign
        } else {
            switch (bps) {
            case 8:
                v = static_cast<int32_t>(p[0]);
                break;
            case 16:
                v = static_cast<int32_t>(static_cast<int16_t>(u16le(p)));
                break;
            case 24:
                v = static_cast<int32_t>(p[0])
                  | (static_cast<int32_t>(p[1]) << 8)
                  | (static_cast<int32_t>(p[2]) << 16);
                if (v & 0x800000) v |= 0xFF000000;
                break;
            }
        }
        m_samples[i] = v;
    }
    return true;
}

// ---------------------------------------------------------------------------
// WavFile::save
// ---------------------------------------------------------------------------
bool WavFile::save(const QString &path) const
{
    if (!m_valid) return false;

    const uint16_t bps  = m_header.bitsPerSample;
    const int      bpsB = bps / 8;
    const uint32_t dataSize = static_cast<uint32_t>(m_samples.size()) * bpsB;

    // Standard 44-byte PCM WAV header
    // RIFF(12) + fmt(24) + data(8) = 44
    uint8_t hdr[44] = {};
    std::memcpy(hdr,      "RIFF", 4);
    wu32le(hdr + 4,   36 + dataSize);          // file size - 8
    std::memcpy(hdr + 8,  "WAVE", 4);
    std::memcpy(hdr + 12, "fmt ", 4);
    wu32le(hdr + 16,  16);                     // fmt chunk size (PCM = 16)
    wu16le(hdr + 20,  1);                      // audioFormat = PCM
    wu16le(hdr + 22,  m_header.numChannels);
    wu32le(hdr + 24,  m_header.sampleRate);
    wu32le(hdr + 28,  m_header.sampleRate * m_header.numChannels * bpsB); // byteRate
    wu16le(hdr + 32,  m_header.numChannels * bpsB);                        // blockAlign
    wu16le(hdr + 34,  bps);
    std::memcpy(hdr + 36, "data", 4);
    wu32le(hdr + 40,  dataSize);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;

    file.write(reinterpret_cast<const char *>(hdr), 44);

    // Encode all samples into a contiguous buffer, then write at once
    std::vector<uint8_t> buf(m_samples.size() * bpsB);
    uint8_t *p = buf.data();

    for (int32_t v : m_samples) {
        switch (bps) {
        case 8:
            *p++ = static_cast<uint8_t>(v & 0xFF);
            break;
        case 16:
            *p++ =  v        & 0xFF;
            *p++ = (v >> 8)  & 0xFF;
            break;
        case 24:
            *p++ =  v        & 0xFF;
            *p++ = (v >> 8)  & 0xFF;
            *p++ = (v >> 16) & 0xFF;
            break;
        }
    }
    file.write(reinterpret_cast<const char *>(buf.data()),
               static_cast<qint64>(buf.size()));
    file.close();
    return true;
}

// ---------------------------------------------------------------------------
// Informational helpers
// ---------------------------------------------------------------------------
uint64_t WavFile::capacityBytes(int nBits) const
{
    if (!m_valid || nBits <= 0) return 0;
    // 32 bits are used to embed the payload length prefix
    const uint64_t totalBits = static_cast<uint64_t>(m_samples.size()) * nBits;
    if (totalBits <= 32) return 0;
    return (totalBits - 32) / 8;
}

double WavFile::durationSeconds() const
{
    if (!m_valid || m_header.sampleRate == 0 || m_header.numChannels == 0)
        return 0.0;
    return static_cast<double>(m_header.numSamples)
         / (m_header.sampleRate * m_header.numChannels);
}

QString WavFile::formatDescription() const
{
    if (!m_valid) return "—";
    const QString ch = (m_header.numChannels == 1) ? "моно"
                     : (m_header.numChannels == 2) ? "стерео"
                     : QString("%1 кан.").arg(m_header.numChannels);
    return QString("%1 Гц, %2, %3-бит PCM")
               .arg(m_header.sampleRate)
               .arg(ch)
               .arg(m_header.bitsPerSample);
}
