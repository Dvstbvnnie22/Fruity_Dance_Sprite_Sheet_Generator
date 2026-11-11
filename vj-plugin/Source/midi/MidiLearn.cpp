#include "MidiLearn.h"
#include "model/Scene.h"

namespace moshdeck
{
MidiLearn::MidiLearn() {}

void MidiLearn::handleIncoming (const juce::MidiMessage& m, MidiState& st)
{
    const int ch = m.getChannel() - 1;
    if (m.isController() && ch >= 0)
        st.cc[(size_t) (ch * 128 + m.getControllerNumber())] = (float) m.getControllerValue() / 127.0f;
    else if (m.isNoteOn())
        st.notes[(size_t) m.getNoteNumber()] = m.getFloatVelocity();
    else if (m.isNoteOff())
        st.notes[(size_t) m.getNoteNumber()] = 0.0f;
    else
        return;

    const auto* raw = m.getRawData();
    st.lastStatus = raw[0]; st.lastData1 = raw[1]; st.lastData2 = m.getRawDataSize() > 2 ? raw[2] : 0;
    ++st.eventCounter;

    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)
        events[(size_t) scope.startIndex1] = { raw[0], raw[1], (uint8_t) (m.getRawDataSize() > 2 ? raw[2] : 0) };
}

void MidiLearn::process (Scene& scene, const std::function<void (const juce::String&)>& onAction)
{
    bool changed = false;
    std::vector<juce::String> actions;
    struct RouteLearn { juce::String dest; int ch; bool note; int num; };
    std::vector<RouteLearn> routeLearns;
    {
        const juce::ScopedLock sl (lock);
        auto scope = fifo.read (fifo.getNumReady());
        auto handle = [&] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                const auto e = events[(size_t) (start + i)];
                const int type = e.status & 0xf0, ch = e.status & 0x0f;
                const bool isCC = type == 0xb0;
                const bool isOn = type == 0x90 && e.d2 > 0;
                const bool isOff = type == 0x80 || (type == 0x90 && e.d2 == 0);
                if (! (isCC || isOn || isOff)) continue;

                if (armed.startsWith ("R.") && (isCC || isOn))
                {
                    routeLearns.push_back ({ armed.substring (2), ch, ! isCC, (int) e.d1 });
                    armed.clear();
                    continue;
                }
                if (armed.isNotEmpty() && (isCC || isOn))
                {
                    mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&] (const MidiMapping& mm) { return mm.target == armed; }), mappings.end());
                    MidiMapping mm;
                    mm.target = armed;
                    mm.channel = ch;
                    mm.isNote = ! isCC;
                    mm.number = e.d1;
                    mm.toggle = mm.isNote && ! armed.startsWith ("A.");
                    mappings.push_back (mm);
                    armed.clear();
                    changed = true;
                    continue;
                }
                for (auto& mm : mappings)
                {
                    if (mm.channel >= 0 && mm.channel != ch) continue;
                    if (mm.number != e.d1 || mm.isNote == isCC) continue;
                    if (mm.target.startsWith ("A."))
                    {
                        if ((isCC && e.d2 >= 64) || isOn) actions.push_back (mm.target);
                        continue;
                    }
                    float n;
                    if (isCC) n = mm.min + (mm.max - mm.min) * (float) e.d2 / 127.0f;
                    else if (mm.toggle)
                    {
                        if (! isOn) continue;
                        mm.toggleState = ! mm.toggleState;
                        n = mm.toggleState ? mm.max : mm.min;
                    }
                    else n = isOn ? mm.min + (mm.max - mm.min) * (float) e.d2 / 127.0f : mm.min;
                    scene.setNormalised (mm.target, n);
                    mm.lastSent = isCC ? e.d2 : -1; // don't echo back what we just received
                }
            }
        };
        handle (scope.startIndex1, scope.blockSize1);
        handle (scope.startIndex2, scope.blockSize2);
    }
    for (auto& a : actions) if (onAction) onAction (a);
    for (auto& r : routeLearns) if (onRouteLearned) onRouteLearned (r.dest, r.ch, r.note, r.num);
    if (changed && onMappingsChanged) onMappingsChanged();
}

void MidiLearn::collectFeedback (Scene& scene, std::vector<juce::MidiMessage>& out)
{
    const juce::ScopedLock sl (lock);
    for (auto& mm : mappings)
    {
        if (! mm.sendFeedback || mm.isNote || mm.target.startsWith ("A.")) continue;
        const float n = scene.getNormalised (mm.target);
        const float r = mm.max != mm.min ? (n - mm.min) / (mm.max - mm.min) : 0.0f;
        const int v = juce::jlimit (0, 127, (int) std::round (r * 127.0f));
        if (v != mm.lastSent)
        {
            out.push_back (juce::MidiMessage::controllerEvent (juce::jmax (0, mm.channel) + 1, mm.number, v));
            mm.lastSent = v;
        }
    }
}

std::vector<MidiMapping> MidiLearn::getMappings() const { const juce::ScopedLock sl (lock); return mappings; }

void MidiLearn::removeMapping (int i)
{
    { const juce::ScopedLock sl (lock); if (i >= 0 && i < (int) mappings.size()) mappings.erase (mappings.begin() + i); }
    if (onMappingsChanged) onMappingsChanged();
}

void MidiLearn::removeMappingsFor (const juce::String& t)
{
    { const juce::ScopedLock sl (lock);
      mappings.erase (std::remove_if (mappings.begin(), mappings.end(), [&] (const MidiMapping& m) { return m.target == t; }), mappings.end()); }
    if (onMappingsChanged) onMappingsChanged();
}

void MidiLearn::clearAll() { { const juce::ScopedLock sl (lock); mappings.clear(); } if (onMappingsChanged) onMappingsChanged(); }

void MidiLearn::setMapping (int i, const MidiMapping& m)
{
    { const juce::ScopedLock sl (lock); if (i >= 0 && i < (int) mappings.size()) mappings[(size_t) i] = m; }
    if (onMappingsChanged) onMappingsChanged();
}

juce::String MidiLearn::describeMapping (const juce::String& target) const
{
    const juce::ScopedLock sl (lock);
    for (auto& m : mappings)
        if (m.target == target)
            return (m.isNote ? "Note " + juce::MidiMessage::getMidiNoteName (m.number, true, true, 3) : "CC " + juce::String (m.number))
                   + (m.channel >= 0 ? " ch" + juce::String (m.channel + 1) : juce::String());
    return {};
}

juce::ValueTree MidiLearn::toValueTree() const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree t ("MidiMappings");
    for (auto& m : mappings)
    {
        juce::ValueTree c ("Map");
        c.setProperty ("target", m.target, nullptr);
        c.setProperty ("ch", m.channel, nullptr);
        c.setProperty ("note", m.isNote, nullptr);
        c.setProperty ("num", m.number, nullptr);
        c.setProperty ("min", m.min, nullptr);
        c.setProperty ("max", m.max, nullptr);
        c.setProperty ("toggle", m.toggle, nullptr);
        c.setProperty ("fb", m.sendFeedback, nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

void MidiLearn::fromValueTree (const juce::ValueTree& t)
{
    {
        const juce::ScopedLock sl (lock);
        mappings.clear();
        for (auto c : t)
        {
            MidiMapping m;
            m.target = c["target"].toString();
            m.channel = c["ch"];
            m.isNote = c["note"];
            m.number = c["num"];
            m.min = c.getProperty ("min", 0.0f);
            m.max = c.getProperty ("max", 1.0f);
            m.toggle = c["toggle"];
            m.sendFeedback = c.getProperty ("fb", true);
            mappings.push_back (m);
        }
    }
    if (onMappingsChanged) onMappingsChanged();
}
} // namespace moshdeck
