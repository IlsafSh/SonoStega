#include "LsbEmbedder.h"

#include <algorithm>
#include <numeric>
#include <random>
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
// buildStream — shared validation + stream assembly
// ---------------------------------------------------------------------------
QByteArray LsbEmbedder::buildStream(const QByteArray &payload, int nBits, size_t totalSamples, QString &errOut)
{
    const uint32_t payloadLen = static_cast<uint32_t>(payload.size());
    const size_t totalBits = (static_cast<size_t>(payloadLen) + 4) * 8;
    const size_t samplesNeeded = (totalBits + nBits - 1) / nBits;

    if (samplesNeeded > totalSamples) {
        errOut = QString("Контейнер слишком мал: требуется %1 сэмплов, доступно %2")
                     .arg(samplesNeeded).arg(totalSamples);
        return {};
    }

    QByteArray stream(4, '\0');
    stream[0] = static_cast<char>( payloadLen        & 0xFF);
    stream[1] = static_cast<char>((payloadLen >>  8) & 0xFF);
    stream[2] = static_cast<char>((payloadLen >> 16) & 0xFF);
    stream[3] = static_cast<char>((payloadLen >> 24) & 0xFF);
    stream.append(payload);
    return stream;
}

// ---------------------------------------------------------------------------
// embedCore — write stream bits into sample LSBs
// ---------------------------------------------------------------------------
void LsbEmbedder::embedCore(std::vector<int32_t> &samples, const uint8_t *stream, size_t totalBits, int nBits, const std::vector<uint32_t> *indices)
{
    const int32_t mask = (1 << nBits) - 1;
    const size_t  slotsNeeded = (totalBits + nBits - 1) / nBits;
    size_t bitPos = 0;

    for (size_t slot = 0; slot < slotsNeeded; ++slot) {
        const size_t si = indices ? static_cast<size_t>((*indices)[slot]) : slot;

        int32_t chunk = 0;
        for (int b = nBits - 1; b >= 0 && bitPos < totalBits; --b, ++bitPos)
            chunk |= (getBit(stream, bitPos) << b);

        samples[si] = (samples[si] & ~mask) | (chunk & mask);
    }
}

// ---------------------------------------------------------------------------
// extractCore — read sample LSBs into output bytes
// ---------------------------------------------------------------------------
size_t LsbEmbedder::extractCore(const std::vector<int32_t>  &samples, uint8_t *outData, size_t startSlot, size_t bitCount, int nBits, const std::vector<uint32_t> *indices)
{
    const int32_t mask = (1 << nBits) - 1;
    size_t bitPos = 0;
    size_t slot = startSlot;

    while (bitPos < bitCount) {
        const size_t  si    = indices ? static_cast<size_t>((*indices)[slot]) : slot;
        const int32_t chunk = samples[si] & mask;
        ++slot;

        for (int b = nBits - 1; b >= 0 && bitPos < bitCount; --b, ++bitPos)
            setBit(outData, bitPos, (chunk >> b) & 1);
    }
    return slot;
}

// ---------------------------------------------------------------------------
// Shared extract logic (used by both modes)
// ---------------------------------------------------------------------------
LsbEmbedder::Result LsbEmbedder::doExtract(const WavFile &wav, QByteArray &outPayload, int nBits, const std::vector<uint32_t> *indices)
{
    const auto &samples = wav.samples();
    const size_t headerSlots  = (32 + nBits - 1) / nBits;
    const size_t totalSlots   = indices ? indices->size() : samples.size();

    if (totalSlots < headerSlots)
        return {false, "WAV-файл слишком короткий для чтения заголовка"};

    // Step 1: read 32-bit payload length
    uint8_t lenBytes[4] = {};
    const size_t nextSlot = LsbEmbedder::extractCore(samples, lenBytes, 0, 32, nBits, indices);

    const uint32_t payloadLen =
          static_cast<uint32_t>(lenBytes[0])
        | (static_cast<uint32_t>(lenBytes[1]) << 8)
        | (static_cast<uint32_t>(lenBytes[2]) << 16)
        | (static_cast<uint32_t>(lenBytes[3]) << 24);

    // Sanity: decoded length must fit within the container capacity
    const uint64_t maxPayload = wav.capacityBytes(nBits);
    if (payloadLen > maxPayload) {
        return {false,
                QString("Некорректная длина сообщения (%1 байт). Файл не содержит внедрённых данных, неверный пароль или параметр nBits")
                    .arg(payloadLen)};
    }

    if (payloadLen == 0) {
        outPayload.clear();
        return {true, {}};
    }

    // Step 2: read payload
    const size_t payloadBits  = static_cast<size_t>(payloadLen) * 8;
    const size_t payloadSlots = (payloadBits + nBits - 1) / nBits;

    if (nextSlot + payloadSlots > totalSlots)
        return {false, "WAV-файл слишком короткий для сообщения заявленной длины"};

    outPayload.resize(static_cast<int>(payloadLen));
    LsbEmbedder::extractCore(samples, reinterpret_cast<uint8_t *>(outPayload.data()), nextSlot, payloadBits, nBits, indices);
    return {true, {}};
}

// ---------------------------------------------------------------------------
// Sequential mode
// ---------------------------------------------------------------------------
LsbEmbedder::Result LsbEmbedder::embed(WavFile &wav, const QByteArray &payload, int nBits)
{
    if (!wav.isValid())         return {false, "WAV-файл не загружен или повреждён"};
    if (nBits < 1 || nBits > 8) return {false, "nBits должен быть в диапазоне 1–8"};

    auto &samples = wav.samples();
    QString err;
    const QByteArray stream = buildStream(payload, nBits, samples.size(), err);
    if (stream.isEmpty()) return {false, err};

    embedCore(samples, reinterpret_cast<const uint8_t *>(stream.constData()), static_cast<size_t>(stream.size()) * 8, nBits, nullptr);
    return {true, {}};
}

LsbEmbedder::Result LsbEmbedder::extract(const WavFile &wav, QByteArray &outPayload, int nBits)
{
    if (!wav.isValid())         return {false, "WAV-файл не загружен или повреждён"};
    if (nBits < 1 || nBits > 8) return {false, "nBits должен быть в диапазоне 1–8"};

    return doExtract(wav, outPayload, nBits, nullptr);
}

// ---------------------------------------------------------------------------
// Keyed mode helpers
// ---------------------------------------------------------------------------

// FNV-1a 64-bit hash of the password UTF-8 bytes
uint64_t LsbEmbedder::passwordSeed(const QString &password)
{
    const QByteArray utf8 = password.toUtf8();
    constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME  =  1099511628211ULL;
    uint64_t hash = FNV_OFFSET;
    for (unsigned char c : utf8) {
        hash ^= c;
        hash *= FNV_PRIME;
    }
    return hash;
}

// Full Fisher-Yates shuffle of [0..n) seeded deterministically
std::vector<uint32_t> LsbEmbedder::keyedIndices(size_t n, uint64_t seed)
{
    std::vector<uint32_t> idx(n);
    std::iota(idx.begin(), idx.end(), 0u);

    std::mt19937_64 rng(seed);
    for (size_t i = n - 1; i > 0; --i) {
        std::uniform_int_distribution<size_t> dist(0, i);
        std::swap(idx[i], idx[dist(rng)]);
    }
    return idx;
}

// ---------------------------------------------------------------------------
// Keyed mode
// ---------------------------------------------------------------------------
LsbEmbedder::Result LsbEmbedder::embedKeyed(WavFile &wav, const QByteArray &payload, int nBits, const QString &password)
{
    if (!wav.isValid())         return {false, "WAV-файл не загружен или повреждён"};
    if (nBits < 1 || nBits > 8) return {false, "nBits должен быть в диапазоне 1–8"};
    if (password.isEmpty())     return {false, "Пароль не может быть пустым"};

    auto &samples = wav.samples();
    QString err;
    const QByteArray stream = buildStream(payload, nBits, samples.size(), err);
    if (stream.isEmpty()) return {false, err};

    const auto indices = keyedIndices(samples.size(), passwordSeed(password));

    embedCore(samples,
              reinterpret_cast<const uint8_t *>(stream.constData()),
              static_cast<size_t>(stream.size()) * 8,
              nBits, &indices);
    return {true, {}};
}

LsbEmbedder::Result LsbEmbedder::extractKeyed(const WavFile &wav, QByteArray &outPayload, int nBits, const QString &password)
{
    if (!wav.isValid())         return {false, "WAV-файл не загружен или повреждён"};
    if (nBits < 1 || nBits > 8) return {false, "nBits должен быть в диапазоне 1–8"};
    if (password.isEmpty())     return {false, "Пароль не может быть пустым"};

    // Generate the full permutation — same seed = same order as on embed
    const auto indices = keyedIndices(wav.samples().size(), passwordSeed(password));

    return doExtract(wav, outPayload, nBits, &indices);
}
