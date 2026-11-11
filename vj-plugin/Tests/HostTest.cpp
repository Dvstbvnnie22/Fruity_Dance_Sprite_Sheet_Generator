// Loads the built VST3 like a DAW would: scan, instantiate, process audio with a
// fake transport + MIDI, check MIDI output, save/restore state, open the editor.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 128.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (true);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

class HostTestApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MoshDeckHostTest"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise (const juce::String& cmd) override
    {
        juce::MessageManager::callAsync ([this, cmd] { setApplicationReturnValue (run (cmd.unquoted()) ? 0 : 1); quit(); });
    }
    void shutdown() override {}

    bool run (const juce::String& path)
    {
        int fails = 0;
        auto check = [&] (bool c, const char* what) { std::cout << (c ? "  ok   " : "  FAIL ") << what << std::endl; if (! c) ++fails; };

        juce::VST3PluginFormat fmt;
        juce::OwnedArray<juce::PluginDescription> descs;
        fmt.findAllTypesForFile (descs, path);
        check (descs.size() == 1, "VST3 scanned, one plugin found");
        if (descs.isEmpty()) return false;
        std::cout << "  name=" << descs[0]->name << " vendor=" << descs[0]->manufacturerName
                  << " ins=" << descs[0]->numInputChannels << " outs=" << descs[0]->numOutputChannels << std::endl;

        juce::String err;
        auto inst = fmt.createInstanceFromDescription (*descs[0], 44100.0, 512, err);
        check (inst != nullptr, "instantiated");
        if (inst == nullptr) { std::cout << err << std::endl; return false; }
        check (inst->acceptsMidi(), "accepts MIDI");
        check (inst->producesMidi(), "produces MIDI");
        check (inst->getParameters().size() >= 18, "host parameters (crossfader, master, 16 macros)");

        FakePlayHead ph;
        inst->setPlayHead (&ph);
        inst->setPlayConfigDetails (2, 2, 44100.0, 512);
        inst->prepareToPlay (44100.0, 512);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        int midiOutEvents = 0, noteOns = 0;
        bool passthrough = true;
        double phase = 0.0;
        for (int block = 0; block < 400; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double t = (block * 512 + i) / 44100.0;
                const double beatPh = std::fmod (t * ph.bpm / 60.0, 1.0);
                const float s = (float) (std::sin (phase) * std::exp (-beatPh * 10.0) * 0.8);
                phase += 2.0 * juce::MathConstants<double>::pi * (55.0 + 90.0 * std::exp (-beatPh * 25.0)) / 44100.0;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            juce::AudioBuffer<float> copy (buf);
            midi.clear();
            if (block == 10) midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 100), 0);
            inst->processBlock (buf, midi);
            for (int i = 0; i < 512 && passthrough; ++i)
                if (std::abs (buf.getSample (0, i) - copy.getSample (0, i)) > 1e-6f) passthrough = false;
            for (const auto m : midi) { ++midiOutEvents; if (m.getMessage().isNoteOn()) ++noteOns; }
            ph.ppq += 512.0 / 44100.0 * ph.bpm / 60.0;
        }
        check (passthrough, "audio passes through unchanged");
        check (midiOutEvents > 0, "MIDI OUT events generated");
        check (noteOns > 10, "onset notes generated from kicks");
        std::cout << "  midi out events=" << midiOutEvents << " noteOns=" << noteOns << std::endl;

        juce::MemoryBlock state;
        inst->getStateInformation (state);
        check (state.getSize() > 100, "state saved");
        inst->setStateInformation (state.getData(), (int) state.getSize());
        juce::MemoryBlock state2;
        inst->getStateInformation (state2);
        check (state2.getSize() > 100, "state restored and re-saved");

        if (auto* ed = inst->createEditorIfNeeded())
        {
            check (true, "editor created");
            juce::DocumentWindow w ("host", juce::Colours::black, 0);
            w.setContentNonOwned (ed, true);
            w.setVisible (true);
            const auto end = juce::Time::getMillisecondCounter() + 3000;
            while (juce::Time::getMillisecondCounter() < end)
            {
                juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
                inst->processBlock (buf, midi);
            }
            w.clearContentComponent();
            delete ed;
            check (true, "editor ran 3s and closed");
        }
        else check (false, "editor created");

        inst->releaseResources();
        inst.reset();
        std::cout << (fails == 0 ? "HOST TEST PASSED" : "HOST TEST FAILED") << std::endl;
        return fails == 0;
    }
};

START_JUCE_APPLICATION (HostTestApp)
