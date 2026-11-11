#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <functional>

namespace moshdeck
{
class Scene;

/** Live MIDI controller state, written by the audio thread (mod-matrix sources). */
struct MidiState
{
    std::array<std::atomic<float>, 16 * 128> cc {};
    std::array<std::atomic<float>, 128> notes {};
    std::atomic<uint32_t> eventCounter { 0 };
    std::atomic<int> lastStatus { 0 }, lastData1 { 0 }, lastData2 { 0 };
};

struct MidiMapping
{
    juce::String target;   // param id, or action id starting with "A."
    int channel = -1;      // 0..15, -1 = any
    bool isNote = false;
    int number = 0;        // CC number or note number
    float min = 0.0f, max = 1.0f; // normalised range
    bool toggle = false;   // notes: toggle between min/max
    bool sendFeedback = true;
    // runtime
    bool toggleState = false;
    int lastSent = -1;
};

/**
    MIDI Learn: arm a parameter (or action) from the UI, move a knob / hit a pad,
    and the mapping is created. Incoming events travel from the audio thread
    through a lock-free FIFO and are applied on the message thread.
*/
class MidiLearn
{
public:
    MidiLearn();

    // audio thread
    void handleIncoming (const juce::MidiMessage& m, MidiState& state);

    // message thread
    void arm (const juce::String& target) { const juce::ScopedLock sl (lock); armed = target; }
    juce::String getArmed() const { const juce::ScopedLock sl (lock); return armed; }
    void cancelArm() { arm ({}); }
    void process (Scene& scene, const std::function<void (const juce::String& action)>& onAction);
    void collectFeedback (Scene& scene, std::vector<juce::MidiMessage>& out);

    std::vector<MidiMapping> getMappings() const;
    void removeMapping (int index);
    void removeMappingsFor (const juce::String& target);
    void clearAll();
    juce::String describeMapping (const juce::String& target) const;
    void setMapping (int index, const MidiMapping&);

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

    std::function<void()> onMappingsChanged;
    /** Armed targets starting with "R." create a modulation route instead: (destParam, channel, isNote, number). */
    std::function<void (const juce::String&, int, bool, int)> onRouteLearned;

private:
    struct Event { uint8_t status, d1, d2; };
    juce::AbstractFifo fifo { 1024 };
    std::array<Event, 1024> events {};

    mutable juce::CriticalSection lock;
    juce::String armed;
    std::vector<MidiMapping> mappings;
};
} // namespace moshdeck
