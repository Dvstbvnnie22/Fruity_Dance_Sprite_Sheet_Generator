#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace moshdeck
{
/**
    Musical clock for the visuals. Follows FL Studio's transport (tempo, song
    position, play state, time signature) from the plugin's AudioPlayHead and
    extrapolates between audio blocks so video frames get a smooth beat position.
    When the host is stopped it keeps free-running at the last host tempo, the
    tempo detected from the audio, or a tapped tempo.
*/
class BeatClock
{
public:
    enum class Mode { Host = 0, AudioDetect, Tap };

    // ---- audio thread
    void updateFromHost (juce::AudioPlayHead* playHead, int numSamples, double sampleRate);

    // ---- render thread
    /** Returns the beat position "now"; also outputs tempo and play state. */
    double beatsNow (double& bpmOut, bool& playingOut, int& beatsPerBarOut, float detectedBpm);

    // ---- UI
    void tap();
    void nudge (double beats) { nudgeBeats.store (nudgeBeats.load() + beats); }
    std::atomic<int> mode { (int) Mode::Host };
    std::atomic<double> tapBpm { 120.0 };
    std::atomic<double> lastShownBpm { 120.0 };
    std::atomic<bool> lastShownPlaying { false };

private:
    std::atomic<double> hostBpm { 120.0 }, hostPpq { 0.0 }, hostStamp { 0.0 };
    std::atomic<bool> hostPlaying { false }, hostHasPpq { false };
    std::atomic<int> hostBeatsPerBar { 4 };
    std::atomic<double> nudgeBeats { 0.0 };

    // render-thread state for free-running
    double freeBeats = 0.0, lastCall = 0.0;
    bool wasPlaying = false;

    double lastTap = 0.0;
    int tapCount = 0;
    double tapAccum = 0.0;
};
} // namespace moshdeck
