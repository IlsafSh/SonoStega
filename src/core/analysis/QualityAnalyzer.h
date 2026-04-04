#pragma once

#include "core/io/WavFile.h"
#include <QString>

// Computes audio quality metrics between an original and a stego WAV file
//
// MSE  = (1/N) * sum((x_i - x̃_i)^2)
// NMSE = MSE / ((1/N) * sum(x_i^2))
//
// Both files must have the same format (channels, sample rate, bit depth, length)
class QualityAnalyzer
{
public:
    struct Metrics {
        double mse = 0.0;
        double nmse = 0.0;
        bool valid = false;
        QString error;
    };

    static Metrics compute(const WavFile &original, const WavFile &stego);
};
