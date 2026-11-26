#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

//==============================================================================
/**
 * The spectrum of a mono signal as numBands levels in dB, spaced evenly in pitch from minHz to
 * maxHz, with the bands' held peaks. A full-scale sine at 1 kHz reads 0 dB; the scale tilts up
 * 4.5 dB an octave, which shows most music about level. Not thread safe: one thread uses it.
 * The bands follow the newest samples pushed, so push silence once the signal stops.
 */
template <int bandCount>
class SpectrumAnalyser
{
public:
    static constexpr int numBands = bandCount;

    // Band b runs from minHz * (maxHz / minHz) ^ (b / numBands)
    static constexpr float minHz = 20.0f;
    static constexpr float maxHz = 20000.0f;
    static constexpr float floorDb = -120.0f;

    // Music's level falls with frequency, so an untilted display slopes down to the right.
    // 4.5 dB an octave is a common choice for an analyser's tilt
    static constexpr float tiltDbPerOctave = 4.5f;

    using Bands = std::array<float, numBands>;

    SpectrumAnalyser()
        : ring (maxSize, 0.0f), window (maxSize), real (maxSize), imaginary (maxSize),
          cosines (maxSize / 2), sines (maxSize / 2), reversed (maxSize)
    {
        setSampleRate (48000.0);
    }

    /** A rate that isn't above 0 is ignored. */
    void setSampleRate (double newSampleRate)
    {
        if (! (newSampleRate > 0.0) || ! std::isfinite (newSampleRate))
            return;

        // About the same length of time at any rate: 4096 samples at 44.1 and 48 kHz, which is about 90 ms,
        // short enough to follow the music and long enough to tell the low notes apart
        const auto order = newSampleRate <= 50000.0 ? 12 : newSampleRate <= 100000.0 ? 13 : maxOrder;
        size = 1 << order;

        for (int i = 0; i < size; ++i)
        {
            // A Hann window
            window[static_cast<size_t> (i)] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * i / size));

            auto bits = 0;

            for (int bit = 0; bit < order; ++bit)
                bits |= ((i >> bit) & 1) << (order - 1 - bit);

            reversed[static_cast<size_t> (i)] = bits;
        }

        for (int i = 0; i < size / 2; ++i)
        {
            cosines[static_cast<size_t> (i)] = static_cast<float> (std::cos (2.0 * std::numbers::pi * i / size));
            sines[static_cast<size_t> (i)] = static_cast<float> (std::sin (2.0 * std::numbers::pi * i / size));
        }

        const auto binsPerHz = static_cast<double> (size) / newSampleRate;
        const auto lastBin = size / 2 - 1;
        const auto ratio = static_cast<double> (maxHz / minHz);

        for (int band = 0; band < numBands; ++band)
        {
            const auto low = minHz * std::pow (ratio, static_cast<double> (band) / numBands) * binsPerHz;
            const auto high = minHz * std::pow (ratio, static_cast<double> (band + 1) / numBands) * binsPerHz;
            const auto centre = static_cast<double> (centreOf (band));

            auto& entry = layout[static_cast<size_t> (band)];
            entry.firstBin = std::clamp (static_cast<int> (std::ceil (low)), 1, lastBin);
            entry.lastBin = std::clamp (static_cast<int> (std::floor (high)), 1, lastBin);
            entry.centreBin = static_cast<float> (std::clamp (centre * binsPerHz, 1.0, static_cast<double> (lastBin)));
            entry.tiltDb = tiltDbPerOctave * static_cast<float> (std::log2 (centre / 1000.0));
            entry.aboveNyquist = low > static_cast<double> (lastBin);
        }

        std::fill (ring.begin(), ring.end(), 0.0f);
        std::fill (real.begin(), real.end(), 0.0f);
        writeIndex = 0;
        stale = false;
        silentRun = maxSize;
        bands.fill (floorDb);
        peaks.fill (floorDb);
    }

    /** Adds the signal's next samples. */
    void push (const float* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto sample = samples[i];
            const auto kept = std::isfinite (sample) ? sample : 0.0f;

            ring[static_cast<size_t> (writeIndex)] = kept;
            writeIndex = (writeIndex + 1) & (maxSize - 1);
            silentRun = std::fpclassify (kept) == FP_ZERO ? std::min (silentRun + 1, maxSize) : 0;
        }

        stale |= numSamples > 0;
    }

    /** Moves the bands on by a time, towards the newest samples' levels: they rise fast and fall slowly. */
    void update (double seconds) noexcept
    {
        // Only when there's something new to hear: an editor draws more often than most hosts send audio
        if (std::exchange (stale, false))
            analyse();

        const auto attack = static_cast<float> (1.0 - std::exp (-seconds / attackSeconds));
        const auto fall = fallDbPerSecond * static_cast<float> (seconds);

        for (size_t band = 0; band < layout.size(); ++band)
        {
            const auto& entry = layout[band];
            auto magnitude = 0.0f;

            if (entry.aboveNyquist)
                magnitude = 0.0f;
            else if (entry.lastBin >= entry.firstBin)
            {
                for (int bin = entry.firstBin; bin <= entry.lastBin; ++bin)
                    magnitude = std::max (magnitude, real[static_cast<size_t> (bin)]);
            }
            else
            {
                const auto below = static_cast<size_t> (entry.centreBin);
                const auto along = entry.centreBin - static_cast<float> (below);
                magnitude = real[below] + (real[below + 1] - real[below]) * along;
            }

            const auto target = std::max (floorDb, 20.0f * std::log10 (std::max (magnitude, 1.0e-9f /* -180 dB, to keep the log finite */)) + entry.tiltDb);
            auto& level = bands[band];

            if (target > level)
                level += (target - level) * attack;
            else
                level = std::max (target, level - fall);

            peaks[band] = std::max (peaks[band], level);
        }
    }

    /** Drops the held peaks to the current levels. */
    void resetPeaks() noexcept                  { peaks = bands; }

    const Bands& getBands() const noexcept      { return bands; }
    const Bands& getPeaks() const noexcept      { return peaks; }

    /** The frequency in the middle of a band. */
    static float centreOf (int band) noexcept
    {
        return minHz * std::pow (maxHz / minHz, (static_cast<float> (band) + 0.5f) / numBands);
    }

private:
    // 16384 samples, for the highest sample rates
    static constexpr int maxOrder = 14;
    static constexpr int maxSize = 1 << maxOrder;

    // Fast enough up to catch a hit within a frame, without the curve flickering between frames
    static constexpr double attackSeconds = 0.012;

    // Slow enough down to read
    static constexpr float fallDbPerSecond = 24.0f;

    struct Band
    {
        int firstBin = 0, lastBin = 0;   // to take the loudest of, where the band spans whole bins
        float centreBin = 0.0f;          // to read between two bins, where it doesn't
        float tiltDb = 0.0f;
        bool aboveNyquist = false;       // nothing can be there at this sample rate
    };

    // An in-place radix-2 FFT of real and imaginary
    void transform() noexcept
    {
        for (int i = 0; i < size; ++i)
        {
            const auto j = reversed[static_cast<size_t> (i)];

            if (j > i)
                std::swap (real[static_cast<size_t> (i)], real[static_cast<size_t> (j)]);
        }

        for (int length = 2; length <= size; length <<= 1)
        {
            const auto half = length / 2;
            const auto step = size / length;

            for (int start = 0; start < size; start += length)
            {
                for (int k = 0; k < half; ++k)
                {
                    const auto c = cosines[static_cast<size_t> (k * step)];
                    const auto s = -sines[static_cast<size_t> (k * step)];
                    const auto a = static_cast<size_t> (start + k);
                    const auto b = a + static_cast<size_t> (half);

                    const auto re = real[b] * c - imaginary[b] * s;
                    const auto im = real[b] * s + imaginary[b] * c;

                    real[b] = real[a] - re;
                    imaginary[b] = imaginary[a] - im;
                    real[a] += re;
                    imaginary[a] += im;
                }
            }
        }
    }

    // The magnitude of each bin of the newest samples, left in real
    void analyse() noexcept
    {
        // A window of nothing but silence needs no transform
        if (silentRun >= size)
        {
            std::fill_n (real.begin(), size / 2, 0.0f);
            return;
        }

        // The newest samples, the oldest first
        const auto first = (writeIndex - size + maxSize) & (maxSize - 1);

        for (int i = 0; i < size; ++i)
        {
            real[static_cast<size_t> (i)] = ring[static_cast<size_t> ((first + i) & (maxSize - 1))] * window[static_cast<size_t> (i)];
            imaginary[static_cast<size_t> (i)] = 0.0f;
        }

        transform();

        // Into magnitudes, where a full-scale sine is 1: 2 for the half of the spectrum that's kept,
        // and 2 more for the Hann window, which lets half the signal through
        const auto scale = 4.0f / static_cast<float> (size);

        for (int i = 0; i < size / 2; ++i)
        {
            const auto index = static_cast<size_t> (i);
            real[index] = std::hypot (real[index], imaginary[index]) * scale;
        }
    }

    int size = 1 << 12;

    std::vector<float> ring, window, real, imaginary, cosines, sines;
    std::vector<int> reversed;
    int writeIndex = 0;

    // Whether there are samples the magnitudes haven't been made from, and how many zeros the newest run of them is
    bool stale = false;
    int silentRun = maxSize;

    std::array<Band, numBands> layout;
    Bands bands, peaks;
};
