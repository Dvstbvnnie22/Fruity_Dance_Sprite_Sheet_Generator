#pragma once
#include "audio/AudioAnalyzer.h"
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <vector>

namespace moshdeck
{
/**
    MIDI OUT generator: turns the audio analysis and the musical clock into MIDI
    so other plugins / lighting rigs / hardware can follow the visuals:
      - onset notes (kick / snare / hat) with velocity from onset strength
      - beat & bar notes locked to FL's song position
      - band energy as continuous CCs
      - MIDI clock (24 ppqn) + start/stop
      - controller feedback for MIDI-learned parameters (motorised faders / LED rings)
*/
class MidiOut
{
public:
    struct Settings
    {
        std::atomic<bool> onsetNotes { true };
        std::atomic<int>  onsetChannel { 10 };
        std::atomic<int>  kickNote { 36 }, snareNote { 38 }, hatNote { 42 };
        std::atomic<bool> beatNotes { false };
        std::atomic<int>  beatChannel { 16 }, beatNote { 60 }, barNote { 48 };
        std::atomic<bool> bandCCs { true };
        std::atomic<int>  ccChannel { 1 }, ccBase { 20 };
        std::atomic<bool> clock { false };
        std::atomic<bool> feedback { true };
        std::atomic<bool> thru { false };
    };

    void prepare (double sampleRate);
    /** Called at the end of processBlock. ppq/bpm describe the block start. */
    void generate (juce::MidiBuffer& out, int numSamples, const AudioFeatures& f,
                   bool playing, double ppq, double bpm, int beatsPerBar);
    /** Message thread -> audio thread (controller feedback). */
    void queue (const juce::MidiMessage& m);

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

    Settings settings;
    std::atomic<uint32_t> sentCounter { 0 };

private:
    struct PendingOff { int channel, note, samplesLeft; };
    std::vector<PendingOff> pendingOffs;
    uint32_t lastKick = 0, lastSnare = 0, lastHat = 0;
    int lastCC[5] = { -1, -1, -1, -1, -1 };
    int ccThrottle = 0;
    bool wasPlaying = false;
    double sr = 44100.0;

    juce::AbstractFifo fifo { 512 };
    std::array<std::array<uint8_t, 3>, 512> queued {};
};
} // namespace moshdeck
