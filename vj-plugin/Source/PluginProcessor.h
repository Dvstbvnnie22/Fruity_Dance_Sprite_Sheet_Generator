#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "audio/AudioAnalyzer.h"
#include "audio/BeatClock.h"
#include "midi/MidiLearn.h"
#include "midi/MidiOut.h"
#include "model/Scene.h"
#include "media/MediaDeck.h"
#include "render/RenderEngine.h"

namespace moshdeck { class OutputWindow; }

class MoshDeckProcessor : public juce::AudioProcessor,
                          public moshdeck::EngineHost,
                          private juce::Timer
{
public:
    MoshDeckProcessor();
    ~MoshDeckProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MoshDeck VJ"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- EngineHost
    moshdeck::Scene& getScene() override { return scene; }
    moshdeck::MediaDeck& getDeck (int i) override { return decks[juce::jlimit (0, 1, i)]; }
    void buildFrameInput (moshdeck::FrameInput&) override;
    void getCanvasSize (int& w, int& h) override { w = canvasW.load(); h = canvasH.load(); }
    void setLayerStatus (const juce::String& uid, const juce::String& status) override;
    void onDeckAdvance (int deck) override;
    void onMilkAdvance (const juce::String& layerUid) override;

    // ---- app API (message thread)
    juce::String getLayerStatus (const juce::String& uid) const;
    void loadDeckFiles (int deck, const juce::StringArray& files, bool append);
    void selectDeckClip (int deck, int index);
    void nextDeckClip (int deck, int delta = 1);
    void performAction (const juce::String& action);
    juce::StringArray milkPresetList (const juce::String& folder) const;
    void stepMilkPreset (const juce::String& layerUid, int delta, bool random = false);
    void setCanvasSize (int w, int h) { canvasW = w; canvasH = h; }

    void openOutputWindow (int displayIndex, bool fullscreen);
    void closeOutputWindow();
    bool isOutputWindowOpen() const { return outputWindow != nullptr; }
    moshdeck::RenderEngine* getOutputEngine() const;

    juce::ValueTree saveScenePreset() const;
    void loadScenePreset (const juce::ValueTree&);

    moshdeck::Scene scene;
    moshdeck::AudioAnalyzer analyzer;
    moshdeck::BeatClock clock;
    moshdeck::MidiState midiState;
    moshdeck::MidiLearn midiLearn;
    moshdeck::MidiOut midiOut;
    moshdeck::MediaDeck decks[2];
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> canvasW { 1280 }, canvasH { 720 };
    juce::String defaultMilkFolder;

    static constexpr int numMacros = 16;

private:
    void timerCallback() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::array<std::atomic<float>*, numMacros> macroParams {};
    std::atomic<float>* hostXfade = nullptr;
    std::atomic<float>* hostMaster = nullptr;
    float lastHostXfade = -1, lastHostMaster = -1, lastSceneXfade = -1, lastSceneMaster = -1;

    juce::CriticalSection statusLock;
    std::map<juce::String, juce::String> layerStatus;

    std::unique_ptr<moshdeck::OutputWindow> outputWindow;
    std::atomic<double> lastPpq { 0.0 }, lastBpmHost { 120.0 };
    std::atomic<bool> lastPlaying { false };
    std::atomic<int> lastBpb { 4 };
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoshDeckProcessor)
};
