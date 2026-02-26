#include "LsbEmbedder.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Bit helpers  (MSB-first within each byte, pos=0 → bit 7 of byte 0)
// ---------------------------------------------------------------------------
int LsbEmbedder::getBit(const uint8_t *data, size_t pos)
{
    return (data[pos / 8] >> (7 - pos % 8)) & 1;
}

void LsbEmbedder::setBit(uint8_t *data, size_t pos, int bit)
{
    const size_t byte  = pos / 8;
    const int    shift = 7 - static_cast<int>(pos % 8);
    if (bit) data[byte] |=  (1 << shift);
    else     data[byte] &= ~(1 << shift);
}

// ---------------------------------------------------------------------------
// embed  — sequential mode
// ---------------------------------------------------------------------------
LsbEmbedder::Result LsbEmbedder::embed(WavFile &wav, const QByteArray &payload, int nBits)
{
    if (!wav.isValid())
        return {false, "WAV-файл не загружен или повреждён"};
    if (nBits < 1 || nBits > 8)
        return {false, "nBits должен быть в диапазоне 1–8"};

    auto &samples = wav.samples();

    // --- Build the full bitstream: 4-byte LE length prefix + payload ---------
    const uint32_t payloadLen = static_cast<uint32_t>(payload.size());

    QByteArray stream(4, '\0');
    stream[0] = static_cast<char>( payloadLen        & 0xFF);
    stream[1] = static_cast<char>((payloadLen >>  8) & 0xFF);
    stream[2] = static_cast<char>((payloadLen >> 16) & 0xFF);
    stream[3] = static_cast<char>((payloadLen >> 24) & 0xFF);
    stream.append(payload);

    const size_t totalBits = static_cast<size_t>(stream.size()) * 8;
    const size_t samplesNeeded = (totalBits + nBits - 1) / nBits;

    if (samplesNeeded > samples.size()) {
        return {false,
                QString("Контейнер слишком мал: требуется %1 сэмплов, доступно %2")
                    .arg(samplesNeeded)
                    .arg(samples.size())};
    }

    const int32_t mask = (1 << nBits) - 1;
    const auto *data = reinterpret_cast<const uint8_t *>(stream.constData());
    size_t bitPos = 0;

    // For each sample, pack nBits consecutive stream bits into its LSBs
    // Bit order within the chunk: stream[bitPos] → bit (nBits-1), ..., stream[bitPos+nBits-1] → bit 0 (LSB)
    for (size_t si = 0; si < samplesNeeded; ++si) {
        int32_t chunk = 0;
        for (int b = nBits - 1; b >= 0 && bitPos < totalBits; --b, ++bitPos)
            chunk |= (getBit(data, bitPos) << b);

        samples[si] = (samples[si] & ~mask) | (chunk & mask);
    }

    return {true, {}};
}

// ---------------------------------------------------------------------------
// extract  — sequential mode
// ---------------------------------------------------------------------------
LsbEmbedder::Result LsbEmbedder::extract(const WavFile &wav, QByteArray &outPayload, int nBits)
{
    if (!wav.isValid())
        return {false, "WAV-файл не загружен или повреждён"};
    if (nBits < 1 || nBits > 8)
        return {false, "nBits должен быть в диапазоне 1–8"};

    const auto &samples = wav.samples();
    const int32_t mask  = (1 << nBits) - 1;

    // Extracts `bitCount` bits from samples starting at `sampleStart`, writes them MSB-first into outData
    // Returns the index of the next unused sample
    auto extractBits = [&](size_t sampleStart, size_t bitCount, uint8_t *outData) -> size_t
    {
        size_t bitPos = 0;
        size_t si     = sampleStart;
        while (bitPos < bitCount) {
            const int32_t chunk = samples[si++] & mask;
            for (int b = nBits - 1; b >= 0 && bitPos < bitCount; --b, ++bitPos)
                setBit(outData, bitPos, (chunk >> b) & 1);
        }
        return si;
    };

    // --- Step 1: read 32-bit payload length from the first samples -----------
    const size_t headerSamples = (32 + nBits - 1) / nBits;
    if (samples.size() < headerSamples)
        return {false, "WAV-файл слишком короткий для чтения заголовка"};

    uint8_t lenBytes[4] = {};
    const size_t nextSample = extractBits(0, 32, lenBytes);

    const uint32_t payloadLen =
           static_cast<uint32_t>(lenBytes[0])
        | (static_cast<uint32_t>(lenBytes[1]) << 8)
        | (static_cast<uint32_t>(lenBytes[2]) << 16)
        | (static_cast<uint32_t>(lenBytes[3]) << 24);

    // Sanity: length must not exceed theoretical capacity
    const uint64_t maxPayload = wav.capacityBytes(nBits);
    if (payloadLen > maxPayload) {
        return {false,
                QString("Некорректная длина сообщения (%1 байт). "
                        "Файл не содержит внедрённых данных "
                        "или использован другой параметр nBits.")
                    .arg(payloadLen)};
    }

    if (payloadLen == 0) {
        outPayload.clear();
        return {true, {}};
    }

    // --- Step 2: read payload ------------------------------------------------
    const size_t payloadBits    = static_cast<size_t>(payloadLen) * 8;
    const size_t payloadSamples = (payloadBits + nBits - 1) / nBits;

    if (nextSample + payloadSamples > samples.size())
        return {false, "WAV-файл слишком короткий для сообщения заявленной длины"};

    outPayload.resize(static_cast<int>(payloadLen));
    extractBits(nextSample, payloadBits, reinterpret_cast<uint8_t *>(outPayload.data()));

    return {true, {}};
}
