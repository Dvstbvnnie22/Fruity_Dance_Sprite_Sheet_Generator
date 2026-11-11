#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/VideoView.h"
#include "milk/MilkPreset.h"

using namespace moshdeck;

juce::AudioProcessorValueTreeState::ParameterLayout MoshDeckProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "xfade", 1 }, "Crossfader A/B", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master Brightness", 0.0f, 2.0f, 1.0f));
    for (int i = 0; i < numMacros; ++i)
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "macro" + juce::String (i + 1), 1 },
                                                                  "Macro " + juce::String (i + 1), 0.0f, 1.0f, 0.0f));
    return layout;
}

MoshDeckProcessor::MoshDeckProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HostParams", createLayout())
{
    for (int i = 0; i < numMacros; ++i)
        macroParams[(size_t) i] = apvts.getRawParameterValue ("macro" + juce::String (i + 1));
    hostXfade = apvts.getRawParameterValue ("xfade");
    hostMaster = apvts.getRawParameterValue ("master");
    midiLearn.onRouteLearned = [this] (const juce::String& dest, int ch, bool isNote, int num)
    {
        ModRoute r;
        r.source = isNote ? ModSourceType::MidiNote : ModSourceType::MidiCC;
        r.sourceIndex = isNote ? num : ch * 128 + num;
        r.dest = dest;
        scene.addRoute (r, 0.5f);
    };
    startTimerHz (30);
}

MoshDeckProcessor::~MoshDeckProcessor()
{
    stopTimer();
    outputWindow.reset();
}

bool MoshDeckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void MoshDeckProcessor::prepareToPlay (double sampleRate, int)
{
    analyzer.prepare (sampleRate);
    midiOut.prepare (sampleRate);
}

void MoshDeckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    // audio passes through untouched; we only listen
    analyzer.push (buffer.getArrayOfReadPointers(), juce::jmin (2, buffer.getNumChannels()), n);

    // FL transport -> beat clock
    clock.updateFromHost (getPlayHead(), n, getSampleRate());
    double ppq = 0.0, bpm = 120.0;
    bool playing = false;
    int bpb = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto sig = pos->getTimeSignature()) bpb = juce::jmax (1, sig->numerator * 4 / juce::jmax (1, sig->denominator));
            playing = pos->getIsPlaying();
        }
    lastPpq = ppq; lastBpmHost = bpm; lastPlaying = playing; lastBpb = bpb;

    // MIDI in -> learn / mod sources
    for (const auto meta : midi)
        midiLearn.handleIncoming (meta.getMessage(), midiState);
    if (! midiOut.settings.thru) midi.clear();

    // MIDI out
    AudioFeatures f;
    analyzer.read (f);
    midiOut.generate (midi, n, f, playing, ppq, bpm, bpb);
}

//==============================================================================
void MoshDeckProcessor::buildFrameInput (FrameInput& in)
{
    analyzer.read (in.mod.audio);
    double bpm = 120.0;
    bool playing = false;
    int bpb = 4;
    in.mod.beats = clock.beatsNow (bpm, playing, bpb, in.mod.audio.detectedBpm);
    in.mod.bpm = bpm;
    in.mod.beatsPerBar = bpb;
    in.hostPlaying = playing;
    const double ph = in.mod.beats - std::floor (in.mod.beats);
    in.mod.beatPulse = (float) std::exp (-ph * 6.0);
    for (size_t i = 0; i < in.mod.cc.size(); ++i) in.mod.cc[i] = midiState.cc[i].load();
    for (size_t i = 0; i < 128; ++i) in.mod.notes[i] = midiState.notes[i].load();
    for (int i = 0; i < numMacros; ++i) in.mod.macros[(size_t) i] = macroParams[(size_t) i]->load();
}

void MoshDeckProcessor::setLayerStatus (const juce::String& uid, const juce::String& status)
{
    const juce::ScopedLock sl (statusLock);
    layerStatus[uid] = status;
}

juce::String MoshDeckProcessor::getLayerStatus (const juce::String& uid) const
{
    const juce::ScopedLock sl (statusLock);
    auto it = layerStatus.find (uid);
    return it != layerStatus.end() ? it->second : juce::String();
}

void MoshDeckProcessor::onDeckAdvance (int deck)
{
    juce::MessageManager::callAsync ([this, deck] { nextDeckClip (deck, 1); });
}

void MoshDeckProcessor::onMilkAdvance (const juce::String& uid)
{
    juce::MessageManager::callAsync ([this, uid] { stepMilkPreset (uid, 1); });
}

//==============================================================================
void MoshDeckProcessor::loadDeckFiles (int d, const juce::StringArray& files, bool append)
{
    auto st = scene.getDeck (d);
    if (! append) { st.files.clear(); st.current = 0; }
    const int first = st.files.size();
    st.files.addArray (files);
    if (! append || first == 0) st.current = juce::jlimit (0, juce::jmax (0, st.files.size() - 1), append ? first : 0);
    scene.setDeck (d, st);
    if (! st.files.isEmpty()) decks[d].load (st.files[st.current]);
}

void MoshDeckProcessor::selectDeckClip (int d, int index)
{
    auto st = scene.getDeck (d);
    if (st.files.isEmpty()) return;
    st.current = juce::jlimit (0, st.files.size() - 1, index);
    scene.setDeck (d, st);
    decks[d].load (st.files[st.current]);
}

void MoshDeckProcessor::nextDeckClip (int d, int delta)
{
    auto st = scene.getDeck (d);
    if (st.files.isEmpty()) return;
    selectDeckClip (d, (st.current + delta + st.files.size()) % st.files.size());
}

juce::StringArray MoshDeckProcessor::milkPresetList (const juce::String& folder) const
{
    juce::StringArray out;
    for (auto& n : milk::BuiltinMilkPresets::names()) out.add ("builtin:" + n);
    const juce::File dir (folder);
    if (folder.isNotEmpty() && dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, true, "*.milk");
        files.sort();
        for (auto& f : files) out.add (f.getFullPathName());
    }
    return out;
}

void MoshDeckProcessor::stepMilkPreset (const juce::String& uid, int delta, bool random)
{
    Layer l;
    if (! scene.getLayer (uid, l) || l.source != SourceType::Milk) return;
    const auto list = milkPresetList (l.milkFolder.isNotEmpty() ? l.milkFolder : defaultMilkFolder);
    if (list.isEmpty()) return;
    int idx = list.indexOf (l.milkPreset);
    idx = random ? rng.nextInt (list.size()) : (idx + delta + list.size()) % list.size();
    const auto spec = list[idx];
    scene.editLayer (uid, [&] (Layer& x) { x.milkPreset = spec; });
}

void MoshDeckProcessor::performAction (const juce::String& a)
{
    if (a == "A.deckA.next") nextDeckClip (0, 1);
    else if (a == "A.deckA.prev") nextDeckClip (0, -1);
    else if (a == "A.deckB.next") nextDeckClip (1, 1);
    else if (a == "A.deckB.prev") nextDeckClip (1, -1);
    else if (a == "A.tap") clock.tap();
    else if (a == "A.milk.next" || a == "A.milk.random")
    {
        for (auto& l : scene.getLayers())
            if (l.source == SourceType::Milk) stepMilkPreset (l.uid, 1, a == "A.milk.random");
    }
    else if (a == "A.xfade.cut")
        scene.setValue (Scene::crossfaderId, scene.getValue (Scene::crossfaderId) < 0.5f ? 1.0f : 0.0f);
    else if (a == "A.nudge.fwd") clock.nudge (0.05);
    else if (a == "A.nudge.back") clock.nudge (-0.05);
}

//==============================================================================
void MoshDeckProcessor::timerCallback()
{
    midiLearn.process (scene, [this] (const juce::String& a) { performAction (a); });

    std::vector<juce::MidiMessage> fb;
    midiLearn.collectFeedback (scene, fb);
    for (auto& m : fb) midiOut.queue (m);

    // two-way sync of host-automatable globals
    auto sync = [this] (std::atomic<float>* hostVal, const char* hostId, const char* sceneId, float& lastHost, float& lastScene)
    {
        const float h = hostVal->load(), s = scene.getValue (sceneId);
        if (h != lastHost) { scene.setValue (sceneId, h); lastHost = h; lastScene = h; }
        else if (s != lastScene)
        {
            if (auto* p = apvts.getParameter (hostId))
                p->setValueNotifyingHost (p->convertTo0to1 (s));
            lastScene = s; lastHost = hostVal->load();
        }
    };
    sync (hostXfade, "xfade", Scene::crossfaderId, lastHostXfade, lastSceneXfade);
    sync (hostMaster, "master", Scene::masterId, lastHostMaster, lastSceneMaster);
}

void MoshDeckProcessor::openOutputWindow (int displayIndex, bool fullscreen)
{
    outputWindow.reset();
    outputWindow = std::make_unique<OutputWindow> (*this, displayIndex, fullscreen);
    outputWindow->onClose = [this] { juce::MessageManager::callAsync ([this] { closeOutputWindow(); }); };
}

void MoshDeckProcessor::closeOutputWindow() { outputWindow.reset(); }

RenderEngine* MoshDeckProcessor::getOutputEngine() const
{
    return outputWindow != nullptr ? &outputWindow->getEngine() : nullptr;
}

//==============================================================================
juce::ValueTree MoshDeckProcessor::saveScenePreset() const
{
    juce::ValueTree t ("MoshDeckPreset");
    t.appendChild (scene.toValueTree(), nullptr);
    return t;
}

void MoshDeckProcessor::loadScenePreset (const juce::ValueTree& t)
{
    auto s = t.getChildWithName ("MoshDeckScene");
    if (! s.isValid()) return;
    scene.fromValueTree (s);
    for (int d = 0; d < 2; ++d)
    {
        auto st = scene.getDeck (d);
        if (! st.files.isEmpty()) decks[d].load (st.files[juce::jlimit (0, st.files.size() - 1, st.current)]);
    }
}

void MoshDeckProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("MoshDeckState");
    root.appendChild (scene.toValueTree(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    root.appendChild (midiOut.toValueTree(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.setProperty ("canvasW", canvasW.load(), nullptr);
    root.setProperty ("canvasH", canvasH.load(), nullptr);
    root.setProperty ("clockMode", clock.mode.load(), nullptr);
    root.setProperty ("tapBpm", clock.tapBpm.load(), nullptr);
    root.setProperty ("gain", analyzer.inputGain.load(), nullptr);
    root.setProperty ("smooth", analyzer.smoothing.load(), nullptr);
    root.setProperty ("sens", analyzer.onsetSensitivity.load(), nullptr);
    root.setProperty ("milkFolder", defaultMilkFolder, nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void MoshDeckProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("MoshDeckState")) return;
    juce::ValueTree preset ("MoshDeckPreset");
    preset.appendChild (root.getChildWithName ("MoshDeckScene").createCopy(), nullptr);
    loadScenePreset (preset);
    midiLearn.fromValueTree (root.getChildWithName ("MidiMappings"));
    midiOut.fromValueTree (root.getChildWithName ("MidiOut"));
    auto hp = root.getChildWithName (apvts.state.getType());
    if (hp.isValid()) apvts.replaceState (hp);
    canvasW = (int) root.getProperty ("canvasW", 1280);
    canvasH = (int) root.getProperty ("canvasH", 720);
    clock.mode = (int) root.getProperty ("clockMode", 0);
    clock.tapBpm = (double) root.getProperty ("tapBpm", 120.0);
    analyzer.inputGain = (float) root.getProperty ("gain", 1.0f);
    analyzer.smoothing = (float) root.getProperty ("smooth", 0.65f);
    analyzer.onsetSensitivity = (float) root.getProperty ("sens", 1.0f);
    defaultMilkFolder = root.getProperty ("milkFolder", "").toString();
}

juce::AudioProcessorEditor* MoshDeckProcessor::createEditor() { return new MoshDeckEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MoshDeckProcessor(); }
