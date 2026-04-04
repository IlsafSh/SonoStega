#include "core/analysis/QualityAnalyzer.h"

QualityAnalyzer::Metrics QualityAnalyzer::compute(const WavFile &original, const WavFile &stego)
{
    Metrics m;

    if (!original.isValid() || !stego.isValid()) {
        m.error = "Один или оба файла не загружены";
        return m;
    }

    const auto &orig = original.samples();
    const auto &steg = stego.samples();

    if (orig.size() != steg.size()) {
        m.error = QString("Файлы имеют разное количество сэмплов (%1 и %2)")
                      .arg(orig.size()).arg(steg.size());
        return m;
    }

    if (orig.empty()) {
        m.error = "Файлы не содержат сэмплов";
        return m;
    }

    const auto &oh = original.header();
    const auto &sh = stego.header();

    if (oh.sampleRate != sh.sampleRate || oh.numChannels != sh.numChannels
        || oh.bitsPerSample != sh.bitsPerSample)
    {
        m.error = "Файлы имеют разный формат (частота, каналы или битность)";
        return m;
    }

    // Accumulate in double to avoid overflow on large files
    double sumSqDiff = 0.0;
    double sumSqOrig = 0.0;

    for (size_t i = 0; i < orig.size(); ++i) {
        double diff = static_cast<double>(orig[i]) - static_cast<double>(steg[i]);
        sumSqDiff += diff * diff;
        sumSqOrig += static_cast<double>(orig[i]) * static_cast<double>(orig[i]);
    }

    const double n = static_cast<double>(orig.size());
    m.mse = sumSqDiff / n;

    // NMSE is undefined if the original signal is silent (all zeros)
    if (sumSqOrig < 1e-12) {
        m.nmse = 0.0;
        if (sumSqDiff > 0.0)
            m.error = "Предупреждение: оригинальный сигнал нулевой, NMSE не определён";
    } else {
        m.nmse = m.mse / (sumSqOrig / n);
    }

    m.valid = true;
    return m;
}
