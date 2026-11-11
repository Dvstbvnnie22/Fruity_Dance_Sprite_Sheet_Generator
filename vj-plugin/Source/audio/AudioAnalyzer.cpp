#include "AudioAnalyzer.h"

namespace moshdeck
{
AudioAnalyzer::AudioAnalyzer() {}

void AudioAnalyzer::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    ring.fill (0.0f);
    prevMag.fill (0.0f);
    ringPos = samplesSinceHop = 0;
    work = {};
    work.sampleRate = sr;
}

void AudioAnalyzer::push (const float* const* channels, int numChannels, int numSamples)
{
    if (numChannels <= 0)
        return;

    const float g = inputGain.load();
    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < numChannels; ++c)
            s += channels[c][i];
        s = s * g / (float) numChannels;

        ring[(size_t) ringPos] = s;
        ringPos = (ringPos + 1) & (fftSize - 1);

        if (++samplesSinceHop >= hopSize)
        {
            samplesSinceHop = 0;
            processFrame();
        }
    }
    for (auto& o : onsets)
        o.samplesSinceHit += numSamples;
}

void AudioAnalyzer::processFrame()
{
    // ---- unwrap ring into FFT buffer + waveform
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) & (fftSize - 1))];

    for (int i = 0; i < AudioFeatures::numWave; ++i)
        work.wave[(size_t) i] = juce::jlimit (-1.0f, 1.0f, fftData[(size_t) (fftSize - AudioFeatures::numWave + i)]);

    double rms = 0.0;
    for (int i = fftSize - 1024; i < fftSize; ++i)
        rms += fftData[(size_t) i] * fftData[(size_t) i];
    const float levelNow = juce::jlimit (0.0f, 1.0f, (float) std::sqrt (rms / 1024.0) * 2.5f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float smooth = juce::jlimit (0.0f, 0.98f, smoothing.load());
    const int half = fftSize / 2;
    const float binHz = (float) (sr / fftSize);

    // ---- magnitudes in dB normalised 0..1
    float flux[3] = { 0, 0, 0 };
    for (int i = 0; i < half; ++i)
    {
        const float mag = fftData[(size_t) i] / (float) fftSize * 4.0f;
        const float db  = juce::Decibels::gainToDecibels (mag, -90.0f);
        const float n   = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f);
        const float diff = juce::jmax (0.0f, n - prevMag[(size_t) i]);
        prevMag[(size_t) i] = n;

        const float hz = (float) i * binHz;
        if (hz < 150.0f)                  flux[0] += diff * 2.0f;
        else if (hz > 200.0f && hz < 2500.0f) flux[1] += diff * 0.25f;
        else if (hz > 6000.0f)            flux[2] += diff * 0.08f;

        if (i < AudioFeatures::numBins * 2 && (i & 1) == 0)
        {
            auto& dst = work.spectrum[(size_t) (i / 2)];
            const float v = juce::jmax (n, prevMag[(size_t) i + 1]);
            dst = dst * smooth * 0.6f + v * (1.0f - smooth * 0.6f);
        }
    }

    // ---- log-frequency spectrum (20 Hz .. 16 kHz)
    const float fall = peakFalloff.load();
    for (int b = 0; b < AudioFeatures::numLogBins; ++b)
    {
        const float f0 = 20.0f * std::pow (800.0f, (float) b / AudioFeatures::numLogBins);
        const float f1 = 20.0f * std::pow (800.0f, (float) (b + 1) / AudioFeatures::numLogBins);
        int i0 = juce::jlimit (1, half - 1, (int) (f0 / binHz));
        int i1 = juce::jlimit (i0 + 1, half, (int) (f1 / binHz) + 1);
        float m = 0.0f;
        for (int i = i0; i < i1; ++i)
            m = juce::jmax (m, prevMag[(size_t) i]);
        m = juce::jlimit (0.0f, 1.0f, (m - 0.15f) / 0.85f * (1.0f + (float) b / AudioFeatures::numLogBins * 0.6f));

        auto& s = work.logSpectrum[(size_t) b];
        s = m > s ? m * (1.0f - smooth * 0.5f) + s * smooth * 0.5f : s * smooth + m * (1.0f - smooth);
        auto& p = work.logPeaks[(size_t) b];
        p = juce::jmax (s, p - fall);
    }

    // ---- bands
    static const float edges[AudioFeatures::numBands + 1] = { 20, 90, 250, 2000, 6000, 16000 };
    for (int b = 0; b < AudioFeatures::numBands; ++b)
    {
        int i0 = juce::jmax (1, (int) (edges[b] / binHz));
        int i1 = juce::jmin (half, juce::jmax (i0 + 1, (int) (edges[b + 1] / binHz)));
        float sum = 0.0f;
        for (int i = i0; i < i1; ++i)
            sum += prevMag[(size_t) i];
        float v = sum / (float) (i1 - i0);
        v = juce::jlimit (0.0f, 1.0f, (v - 0.2f) * 1.8f);
        work.bandsFast[(size_t) b] = v;
        auto& sb = work.bands[(size_t) b];
        sb = v > sb ? v * 0.6f + sb * 0.4f : sb * smooth + v * (1.0f - smooth);
    }
    work.level = work.level * smooth + levelNow * (1.0f - smooth);

    // ---- onsets (spectral flux with adaptive threshold)
    const float sens = juce::jmax (0.1f, onsetSensitivity.load());
    const int minGap[3] = { (int) (sr * 0.12), (int) (sr * 0.10), (int) (sr * 0.06) };
    for (int k = 0; k < 3; ++k)
    {
        auto& o = onsets[k];
        float mean = 0.0f;
        for (auto h : o.history) mean += h;
        mean /= (float) fluxHistory;
        const float threshold = mean * (1.6f / sens) + 0.02f;
        o.history[(size_t) o.pos] = flux[k];
        o.pos = (o.pos + 1) % fluxHistory;

        o.env *= 0.86f;
        if (flux[k] > threshold && o.samplesSinceHit > minGap[k])
        {
            o.env = 1.0f;
            o.samplesSinceHit = 0;
            ++o.count;
        }
    }
    work.kick = onsets[0].env;  work.kickCount  = onsets[0].count;
    work.snare = onsets[1].env; work.snareCount = onsets[1].count;
    work.hat = onsets[2].env;   work.hatCount   = onsets[2].count;

    // ---- tempo
    tempoEnv[(size_t) tempoPos] = flux[0] + flux[1] * 0.5f;
    tempoPos = (tempoPos + 1) % tempoHistory;
    if (++hopsSinceTempo >= 43)
    {
        hopsSinceTempo = 0;
        estimateTempo();
    }
    work.detectedBpm = bpmEstimate;
    work.sampleRate = sr;

    const juce::SpinLock::ScopedTryLockType tl (lock);
    if (tl.isLocked())
        published = work;
}

void AudioAnalyzer::estimateTempo()
{
    const double hopRate = sr / hopSize;
    std::array<float, tempoHistory> e {};
    float mean = 0.0f;
    for (int i = 0; i < tempoHistory; ++i)
    {
        e[(size_t) i] = tempoEnv[(size_t) ((tempoPos + i) % tempoHistory)];
        mean += e[(size_t) i];
    }
    mean /= tempoHistory;
    if (mean < 1.0e-4f)
        return;
    for (auto& v : e) v -= mean;

    float best = 0.0f;
    double bestBpm = 0.0;
    for (double bpm = 70.0; bpm <= 180.0; bpm += 0.5)
    {
        const double lag = hopRate * 60.0 / bpm;
        const int l0 = (int) lag;
        const float frac = (float) (lag - l0);
        float acc = 0.0f;
        for (int i = l0 + 1; i < tempoHistory; ++i)
        {
            const float delayed = e[(size_t) (i - l0)] * (1.0f - frac) + e[(size_t) (i - l0 - 1)] * frac;
            acc += e[(size_t) i] * delayed;
        }
        // mild preference toward 100-140 BPM to resolve octave errors
        const float w = 1.0f - 0.25f * (float) std::abs (std::log2 (bpm / 120.0));
        acc *= w;
        if (acc > best) { best = acc; bestBpm = bpm; }
    }
    if (bestBpm > 0.0)
        bpmEstimate = bpmEstimate <= 0.0f || std::abs (bpmEstimate - (float) bestBpm) > 8.0f
                          ? (float) bestBpm
                          : bpmEstimate * 0.8f + (float) bestBpm * 0.2f;
}

void AudioAnalyzer::read (AudioFeatures& dest) const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    dest = published;
}
} // namespace moshdeck
