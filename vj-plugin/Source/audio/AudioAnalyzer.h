#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace moshdeck
{
/** Snapshot of everything the visual engine needs to know about the audio. */
struct AudioFeatures
{
    static constexpr int numBins    = 512; // linear FFT bins (Shadertoy iChannel "music" layout)
    static constexpr int numLogBins = 128; // log-frequency bins for spectrum visualizers
    static constexpr int numWave    = 512;
    static constexpr int numBands   = 5;   // sub, bass, mid, high-mid, treble

    std::array<float, numBins>    spectrum {};     // 0..1
    std::array<float, numLogBins> logSpectrum {};  // 0..1 smoothed w/ peak falloff
    std::array<float, numLogBins> logPeaks {};     // 0..1 peak-hold
    std::array<float, numWave>    wave {};         // -1..1
    std::array<float, numBands>   bands {};        // 0..1 smoothed
    std::array<float, numBands>   bandsFast {};    // 0..1 un-smoothed (attack)
    float level = 0.0f;                            // RMS 0..1

    // onset envelopes (1 on hit, decaying) & hit counters for edge detection
    float kick = 0.0f, snare = 0.0f, hat = 0.0f;
    uint32_t kickCount = 0, snareCount = 0, hatCount = 0;

    float detectedBpm = 0.0f;  // 0 when unknown
    double sampleRate = 44100.0;
};

/**
    Real-time audio analysis: FFT spectrum, waveform, band energies, per-band onset
    (kick / snare / hat) detection and tempo estimation from the onset envelope.
    push() is called from the audio thread, read() from any other thread.
*/
class AudioAnalyzer
{
public:
    AudioAnalyzer();

    void prepare (double sampleRate);
    void push (const float* const* channels, int numChannels, int numSamples);
    void read (AudioFeatures& dest) const;

    std::atomic<float> inputGain { 1.0f };
    std::atomic<float> smoothing { 0.65f };      // 0 = none, 0.95 = very smooth
    std::atomic<float> onsetSensitivity { 1.0f }; // >1 = more hits
    std::atomic<float> peakFalloff { 0.015f };

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int hopSize  = 512;
    static constexpr int fluxHistory = 43;
    static constexpr int tempoHistory = 512;

    void processFrame();
    void estimateTempo();

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::array<float, fftSize> ring {};
    int ringPos = 0, samplesSinceHop = 0;
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> prevMag {};

    struct Onset
    {
        std::array<float, fluxHistory> history {};
        int pos = 0;
        int samplesSinceHit = 1 << 20;
        float env = 0.0f;
        uint32_t count = 0;
    };
    Onset onsets[3];

    std::array<float, tempoHistory> tempoEnv {};
    int tempoPos = 0, hopsSinceTempo = 0;
    float bpmEstimate = 0.0f;

    double sr = 44100.0;
    AudioFeatures work;

    mutable juce::SpinLock lock;
    AudioFeatures published;
};
} // namespace moshdeck
