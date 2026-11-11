#include "PluginEditor.h"

using namespace moshdeck;

MoshDeckEditor::MoshDeckEditor (MoshDeckProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p), top (p), deckA (p, 0), deckB (p, 1), layerList (p), inspector (p),
      modPanel (p), midiPanel (p), audioPanel (p), preview (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (top);
    addAndMakeVisible (deckA);
    addAndMakeVisible (deckB);
    addAndMakeVisible (layerList);
    addAndMakeVisible (inspector);
    tabs.addTab ("Modulation Matrix", theme::panel, &modPanel, false);
    tabs.addTab ("MIDI Learn / Out", theme::panel, &midiPanel, false);
    tabs.addTab ("Audio Analysis", theme::panel, &audioPanel, false);
    tabs.setTabBarDepth (26);
    addAndMakeVisible (tabs);
    addAndMakeVisible (preview);
    addChildComponent (thumb);
    preview.onDoubleClick = [this] { proc.openOutputWindow (1, true); updatePreviewMode(); };

    layerList.onSelect = [this] (const juce::String& uid) { inspector.setLayer (uid); };
    top.onOutputToggled = [this] { updatePreviewMode(); };

    setResizable (true, true);
    setResizeLimits (1100, 700, 3000, 2000);
    setSize (1500, 900);
    updatePreviewMode();
    lastVersion = proc.scene.getVersion();
    startTimerHz (10);
}

MoshDeckEditor::~MoshDeckEditor()
{
    preview.stop();
    setLookAndFeel (nullptr);
}

void MoshDeckEditor::updatePreviewMode()
{
    const bool out = proc.isOutputWindowOpen();
    if (out) preview.stop(); else preview.start();
    preview.setVisible (! out);
    thumb.setVisible (out);
    if (auto* e = proc.getOutputEngine()) e->requestThumbnail (true);
}

void MoshDeckEditor::timerCallback()
{
    const auto v = proc.scene.getVersion();
    if (v != lastVersion)
    {
        lastVersion = v;
        layerList.refresh();
        inspector.rebuild();
        modPanel.rebuild();
        deckA.refresh();
        deckB.refresh();
    }
    if (proc.isOutputWindowOpen() != thumb.isVisible())
        updatePreviewMode();
    if (thumb.isVisible())
        if (auto* e = proc.getOutputEngine())
        {
            e->requestThumbnail (true);
            if (e->getThumbnail (thumb.img)) thumb.repaint();
        }
}

void MoshDeckEditor::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void MoshDeckEditor::resized()
{
    auto r = getLocalBounds();
    top.setBounds (r.removeFromTop (44));
    r.reduce (4, 4);
    auto left = r.removeFromLeft (300);
    deckA.setBounds (left.removeFromTop (left.getHeight() / 2));
    deckB.setBounds (left);
    auto right = r.removeFromRight (400);
    layerList.setBounds (right.removeFromTop (juce::jmin (280, right.getHeight() / 3)));
    inspector.setBounds (right);
    auto centre = r.reduced (4, 0);
    const int previewH = juce::jmin (centre.getHeight() - 220, (int) (centre.getWidth() * 9.0 / 16.0));
    auto pv = centre.removeFromTop (previewH);
    preview.setBounds (pv);
    thumb.setBounds (pv);
    centre.removeFromTop (4);
    tabs.setBounds (centre);
}
