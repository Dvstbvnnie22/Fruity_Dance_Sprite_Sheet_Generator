#include "BeatClock.h"

namespace moshdeck
{
static double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

void BeatClock::updateFromHost (juce::AudioPlayHead* ph, int numSamples, double sampleRate)
{
    juce::ignoreUnused (numSamples, sampleRate);
    if (ph == nullptr) { hostPlaying = false; return; }
    const auto pos = ph->getPosition();
    if (! pos.hasValue()) return;
    if (auto bpm = pos->getBpm()) hostBpm = juce::jlimit (20.0, 999.0, *bpm);
    if (auto sig = pos->getTimeSignature()) hostBeatsPerBar = juce::jmax (1, sig->numerator * 4 / juce::jmax (1, sig->denominator));
    hostPlaying = pos->getIsPlaying();
    if (auto ppq = pos->getPpqPosition())
    {
        hostPpq = *ppq;
        hostHasPpq = true;
    }
    hostStamp = nowSeconds();
}

double BeatClock::beatsNow (double& bpmOut, bool& playingOut, int& bpbOut, float detectedBpm)
{
    const double now = nowSeconds();
    const double dt = lastCall > 0 ? juce::jlimit (0.0, 0.5, now - lastCall) : 0.0;
    lastCall = now;

    const auto m = (Mode) mode.load();
    double bpm = hostBpm.load();
    if (m == Mode::AudioDetect && detectedBpm > 0.0f) bpm = detectedBpm;
    if (m == Mode::Tap) bpm = tapBpm.load();

    const bool playing = hostPlaying.load() && hostHasPpq.load();
    double beats;
    if (m == Mode::Host && playing)
    {
        const double since = juce::jlimit (0.0, 0.25, now - hostStamp.load());
        beats = hostPpq.load() + since * bpm / 60.0;
        freeBeats = beats; // so stopping continues seamlessly
    }
    else
    {
        freeBeats += dt * bpm / 60.0;
        beats = freeBeats;
    }
    wasPlaying = playing;
    beats += nudgeBeats.load();
    bpmOut = bpm;
    playingOut = playing;
    bpbOut = hostBeatsPerBar.load();
    lastShownBpm = bpm;
    lastShownPlaying = playing;
    return beats;
}

void BeatClock::tap()
{
    const double now = nowSeconds();
    const double gap = now - lastTap;
    lastTap = now;
    if (gap > 2.0 || gap < 0.2) { tapCount = 0; tapAccum = 0; return; }
    tapAccum += gap;
    ++tapCount;
    tapBpm = 60.0 / (tapAccum / tapCount);
    if (tapCount > 8) { tapAccum = tapAccum / tapCount * 4; tapCount = 4; }
}
} // namespace moshdeck
