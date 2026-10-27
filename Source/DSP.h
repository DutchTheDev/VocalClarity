#pragma once
// Header-only DSP for Vocal Clarity (no JUCE dependency, so it can be unit tested standalone).
#include <algorithm>
#include <cmath>

namespace vc
{
constexpr double kPi = 3.14159265358979323846;

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double lin) { return 20.0 * std::log10 (std::max (lin, 1.0e-9)); }

// ---------------------------------------------------------------- Biquad (RBJ cookbook)
struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

namespace coeffs
{
    inline BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    inline BiquadCoeffs highPass (double fs, double f, double q)
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    inline BiquadCoeffs bandPass (double fs, double f, double q) // constant 0 dB peak gain
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }

    inline BiquadCoeffs peak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        return normalise (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    inline BiquadCoeffs lowShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / std::sqrt (2.0), t = 2.0 * std::sqrt (A) * al;
        return normalise (A * ((A + 1) - (A - 1) * c + t),
                          2 * A * ((A - 1) - (A + 1) * c),
                          A * ((A + 1) - (A - 1) * c - t),
                          (A + 1) + (A - 1) * c + t,
                          -2 * ((A - 1) + (A + 1) * c),
                          (A + 1) + (A - 1) * c - t);
    }

    inline BiquadCoeffs highShelf (double fs, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / std::sqrt (2.0), t = 2.0 * std::sqrt (A) * al;
        return normalise (A * ((A + 1) + (A - 1) * c + t),
                          -2 * A * ((A - 1) + (A + 1) * c),
                          A * ((A + 1) + (A - 1) * c - t),
                          (A + 1) - (A - 1) * c + t,
                          2 * ((A - 1) - (A + 1) * c),
                          (A + 1) - (A - 1) * c - t);
    }
}

class Biquad
{
public:
    void reset() { z1 = z2 = 0.0; }
    void set (const BiquadCoeffs& c) { k = c; }
    inline double process (double x)
    {
        const double y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2;
        z2 = k.b2 * x - k.a2 * y;
        return y;
    }

private:
    BiquadCoeffs k;
    double z1 = 0.0, z2 = 0.0;
};

// ---------------------------------------------------------------- Smooth vocal compressor
// Stereo-linked, feed-forward, soft knee, gain reduction smoothed in the dB domain.
class Compressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        grDb = 0.0;
        setTimes (12.0, 140.0);
    }

    void setTimes (double attackMs, double releaseMs)
    {
        atk = std::exp (-1.0 / (0.001 * attackMs * fs));
        rel = std::exp (-1.0 / (0.001 * releaseMs * fs));
    }

    // amount 0..1 : 0 = bypass, 1 = firm leveling
    void setAmount (double amount)
    {
        amt = std::clamp (amount, 0.0, 1.0);
        thresholdDb = -8.0 - 22.0 * amt;
        ratio = 1.5 + 2.5 * amt;
        makeupDb = -thresholdDb * (1.0 - 1.0 / ratio) * 0.25;
    }

    // data: array of channel pointers. Returns max gain reduction (dB) seen in the block.
    template <typename Sample>
    double process (Sample* const* data, int numCh, int numSamples)
    {
        double maxGr = 0.0;
        if (amt < 0.001)
        {
            grDb = 0.0;
            return 0.0;
        }
        const double knee = 6.0;
        for (int i = 0; i < numSamples; ++i)
        {
            double peak = 0.0;
            for (int c = 0; c < numCh; ++c)
                peak = std::max (peak, std::abs ((double) data[c][i]));

            const double lvl = linToDb (peak);
            const double over = lvl - thresholdDb;
            double target;
            if (2.0 * over < -knee)
                target = 0.0;
            else if (2.0 * std::abs (over) <= knee)
            {
                const double x = over + knee / 2.0;
                target = (1.0 - 1.0 / ratio) * x * x / (2.0 * knee);
            }
            else
                target = over * (1.0 - 1.0 / ratio);

            grDb = target > grDb ? atk * grDb + (1.0 - atk) * target
                                 : rel * grDb + (1.0 - rel) * target;
            maxGr = std::max (maxGr, grDb);

            const double g = dbToLin (makeupDb - grDb);
            for (int c = 0; c < numCh; ++c)
                data[c][i] = (Sample) (data[c][i] * g);
        }
        return maxGr;
    }

private:
    double fs = 44100.0, atk = 0.0, rel = 0.0, grDb = 0.0;
    double amt = 0.0, thresholdDb = -20.0, ratio = 2.0, makeupDb = 0.0;
};

// ---------------------------------------------------------------- De-esser
// Band-pass sidechain detector. Split-band mode cuts only the sibilant band with a dynamic
// bell filter; wide-band mode ducks the whole signal. Stereo-linked.
class DeEsser
{
public:
    static constexpr int kMaxCh = 2;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& b : detector) b.reset();
        for (auto& b : bell) b.reset();
        env = 0.0;
        coeffCounter = 0;
        atk = std::exp (-1.0 / (0.0005 * fs)); // 0.5 ms
        rel = std::exp (-1.0 / (0.050 * fs));  // 50 ms
        setFrequency (6500.0);
    }

    void setFrequency (double hz)
    {
        freq = std::clamp (hz, 2000.0, 0.45 * fs);
        const auto d = coeffs::bandPass (fs, freq, 1.4);
        for (auto& b : detector) b.set (d);
    }

    void setParams (double thresholdDb_, double rangeDb_, bool wideBand_, bool listen_)
    {
        thresholdDb = thresholdDb_;
        rangeDb = rangeDb_;
        wide = wideBand_;
        listen = listen_;
    }

    template <typename Sample>
    double process (Sample* const* data, int numCh, int numSamples)
    {
        numCh = std::min (numCh, kMaxCh);
        const double ratio = 5.0;
        double maxGr = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            double level = 0.0;
            double band[kMaxCh] = { 0.0, 0.0 };
            for (int c = 0; c < numCh; ++c)
            {
                band[c] = detector[c].process ((double) data[c][i]);
                level = std::max (level, std::abs (band[c]));
            }

            env = level > env ? atk * env + (1.0 - atk) * level
                              : rel * env + (1.0 - rel) * level;

            const double over = linToDb (env) - thresholdDb;
            double gr = over > 0.0 ? over * (1.0 - 1.0 / ratio) : 0.0;
            gr = std::min (gr, rangeDb);
            maxGr = std::max (maxGr, gr);

            if (listen)
            {
                for (int c = 0; c < numCh; ++c)
                    data[c][i] = (Sample) band[c];
                continue;
            }

            if (wide)
            {
                const double g = dbToLin (-gr);
                for (int c = 0; c < numCh; ++c)
                    data[c][i] = (Sample) (data[c][i] * g);
            }
            else
            {
                if (coeffCounter-- <= 0)
                {
                    coeffCounter = 7;
                    const auto k = coeffs::peak (fs, freq, 1.1, -gr);
                    for (auto& b : bell) b.set (k);
                }
                for (int c = 0; c < numCh; ++c)
                    data[c][i] = (Sample) bell[c].process ((double) data[c][i]);
            }
        }
        return maxGr;
    }

private:
    double fs = 44100.0, freq = 6500.0, env = 0.0, atk = 0.0, rel = 0.0;
    double thresholdDb = -30.0, rangeDb = 8.0;
    bool wide = false, listen = false;
    int coeffCounter = 0;
    Biquad detector[kMaxCh], bell[kMaxCh];
};
} // namespace vc
