#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Panels.h"
#include "ui/CrossfaderPanel.h"
#include "ui/Theme.h"
#include "ui/VideoView.h"

class MoshDeckEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MoshDeckEditor (MoshDeckProcessor&);
    ~MoshDeckEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updatePreviewMode();

    struct Thumb : juce::Component
    {
        juce::Image img;
        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black);
            if (img.isValid()) g.drawImageWithin (img, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::centred);
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawText ("Output window active - preview", getLocalBounds().reduced (6), juce::Justification::topLeft);
        }
    };

    MoshDeckProcessor& proc;
    moshdeck::theme::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    moshdeck::TopBar top;
    moshdeck::CrossfaderPanel xfPanel;
    moshdeck::DeckPanel deckA, deckB;
    moshdeck::LayerListPanel layerList;
    moshdeck::InspectorPanel inspector;
    moshdeck::ModMatrixPanel modPanel;
    moshdeck::MidiPanel midiPanel;
    moshdeck::AudioPanel audioPanel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    moshdeck::VideoView preview;
    Thumb thumb;
    uint32_t lastVersion = 0;
};
