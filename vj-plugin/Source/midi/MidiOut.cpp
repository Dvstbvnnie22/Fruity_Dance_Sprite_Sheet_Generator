#include "MidiOut.h"

namespace moshdeck
{
void MidiOut::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    pendingOffs.clear();
    pendingOffs.reserve (64);
}

void MidiOut::queue (const juce::MidiMessage& m)
{
    if (m.getRawDataSize() > 3) return;
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)
    {
        auto& q = queued[(size_t) scope.startIndex1];
        const auto* r = m.getRawData();
        q = { r[0], (uint8_t) (m.getRawDataSize() > 1 ? r[1] : 0), (uint8_t) (m.getRawDataSize() > 2 ? r[2] : 0) };
    }
}

void MidiOut::generate (juce::MidiBuffer& out, int n, const AudioFeatures& f, bool playing, double ppq, double bpm, int bpb)
{
    int sent = 0;
    auto add = [&] (const juce::MidiMessage& m, int offset) { out.addEvent (m, juce::jlimit (0, juce::jmax (0, n - 1), offset)); ++sent; };

    // ---- scheduled note-offs
    for (auto it = pendingOffs.begin(); it != pendingOffs.end();)
    {
        it->samplesLeft -= n;
        if (it->samplesLeft <= 0)
        {
            add (juce::MidiMessage::noteOff (it->channel, it->note), juce::jmax (0, n + it->samplesLeft));
            it = pendingOffs.erase (it);
        }
        else ++it;
    }
    auto note = [&] (int ch, int nn, float vel, int offset)
    {
        ch = juce::jlimit (1, 16, ch); nn = juce::jlimit (0, 127, nn);
        add (juce::MidiMessage::noteOn (ch, nn, juce::jlimit (0.05f, 1.0f, vel)), offset);
        if (pendingOffs.size() < 64) pendingOffs.push_back ({ ch, nn, (int) (sr * 0.06) + offset });
    };

    // ---- onset notes
    if (settings.onsetNotes)
    {
        const int ch = settings.onsetChannel;
        if (f.kickCount != lastKick)   note (ch, settings.kickNote,  0.5f + f.bandsFast[1] * 0.5f, 0);
        if (f.snareCount != lastSnare) note (ch, settings.snareNote, 0.5f + f.bandsFast[2] * 0.5f, 0);
        if (f.hatCount != lastHat)     note (ch, settings.hatNote,   0.4f + f.bandsFast[4] * 0.6f, 0);
    }
    lastKick = f.kickCount; lastSnare = f.snareCount; lastHat = f.hatCount;

    // ---- beat-locked notes and clock
    const double beatsPerSample = bpm / 60.0 / sr;
    const double ppqEnd = ppq + n * beatsPerSample;
    if (playing && bpm > 0)
    {
        if (settings.beatNotes)
            for (double b = std::ceil (ppq); b < ppqEnd; b += 1.0)
            {
                const int off = (int) ((b - ppq) / beatsPerSample);
                const bool isBar = ((long long) std::llround (b)) % juce::jmax (1, bpb) == 0;
                note (settings.beatChannel, isBar ? settings.barNote : settings.beatNote, isBar ? 1.0f : 0.8f, off);
            }
        if (settings.clock)
        {
            if (! wasPlaying) add (juce::MidiMessage::midiStart(), 0);
            for (double t = std::ceil (ppq * 24.0) / 24.0; t < ppqEnd; t += 1.0 / 24.0)
                add (juce::MidiMessage::midiClock(), (int) ((t - ppq) / beatsPerSample));
        }
    }
    else if (wasPlaying && settings.clock)
        add (juce::MidiMessage::midiStop(), 0);
    wasPlaying = playing;

    // ---- band CCs (throttled to ~100 Hz)
    ccThrottle -= n;
    if (settings.bandCCs && ccThrottle <= 0)
    {
        ccThrottle = (int) (sr * 0.01);
        for (int b = 0; b < 5; ++b)
        {
            const int v = juce::jlimit (0, 127, (int) std::round (f.bands[(size_t) b] * 127.0f));
            if (v != lastCC[b])
            {
                add (juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, settings.ccChannel.load()), juce::jlimit (0, 127, settings.ccBase + b), v), 0);
                lastCC[b] = v;
            }
        }
    }

    // ---- controller feedback from the message thread
    auto scope = fifo.read (fifo.getNumReady());
    auto drain = [&] (int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const auto& q = queued[(size_t) (start + i)];
            if (settings.feedback) add (juce::MidiMessage (q[0], q[1], q[2]), 0);
        }
    };
    drain (scope.startIndex1, scope.blockSize1);
    drain (scope.startIndex2, scope.blockSize2);
    sentCounter += (uint32_t) sent;
}

juce::ValueTree MidiOut::toValueTree() const
{
    juce::ValueTree t ("MidiOut");
    const auto& s = settings;
    t.setProperty ("onset", s.onsetNotes.load(), nullptr);
    t.setProperty ("onsetCh", s.onsetChannel.load(), nullptr);
    t.setProperty ("kick", s.kickNote.load(), nullptr);
    t.setProperty ("snare", s.snareNote.load(), nullptr);
    t.setProperty ("hat", s.hatNote.load(), nullptr);
    t.setProperty ("beat", s.beatNotes.load(), nullptr);
    t.setProperty ("beatCh", s.beatChannel.load(), nullptr);
    t.setProperty ("beatNote", s.beatNote.load(), nullptr);
    t.setProperty ("barNote", s.barNote.load(), nullptr);
    t.setProperty ("cc", s.bandCCs.load(), nullptr);
    t.setProperty ("ccCh", s.ccChannel.load(), nullptr);
    t.setProperty ("ccBase", s.ccBase.load(), nullptr);
    t.setProperty ("clock", s.clock.load(), nullptr);
    t.setProperty ("fb", s.feedback.load(), nullptr);
    t.setProperty ("thru", s.thru.load(), nullptr);
    return t;
}

void MidiOut::fromValueTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    auto& s = settings;
    s.onsetNotes = (bool) t.getProperty ("onset", true);
    s.onsetChannel = (int) t.getProperty ("onsetCh", 10);
    s.kickNote = (int) t.getProperty ("kick", 36);
    s.snareNote = (int) t.getProperty ("snare", 38);
    s.hatNote = (int) t.getProperty ("hat", 42);
    s.beatNotes = (bool) t.getProperty ("beat", false);
    s.beatChannel = (int) t.getProperty ("beatCh", 16);
    s.beatNote = (int) t.getProperty ("beatNote", 60);
    s.barNote = (int) t.getProperty ("barNote", 48);
    s.bandCCs = (bool) t.getProperty ("cc", true);
    s.ccChannel = (int) t.getProperty ("ccCh", 1);
    s.ccBase = (int) t.getProperty ("ccBase", 20);
    s.clock = (bool) t.getProperty ("clock", false);
    s.feedback = (bool) t.getProperty ("fb", true);
    s.thru = (bool) t.getProperty ("thru", false);
}
} // namespace moshdeck
