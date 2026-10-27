// Standalone test for DSP.h: sibilance should be reduced, low-band vocal content should not.
#include "../Source/DSP.h"
#include <cstdio>
#include <random>
#include <vector>

static double rms (const std::vector<float>& v, size_t from)
{
    double s = 0; size_t n = 0;
    for (size_t i = from; i < v.size(); ++i) { s += v[i] * (double) v[i]; ++n; }
    return std::sqrt (s / n);
}

int main()
{
    const double fs = 48000.0;
    const int N = 48000;
    std::mt19937 rng (1);
    std::normal_distribution<double> nd (0.0, 1.0);

    // "voice": 220 Hz tone. "ess": loud 5-9 kHz noise burst in the second half.
    vc::Biquad hp; hp.set (vc::coeffs::bandPass (fs, 7000.0, 1.2)); // realistic sibilance band
    std::vector<float> voice (N), ess (N), mix (N);
    for (int i = 0; i < N; ++i)
    {
        voice[i] = (float) (0.2 * std::sin (2 * vc::kPi * 220.0 * i / fs));
        ess[i] = i > N / 2 ? (float) (0.9 * hp.process (nd (rng))) : 0.0f;
        mix[i] = voice[i] + ess[i];
    }

    // 1. De-esser: sibilant half should drop, tone-only half should be untouched.
    {
        vc::DeEsser d; d.prepare (fs); d.setFrequency (6500.0);
        d.setParams (-34.0, 10.0, false, false);
        std::vector<float> out = mix;
        float* ch[1] = { out.data() };
        double gr = d.process (ch, 1, N);
        std::vector<float> a (out.begin() + 4000, out.begin() + N / 2 - 100);
        std::vector<float> b (mix.begin() + 4000, mix.begin() + N / 2 - 100);
        // measure only the sibilant band (>4 kHz) in the burst section
        auto band = [&] (const std::vector<float>& v)
        {
            vc::Biquad h1, h2; h1.set (vc::coeffs::highPass (fs, 4000.0, 0.7)); h2 = h1;
            std::vector<float> o (v.size());
            for (size_t i = 0; i < v.size(); ++i) o[i] = (float) h2.process (h1.process (v[i]));
            return o;
        };
        auto bo = band (out), bm = band (mix);
        std::printf ("DeEsser maxGR %.2f dB | voice-only change %.3f dB | >4kHz band change in burst %.2f dB\n",
                     gr, vc::linToDb (rms (a, 0) / rms (b, 0)), vc::linToDb (rms (bo, N / 2 + 4000) / rms (bm, N / 2 + 4000)));
    }

    // 2. Compressor: loud signal reduced, then level behaviour sane, no NaN.
    {
        vc::Compressor comp; comp.prepare (fs); comp.setAmount (0.6);
        std::vector<float> loud (N);
        for (int i = 0; i < N; ++i) loud[i] = (float) (0.8 * std::sin (2 * vc::kPi * 220.0 * i / fs));
        float* ch[1] = { loud.data() };
        double gr = comp.process (ch, 1, N);
        std::printf ("Compressor maxGR %.2f dB | out rms %.3f (in %.3f)\n", gr,
                     rms (loud, N / 2), 0.8 / std::sqrt (2.0));
    }

    // 3. EQ sanity: 3.2 kHz +6 dB bell on a 3.2 kHz sine.
    {
        vc::Biquad b; b.set (vc::coeffs::peak (fs, 3200.0, 0.7, 6.0));
        std::vector<float> s (N);
        for (int i = 0; i < N; ++i) s[i] = (float) b.process (std::sin (2 * vc::kPi * 3200.0 * i / fs));
        std::printf ("Peak +6dB @3.2k measured gain %.2f dB\n", vc::linToDb (rms (s, N / 2) * std::sqrt (2.0)));

        vc::Biquad hs; hs.set (vc::coeffs::highShelf (fs, 11000.0, 6.0));
        for (int i = 0; i < N; ++i) s[i] = (float) hs.process (std::sin (2 * vc::kPi * 18000.0 * i / fs));
        std::printf ("HighShelf +6dB @11k measured gain at 18k %.2f dB\n", vc::linToDb (rms (s, N / 2) * std::sqrt (2.0)));

        vc::Biquad ls; ls.set (vc::coeffs::lowShelf (fs, 180.0, 6.0));
        for (int i = 0; i < N; ++i) s[i] = (float) ls.process (std::sin (2 * vc::kPi * 40.0 * i / fs));
        std::printf ("LowShelf +6dB @180 measured gain at 40Hz %.2f dB\n", vc::linToDb (rms (s, N / 2) * std::sqrt (2.0)));
    }
    return 0;
}
