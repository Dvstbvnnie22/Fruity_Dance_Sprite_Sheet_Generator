#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "ParamSlider.h"
#include "audio/AudioAnalyzer.h"

class MoshDeckProcessor;

namespace moshdeck
{
//==============================================================================
class TopBar : public juce::Component, private juce::Timer
{
public:
    explicit TopBar (MoshDeckProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    std::function<void()> onOutputToggled;
private:
    void timerCallback() override;
    MoshDeckProcessor& proc;
    juce::Label title, bpm;
    juce::ComboBox syncMode, canvas, display;
    juce::TextButton tap { "TAP" }, nudgeBack { "<" }, nudgeFwd { ">" }, output { "Output Window" }, fullscreen { "Fullscreen" },
                     save { "Save Scene" }, load { "Load Scene" };
    std::unique_ptr<ParamSlider> xfade, master;
    double beatPhase = 0; bool playing = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class DeckPanel : public juce::Component, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    DeckPanel (MoshDeckProcessor&, int deckIndex);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
private:
    void timerCallback() override;
    MoshDeckProcessor& proc;
    int deck;
    juce::Label title, status;
    juce::TextButton loadBtn { "Load..." }, addBtn { "Add..." }, prev { "<" }, next { ">" }, clear { "Clear" };
    juce::ComboBox playlist;
    std::unique_ptr<ParamPanel> params;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class LayerListPanel : public juce::Component, private juce::ListBoxModel
{
public:
    explicit LayerListPanel (MoshDeckProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    juce::String getSelected() const { return selected; }
    void setSelected (const juce::String& uid);
    std::function<void (const juce::String&)> onSelect;
    static void buildAddMenu (juce::PopupMenu& m);
    static bool handleAddMenu (MoshDeckProcessor& p, int result, const juce::String& replaceLayer = {});
private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selectedRow) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    MoshDeckProcessor& proc;
    juce::ListBox list;
    juce::TextButton add { "+ Layer" }, remove { "Delete" }, up { "Up" }, down { "Down" }, dup { "Dup" };
    std::vector<Layer> layers; // top -> bottom for display
    juce::String selected;
};

//==============================================================================
class ShaderEditorWindow;

class InspectorPanel : public juce::Component, private juce::Timer
{
public:
    explicit InspectorPanel (MoshDeckProcessor&);
    ~InspectorPanel() override;
    void setLayer (const juce::String& uid);
    void rebuild();
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    void layoutContent();
    MoshDeckProcessor& proc;
    juce::String uid;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<juce::Component> owned;
    juce::Label* statusLabel = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<ShaderEditorWindow> shaderWindow;
    struct Row { juce::Component* c; int h; };
    std::vector<Row> rows;
};

//==============================================================================
class ModMatrixPanel : public juce::Component
{
public:
    explicit ModMatrixPanel (MoshDeckProcessor&);
    void rebuild();
    void resized() override;
private:
    MoshDeckProcessor& proc;
    juce::TextButton add { "+ Route" }, lfoBtn { "LFO settings" };
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<juce::Component> owned;
    juce::Label hint;
};

//==============================================================================
class MidiPanel : public juce::Component, private juce::Timer
{
public:
    explicit MidiPanel (MoshDeckProcessor&);
    void rebuild();
    void resized() override;
private:
    void timerCallback() override;
    MoshDeckProcessor& proc;
    juce::Label monitor, learnState;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<juce::Component> owned, outControls, actionButtons;
    juce::TextButton clearAll { "Clear all mappings" };
    uint32_t lastEvents = 0;
    size_t lastCount = 0;
};

//==============================================================================
class AudioPanel : public juce::Component, private juce::Timer
{
public:
    explicit AudioPanel (MoshDeckProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    MoshDeckProcessor& proc;
    AudioFeatures f;
    juce::Slider gain, smooth, sens;
    juce::Label gainL, smoothL, sensL;
    juce::Rectangle<int> meterArea;
};
} // namespace moshdeck
