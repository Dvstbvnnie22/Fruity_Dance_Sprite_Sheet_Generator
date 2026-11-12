#include "Panels.h"
#include "Theme.h"
#include "PluginProcessor.h"
#include "render/EffectLibrary.h"
#include "render/ShadertoyImporter.h"
#include "milk/MilkPreset.h"

namespace moshdeck
{
static void styleHeader (juce::Label& l, float size = 14.0f)
{
    l.setFont (juce::FontOptions (size, juce::Font::bold));
    l.setColour (juce::Label::textColourId, theme::text);
}

static void panelBg (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r.toFloat().reduced (2), 6.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r.toFloat().reduced (2), 6.0f, 1.0f);
}

//==============================================================================
TopBar::TopBar (MoshDeckProcessor& p) : proc (p)
{
    title.setText ("MOSHDECK VJ", juce::dontSendNotification);
    title.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, theme::accent);
    addAndMakeVisible (title);

    bpm.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    bpm.setColour (juce::Label::textColourId, theme::accent2);
    addAndMakeVisible (bpm);

    syncMode.addItem ("Sync: FL Host", 1);
    syncMode.addItem ("Sync: Auto-detect", 2);
    syncMode.addItem ("Sync: Tap", 3);
    syncMode.setSelectedId (proc.clock.mode.load() + 1, juce::dontSendNotification);
    syncMode.onChange = [this] { proc.clock.mode = syncMode.getSelectedId() - 1; };
    syncMode.setTooltip ("Where the beat clock comes from. FL Host follows FL Studio's tempo & song position.");
    addAndMakeVisible (syncMode);

    tap.onClick = [this] { proc.clock.tap(); proc.clock.mode = (int) BeatClock::Mode::Tap; syncMode.setSelectedId (3, juce::dontSendNotification); };
    nudgeBack.onClick = [this] { proc.clock.nudge (-0.05); };
    nudgeFwd.onClick = [this] { proc.clock.nudge (0.05); };
    nudgeBack.setTooltip ("Nudge beat phase back"); nudgeFwd.setTooltip ("Nudge beat phase forward");
    addAndMakeVisible (tap); addAndMakeVisible (nudgeBack); addAndMakeVisible (nudgeFwd);

    const std::pair<int, int> sizes[] = { { 640, 360 }, { 960, 540 }, { 1280, 720 }, { 1920, 1080 }, { 1080, 1080 }, { 1080, 1920 } };
    int i = 1;
    for (auto s : sizes)
    {
        canvas.addItem (juce::String (s.first) + " x " + juce::String (s.second), i);
        if (s.first == proc.canvasW.load() && s.second == proc.canvasH.load()) canvas.setSelectedId (i, juce::dontSendNotification);
        ++i;
    }
    canvas.onChange = [this, sizes] { const auto s = sizes[canvas.getSelectedId() - 1]; proc.setCanvasSize (s.first, s.second); };
    canvas.setTooltip ("Render resolution");
    addAndMakeVisible (canvas);

    const auto& displays = juce::Desktop::getInstance().getDisplays().displays;
    for (int d = 0; d < displays.size(); ++d)
        display.addItem ("Display " + juce::String (d + 1) + (displays.getReference (d).isMain ? " (main)" : "")
                         + " " + juce::String (displays.getReference (d).totalArea.getWidth()) + "x" + juce::String (displays.getReference (d).totalArea.getHeight()), d + 1);
    display.setSelectedId (displays.size() > 1 ? 2 : 1, juce::dontSendNotification);
    addAndMakeVisible (display);

    output.setClickingTogglesState (false);
    output.onClick = [this]
    {
        if (proc.isOutputWindowOpen()) proc.closeOutputWindow();
        else proc.openOutputWindow (display.getSelectedId() - 1, false);
        if (onOutputToggled) onOutputToggled();
    };
    fullscreen.onClick = [this]
    {
        proc.openOutputWindow (display.getSelectedId() - 1, true);
        if (onOutputToggled) onOutputToggled();
    };
    addAndMakeVisible (output); addAndMakeVisible (fullscreen);

    master = std::make_unique<ParamSlider> (proc, Scene::masterId, "Master");
    addAndMakeVisible (*master);

    save.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save scene", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mdvj");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (auto xml = proc.saveScenePreset().createXml()) xml->writeTo (f.withFileExtension ("mdvj"));
        });
    };
    load.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load scene", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mdvj");
        chooser->launchAsync (juce::FileBrowserComponent::openMode, [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (auto xml = juce::XmlDocument::parse (f)) proc.loadScenePreset (juce::ValueTree::fromXml (*xml));
        });
    };
    addAndMakeVisible (save); addAndMakeVisible (load);
    startTimerHz (30);
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (6, 6);
    title.setBounds (r.removeFromLeft (150));
    bpm.setBounds (r.removeFromLeft (150));
    r.removeFromLeft (40); // beat lights painted
    syncMode.setBounds (r.removeFromLeft (150).reduced (2));
    tap.setBounds (r.removeFromLeft (46).reduced (2));
    nudgeBack.setBounds (r.removeFromLeft (26).reduced (2));
    nudgeFwd.setBounds (r.removeFromLeft (26).reduced (2));
    r.removeFromLeft (8);
    master->setBounds (r.removeFromLeft (180).reduced (2, 4));
    load.setBounds (r.removeFromRight (90).reduced (2));
    save.setBounds (r.removeFromRight (90).reduced (2));
    fullscreen.setBounds (r.removeFromRight (90).reduced (2));
    output.setBounds (r.removeFromRight (110).reduced (2));
    display.setBounds (r.removeFromRight (190).reduced (2));
    canvas.setBounds (r.removeFromRight (120).reduced (2));
}

void TopBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    // 4 beat lights
    const int x0 = 306;
    const int beatInBar = (int) std::floor (beatPhase) % 4;
    for (int i = 0; i < 4; ++i)
    {
        const bool on = i == beatInBar;
        g.setColour (on ? (i == 0 ? theme::accent : theme::accent2) : theme::outline);
        g.fillEllipse ((float) (x0 + i * 9), getHeight() * 0.5f - 3.5f, 7.0f, 7.0f);
    }
}

void TopBar::timerCallback()
{
    const double b = proc.clock.lastShownBpm.load();
    playing = proc.clock.lastShownPlaying.load();
    AudioFeatures f;
    proc.analyzer.read (f);
    juce::String s = juce::String (b, 1) + " BPM" + (playing ? "  >" : "  ||");
    if (f.detectedBpm > 0) s << "  (det " << juce::String (f.detectedBpm, 0) << ")";
    bpm.setText (s, juce::dontSendNotification);
    double bp = 0; bool pl = false; int bpb = 4;
    juce::ignoreUnused (bp, pl, bpb);
    beatPhase = std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.001 * b / 60.0, 4.0);
    output.setButtonText (proc.isOutputWindowOpen() ? "Close Output" : "Output Window");
    repaint (300, 0, 40, getHeight());
}

//==============================================================================
DeckPanel::DeckPanel (MoshDeckProcessor& p, int d) : proc (p), deck (d)
{
    title.setText (d == 0 ? "DECK A" : "DECK B", juce::dontSendNotification);
    styleHeader (title);
    title.setColour (juce::Label::textColourId, d == 0 ? theme::accent : theme::accent2);
    addAndMakeVisible (title);
    status.setFont (juce::FontOptions (11.0f));
    status.setColour (juce::Label::textColourId, theme::dim);
    addAndMakeVisible (status);

    auto choose = [this] (bool append)
    {
        chooser = std::make_unique<juce::FileChooser> ("Load media for Deck " + juce::String (deck == 0 ? "A" : "B"),
                                                       juce::File::getSpecialLocation (juce::File::userMoviesDirectory), MediaDeck::supportedWildcard());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this, append] (const juce::FileChooser& fc)
        {
            juce::StringArray files;
            for (auto& f : fc.getResults()) files.add (f.getFullPathName());
            if (! files.isEmpty()) { proc.loadDeckFiles (deck, files, append); refresh(); }
        });
    };
    loadBtn.onClick = [choose] { choose (false); };
    addBtn.onClick = [choose] { choose (true); };
    prev.onClick = [this] { proc.nextDeckClip (deck, -1); refresh(); };
    next.onClick = [this] { proc.nextDeckClip (deck, 1); refresh(); };
    clear.onClick = [this] { proc.scene.setDeck (deck, {}); proc.decks[deck].load ({}); refresh(); };
    playlist.onChange = [this] { if (playlist.getSelectedId() > 0) proc.selectDeckClip (deck, playlist.getSelectedId() - 1); };
    for (auto* b : { &loadBtn, &addBtn, &prev, &next, &clear }) addAndMakeVisible (b);
    addAndMakeVisible (playlist);

    const juce::String pre = d == 0 ? "DA" : "DB";
    params = std::make_unique<ParamPanel> (proc, juce::StringArray { pre + ".speed", pre + ".beats", pre + ".cue", pre + ".next" });
    addAndMakeVisible (*params);
    refresh();
    startTimerHz (4);
}

void DeckPanel::refresh()
{
    const auto st = proc.scene.getDeck (deck);
    playlist.clear (juce::dontSendNotification);
    for (int i = 0; i < st.files.size(); ++i)
        playlist.addItem (juce::String (i + 1) + ". " + juce::File (st.files[i]).getFileName(), i + 1);
    if (! st.files.isEmpty()) playlist.setSelectedId (st.current + 1, juce::dontSendNotification);
    playlist.setTextWhenNothingSelected ("Drop media files here");
}

void DeckPanel::filesDropped (const juce::StringArray& files, int, int)
{
    proc.loadDeckFiles (deck, files, true);
    refresh();
}

void DeckPanel::timerCallback()
{
    auto& d = proc.decks[deck];
    auto s = d.getStatus();
    if (d.isVideo()) s << "  " << juce::String (d.getPosition(), 1) << "s";
    status.setText (s, juce::dontSendNotification);
    const auto st = proc.scene.getDeck (deck);
    if (playlist.getNumItems() != st.files.size() || (st.files.size() > 0 && playlist.getSelectedId() != st.current + 1))
        refresh();
}

void DeckPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (22);
    title.setBounds (top.removeFromLeft (70));
    clear.setBounds (top.removeFromRight (46).reduced (1));
    next.setBounds (top.removeFromRight (26).reduced (1));
    prev.setBounds (top.removeFromRight (26).reduced (1));
    addBtn.setBounds (top.removeFromRight (46).reduced (1));
    loadBtn.setBounds (top.removeFromRight (56).reduced (1));
    r.removeFromTop (3);
    playlist.setBounds (r.removeFromTop (24));
    status.setBounds (r.removeFromTop (18));
    params->setBounds (r.removeFromTop (params->getIdealHeight()));
}

void DeckPanel::paint (juce::Graphics& g) { panelBg (g, getLocalBounds()); }

//==============================================================================
LayerListPanel::LayerListPanel (MoshDeckProcessor& p) : proc (p)
{
    list.setModel (this);
    list.setRowHeight (26);
    addAndMakeVisible (list);
    add.onClick = [this]
    {
        juce::PopupMenu m;
        buildAddMenu (m);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&add), [this] (int r)
        {
            if (handleAddMenu (proc, r))
            {
                const auto ls = proc.scene.getLayers();
                if (! ls.empty()) setSelected (ls.back().uid);
            }
        });
    };
    remove.onClick = [this] { if (selected.isNotEmpty() && selected != "master") { proc.scene.removeLayer (selected); selected.clear(); refresh(); } };
    up.onClick = [this] { if (selected.isNotEmpty()) proc.scene.moveLayer (selected, 1); };
    down.onClick = [this] { if (selected.isNotEmpty()) proc.scene.moveLayer (selected, -1); };
    dup.onClick = [this] { if (selected.isNotEmpty() && selected != "master") proc.scene.duplicateLayer (selected); };
    for (auto* b : { &add, &remove, &up, &down, &dup }) addAndMakeVisible (b);
    refresh();
}

void LayerListPanel::buildAddMenu (juce::PopupMenu& m)
{
    juce::PopupMenu media, gens, viz, templates;
    media.addItem (1, "Deck A");
    media.addItem (2, "Deck B");
    media.addItem (3, "Deck A/B Mix (crossfader)");
    media.addItem (4, "Layers Below (adjustment layer)");
    std::map<juce::String, juce::PopupMenu> cats;
    for (int i = 0; i < GeneratorLibrary::count(); ++i)
        cats[GeneratorLibrary::get (i)->category].addItem (100 + i, GeneratorLibrary::get (i)->name);
    for (auto& c : cats) gens.addSubMenu (c.first, c.second);
    for (int i = 0; i < numVizTypes; ++i) viz.addItem (200 + i, vizTypeName (i));
    templates.addItem (500, "Circle spectrum + cover image + particles");
    templates.addItem (501, "Bars + waveform + peaks");
    templates.addItem (502, "Pulse cover + polygon ring + glow");
    m.addSubMenu ("Media track", media);
    m.addItem (300, "Milk preset (MilkDrop-format feedback visuals)");
    m.addSubMenu ("Generator", gens);
    m.addSubMenu ("Audio visualizer element", viz);
    m.addSubMenu ("Visualizer template (adds several layers)", templates);
    m.addItem (400, "Shadertoy shader");
}

bool LayerListPanel::handleAddMenu (MoshDeckProcessor& p, int r, const juce::String& replace)
{
    if (r <= 0) return false;
    auto make = [&] (SourceType t, int idx) -> juce::String
    {
        if (replace.isNotEmpty()) { p.scene.setLayerSource (replace, t, idx); return replace; }
        return p.scene.addLayer (t, idx);
    };
    if (r >= 1 && r <= 4)
    {
        const SourceType t[] = { SourceType::DeckA, SourceType::DeckB, SourceType::DeckMix, SourceType::Below };
        make (t[r - 1], 0);
    }
    else if (r >= 100 && r < 200) make (SourceType::Generator, r - 100);
    else if (r >= 200 && r < 300) make (SourceType::AudioViz, r - 200);
    else if (r == 300) make (SourceType::Milk, 0);
    else if (r == 400) make (SourceType::Shadertoy, 0);
    else if (r >= 500 && replace.isEmpty())
    {
        auto viz = [&] (int type, InputRoute in, float size, float x, float y, float hue)
        {
            const auto u = p.scene.addLayer (SourceType::AudioViz, type);
            p.scene.editLayer (u, [&] (Layer& l) { l.input = in; l.blendMode = (int) BlendMode::AlphaOver; });
            p.scene.setValue (u + ".p0", size); p.scene.setValue (u + ".p1", x); p.scene.setValue (u + ".p2", y); p.scene.setValue (u + ".p4", hue);
            return u;
        };
        if (r == 500) { viz (6, InputRoute::None, 1.2f, 0, 0, 0.6f); viz (2, InputRoute::DeckA, 1.0f, 0, 0, 0.55f); }
        if (r == 501) { viz (8, InputRoute::None, 1.4f, 0, -0.6f, 0.75f); viz (4, InputRoute::None, 1.6f, 0, 0.35f, 0.5f); }
        if (r == 502) { viz (11, InputRoute::None, 1.4f, 0, 0, 0.9f); viz (9, InputRoute::None, 1.2f, 0, 0, 0.1f); viz (7, InputRoute::DeckB, 1.0f, 0, 0, 0.0f); }
    }
    return true;
}

void LayerListPanel::refresh()
{
    auto ls = proc.scene.getLayers();
    layers.assign (ls.rbegin(), ls.rend());
    list.updateContent();
    list.repaint();
}

void LayerListPanel::setSelected (const juce::String& uid)
{
    selected = uid;
    refresh();
    if (onSelect) onSelect (uid);
}

int LayerListPanel::getNumRows() { return (int) layers.size() + 1; }

void LayerListPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    const bool isMaster = row == (int) layers.size();
    const auto uid = isMaster ? juce::String ("master") : layers[(size_t) row].uid;
    const bool sel = uid == selected;
    g.setColour (sel ? theme::accent.withAlpha (0.35f) : (row % 2 ? theme::panel : theme::panelHi));
    g.fillRect (0, 0, w, h);
    if (isMaster)
    {
        g.setColour (theme::accent2);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("MASTER FX", 30, 0, w - 34, h, juce::Justification::centredLeft);
        return;
    }
    const auto& l = layers[(size_t) row];
    g.setColour (l.enabled ? theme::accent2 : theme::outline);
    g.fillEllipse (8, h * 0.5f - 5, 10, 10);
    if (l.solo) { g.setColour (theme::mod); g.drawText ("S", w - 20, 0, 16, h, juce::Justification::centred); }
    g.setColour (l.enabled ? theme::text : theme::dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (l.name, 26, 0, w - 140, h, juce::Justification::centredLeft);
    g.setColour (theme::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (blendModeName ((BlendMode) l.blendMode) + (l.effects.empty() ? juce::String() : "  fx:" + juce::String ((int) l.effects.size())),
                w - 140, 0, 116, h, juce::Justification::centredRight);
}

void LayerListPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    const bool isMaster = row == (int) layers.size();
    const auto uid = isMaster ? juce::String ("master") : layers[(size_t) row].uid;
    if (! isMaster && e.x < 24)
        proc.scene.editLayer (uid, [] (Layer& l) { l.enabled = ! l.enabled; });
    setSelected (uid);
}

void LayerListPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto b = r.removeFromTop (26);
    const int w = b.getWidth() / 5;
    for (auto* btn : { &add, &dup, &up, &down, &remove }) btn->setBounds (b.removeFromLeft (w).reduced (1));
    r.removeFromTop (4);
    list.setBounds (r);
}

void LayerListPanel::paint (juce::Graphics& g) { panelBg (g, getLocalBounds()); }

//==============================================================================
class ShaderEditorWindow : public juce::DocumentWindow
{
public:
    ShaderEditorWindow (MoshDeckProcessor& p, const juce::String& layerUid)
        : juce::DocumentWindow ("Shadertoy Editor", theme::bg, juce::DocumentWindow::closeButton), proc (p), uid (layerUid)
    {
        setUsingNativeTitleBar (true);
        auto* c = new Content (*this);
        setContentOwned (c, false);
        setResizable (true, false);
        centreWithSize (900, 640);
        setVisible (true);
    }
    void closeButtonPressed() override { setVisible (false); }

private:
    struct Content : juce::Component
    {
        explicit Content (ShaderEditorWindow& w) : owner (w), editor (doc, &tokeniser)
        {
            Layer l;
            if (owner.proc.scene.getLayer (owner.uid, l))
                doc.replaceAllContent (l.shaderCode.isEmpty() ? ShadertoyProject::defaultShader() : l.shaderCode);
            editor.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
            addAndMakeVisible (editor);
            apply.onClick = [this]
            {
                const auto code = doc.getAllContent();
                owner.proc.scene.editLayer (owner.uid, [&] (Layer& l) { l.shaderCode = code; });
            };
            reset.onClick = [this] { doc.replaceAllContent (ShadertoyProject::defaultShader()); };
            for (auto* b : { &apply, &reset }) addAndMakeVisible (b);
            status.setColour (juce::Label::textColourId, theme::dim);
            status.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (status);
            startTimer (300);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            auto b = r.removeFromBottom (90);
            auto br = b.removeFromTop (28);
            apply.setBounds (br.removeFromLeft (160).reduced (2));
            reset.setBounds (br.removeFromLeft (160).reduced (2));
            status.setBounds (b);
            editor.setBounds (r);
        }
        void startTimer (int ms) { timer.cb = [this] { status.setText (owner.proc.getLayerStatus (owner.uid), juce::dontSendNotification); }; timer.startTimer (ms); }
        struct T : juce::Timer { std::function<void()> cb; void timerCallback() override { cb(); } } timer;
        ShaderEditorWindow& owner;
        juce::CodeDocument doc;
        juce::CPlusPlusCodeTokeniser tokeniser;
        juce::CodeEditorComponent editor;
        juce::TextButton apply { "Apply / Compile (live)" }, reset { "Reset to template" };
        juce::Label status;
    };
    MoshDeckProcessor& proc;
    juce::String uid;
};

InspectorPanel::InspectorPanel (MoshDeckProcessor& p) : proc (p)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    startTimerHz (4);
}

InspectorPanel::~InspectorPanel() = default;

void InspectorPanel::setLayer (const juce::String& u)
{
    if (u != uid) shaderWindow.reset();
    uid = u;
    rebuild();
}

void InspectorPanel::timerCallback()
{
    if (statusLabel != nullptr && uid.isNotEmpty())
    {
        const auto s = proc.getLayerStatus (uid);
        if (s != statusLabel->getText()) statusLabel->setText (s, juce::dontSendNotification);
    }
}

void InspectorPanel::rebuild()
{
    const int scrollY = viewport.getViewPositionY();
    rows.clear();
    content.removeAllChildren();
    owned.clear();
    statusLabel = nullptr;

    auto addRow = [this] (juce::Component* c, int h) { owned.add (c); content.addAndMakeVisible (c); rows.push_back ({ c, h }); return c; };
    auto header = [&] (const juce::String& text, juce::Colour col)
    {
        auto* l = new juce::Label ({}, text);
        styleHeader (*l, 15.0f);
        l->setColour (juce::Label::textColourId, col);
        return addRow (l, 26);
    };

    const bool isMaster = uid == "master";
    Layer layer;
    if (! isMaster && (uid.isEmpty() || ! proc.scene.getLayer (uid, layer)))
    {
        auto* l = new juce::Label ({}, "Select a layer on the left (or MASTER FX).\n\nTips:\n- Right-click any slider to MIDI-learn it, modulate it with audio/beat/LFO/MIDI, or link it to another parameter.\n- Layers blend bottom to top. Generators, Milk presets, visualizers and Shadertoy shaders use their 'Input' (deck A, B, mix or layers below) as their base picture.");
        l->setJustificationType (juce::Justification::topLeft);
        l->setColour (juce::Label::textColourId, theme::dim);
        addRow (l, 220);
        layoutContent();
        return;
    }

    if (! isMaster)
    {
        header (layer.name, theme::accent);

        // ---- source chooser
        auto* srcBtn = new juce::TextButton ("Source: " + sourceTypeName (layer.source)
                                             + (layer.source == SourceType::Generator && GeneratorLibrary::get (layer.sourceIndex) ? " / " + GeneratorLibrary::get (layer.sourceIndex)->name
                                                : layer.source == SourceType::AudioViz ? " / " + vizTypeName (layer.sourceIndex) : juce::String()) + "  (change...)");
        srcBtn->onClick = [this, srcBtn]
        {
            juce::PopupMenu m;
            LayerListPanel::buildAddMenu (m);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (srcBtn), [this] (int r) { LayerListPanel::handleAddMenu (proc, r, uid); });
        };
        addRow (srcBtn, 26);

        // ---- input / blend / flags
        struct ComboRow : juce::Component
        {
            juce::Label label; juce::ComboBox box;
            ComboRow (const juce::String& name) { label.setText (name, juce::dontSendNotification); label.setColour (juce::Label::textColourId, theme::dim); addAndMakeVisible (label); addAndMakeVisible (box); }
            void resized() override { auto r = getLocalBounds().reduced (1); label.setBounds (r.removeFromLeft (r.getWidth() * 45 / 100)); box.setBounds (r); }
        };
        const bool usesInput = layer.source == SourceType::Generator || layer.source == SourceType::Milk
                            || layer.source == SourceType::AudioViz || layer.source == SourceType::Shadertoy;
        if (usesInput)
        {
            auto* in = new ComboRow ("Input (base picture)");
            for (int i = 0; i < (int) InputRoute::NumRoutes; ++i) in->box.addItem (inputRouteName ((InputRoute) i), i + 1);
            in->box.setSelectedId ((int) layer.input + 1, juce::dontSendNotification);
            in->box.onChange = [this, in] { const int v = in->box.getSelectedId() - 1; proc.scene.editLayer (uid, [v] (Layer& l) { l.input = (InputRoute) v; }); };
            addRow (in, 26);
        }
        auto* bl = new ComboRow ("Blend mode");
        for (int i = 0; i < (int) BlendMode::NumModes; ++i) bl->box.addItem (blendModeName ((BlendMode) i), i + 1);
        bl->box.setSelectedId (layer.blendMode + 1, juce::dontSendNotification);
        bl->box.onChange = [this, bl] { const int v = bl->box.getSelectedId() - 1; proc.scene.editLayer (uid, [v] (Layer& l) { l.blendMode = v; }); };
        addRow (bl, 26);

        struct Flags : juce::Component
        {
            juce::ToggleButton en { "Enabled" }, solo { "Solo" };
            Flags() { addAndMakeVisible (en); addAndMakeVisible (solo); }
            void resized() override { auto r = getLocalBounds(); en.setBounds (r.removeFromLeft (r.getWidth() / 2)); solo.setBounds (r); }
        };
        auto* fl = new Flags();
        fl->en.setToggleState (layer.enabled, juce::dontSendNotification);
        fl->solo.setToggleState (layer.solo, juce::dontSendNotification);
        fl->en.onClick = [this, fl] { const bool v = fl->en.getToggleState(); proc.scene.editLayer (uid, [v] (Layer& l) { l.enabled = v; }); };
        fl->solo.onClick = [this, fl] { const bool v = fl->solo.getToggleState(); proc.scene.editLayer (uid, [v] (Layer& l) { l.solo = v; }); };
        addRow (fl, 24);

        addRow (new ParamSlider (proc, uid + ".opacity"), 24);
        if (layer.source != SourceType::DeckA && layer.source != SourceType::DeckB && layer.source != SourceType::DeckMix)
            addRow (new ParamSlider (proc, uid + ".speed"), 24);

        // ---- source specific controls
        if (layer.source == SourceType::Milk)
        {
            const auto folder = layer.milkFolder.isNotEmpty() ? layer.milkFolder : proc.defaultMilkFolder;
            const auto list = proc.milkPresetList (folder);
            auto* pr = new ComboRow ("Preset");
            for (int i = 0; i < list.size(); ++i)
                pr->box.addItem (list[i].startsWith ("builtin:") ? list[i].substring (8) + "  (built-in)" : juce::File (list[i]).getFileNameWithoutExtension(), i + 1);
            pr->box.setSelectedId (list.indexOf (layer.milkPreset) + 1, juce::dontSendNotification);
            pr->box.onChange = [this, pr, list] { const auto s = list[pr->box.getSelectedId() - 1]; proc.scene.editLayer (uid, [s] (Layer& l) { l.milkPreset = s; }); };
            addRow (pr, 26);

            struct Btns : juce::Component
            {
                juce::TextButton prev { "< Prev" }, next { "Next >" }, rnd { "Random" }, folder { "Preset folder..." };
                Btns() { for (auto* b : { &prev, &next, &rnd, &folder }) addAndMakeVisible (b); }
                void resized() override { auto r = getLocalBounds(); const int w = r.getWidth() / 4; for (auto* b : { &prev, &next, &rnd, &folder }) b->setBounds (r.removeFromLeft (w).reduced (1)); }
            };
            auto* b = new Btns();
            b->prev.onClick = [this] { proc.stepMilkPreset (uid, -1); };
            b->next.onClick = [this] { proc.stepMilkPreset (uid, 1); };
            b->rnd.onClick = [this] { proc.stepMilkPreset (uid, 1, true); };
            b->folder.setTooltip ("Pick a folder of .milk presets (e.g. your MilkDrop / NestDrop preset folders). Sub-folders are scanned.");
            b->folder.onClick = [this]
            {
                chooser = std::make_unique<juce::FileChooser> ("Choose a folder containing .milk presets");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (! f.isDirectory()) return;
                    proc.defaultMilkFolder = f.getFullPathName();
                    proc.scene.editLayer (uid, [f] (Layer& l) { l.milkFolder = f.getFullPathName(); });
                });
            };
            addRow (b, 26);
        }
        if (layer.source == SourceType::Shadertoy)
        {
            struct Btns : juce::Component
            {
                juce::TextButton edit { "Edit code" }, file { "Import file..." }, web { "Fetch by ID..." };
                Btns() { for (auto* b : { &edit, &file, &web }) addAndMakeVisible (b); }
                void resized() override { auto r = getLocalBounds(); const int w = r.getWidth() / 3; for (auto* b : { &edit, &file, &web }) b->setBounds (r.removeFromLeft (w).reduced (1)); }
            };
            auto* b = new Btns();
            b->edit.onClick = [this] { shaderWindow = std::make_unique<ShaderEditorWindow> (proc, uid); };
            b->file.setTooltip ("Import a .glsl / .frag / .txt with mainImage(), or a Shadertoy JSON export (multipass Buffer A-D supported)");
            b->file.onClick = [this]
            {
                chooser = std::make_unique<juce::FileChooser> ("Import Shadertoy shader", juce::File(), "*.glsl;*.frag;*.fs;*.txt;*.json;*.shader");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (! f.existsAsFile()) return;
                    const auto code = f.loadFileAsString();
                    proc.scene.editLayer (uid, [&] (Layer& l) { l.shaderCode = code; l.shaderName = f.getFileNameWithoutExtension(); });
                });
            };
            b->web.onClick = [this]
            {
                auto* w = new juce::AlertWindow ("Fetch from shadertoy.com", "Enter a shader ID or URL and your Shadertoy API key\n(shadertoy.com > Profile > Apps). Only shaders published as 'Public + API' can be fetched.", juce::MessageBoxIconType::NoIcon);
                w->addTextEditor ("id", "", "Shader ID / URL");
                w->addTextEditor ("key", "", "API key");
                w->addButton ("Fetch", 1);
                w->addButton ("Cancel", 0);
                juce::Component::SafePointer<InspectorPanel> safe (this);
                const auto layerUid = uid;
                w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, layerUid] (int r)
                {
                    if (r != 1 || safe == nullptr) return;
                    const auto id = w->getTextEditorContents ("id"), key = w->getTextEditorContents ("key");
                    auto* proc = &safe->proc;
                    juce::Thread::launch ([proc, id, key, layerUid]
                    {
                        juce::String err;
                        const auto json = ShadertoyProject::fetchFromWeb (id, key, err);
                        juce::MessageManager::callAsync ([proc, json, err, layerUid]
                        {
                            if (json.isEmpty()) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Shadertoy", err); return; }
                            proc->scene.editLayer (layerUid, [&] (Layer& l) { l.shaderCode = json; });
                        });
                    });
                }), true);
            };
            addRow (b, 26);
        }

        auto* st = new juce::Label();
        st->setFont (juce::FontOptions (11.0f));
        st->setColour (juce::Label::textColourId, theme::dim);
        st->setJustificationType (juce::Justification::topLeft);
        statusLabel = st;
        addRow (st, layer.source == SourceType::Shadertoy || layer.source == SourceType::Milk ? 64 : 4);

        // ---- source params
        const auto defs = sourceParamDefs (layer.source, layer.sourceIndex);
        if (! defs.empty())
        {
            header ("Source parameters", theme::accent2);
            for (auto& d : defs) addRow (new ParamSlider (proc, uid + "." + d.key), 24);
        }
    }
    else
        header ("MASTER FX (applied to the final mix)", theme::accent2);

    // ---- effects rack
    const juce::String owner = isMaster ? juce::String ("master") : uid;
    {
        struct FxHeader : juce::Component
        {
            juce::Label l; juce::TextButton add { "+ Add effect" };
            FxHeader() { l.setText ("Effects (top runs first)", juce::dontSendNotification); styleHeader (l); l.setColour (juce::Label::textColourId, theme::accent2); addAndMakeVisible (l); addAndMakeVisible (add); }
            void resized() override { auto r = getLocalBounds(); add.setBounds (r.removeFromRight (110).reduced (1)); l.setBounds (r); }
        };
        auto* fh = new FxHeader();
        fh->add.onClick = [this, fh, owner]
        {
            juce::PopupMenu m;
            const auto& all = EffectLibrary::all();
            for (auto& cat : EffectLibrary::categories())
            {
                juce::PopupMenu sub;
                for (size_t i = 0; i < all.size(); ++i)
                    if (all[i].category == cat) sub.addItem ((int) i + 1, all[i].name);
                m.addSubMenu (cat, sub);
            }
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fh->add), [this, owner] (int r)
            {
                if (r > 0) proc.scene.addEffect (owner, EffectLibrary::all()[(size_t) r - 1].id);
            });
        };
        addRow (fh, 28);
    }
    for (auto& fx : proc.scene.getEffects (owner))
    {
        auto* def = EffectLibrary::find (fx.effectId);
        if (def == nullptr) continue;
        struct FxRow : juce::Component
        {
            juce::ToggleButton en; juce::Label name; juce::TextButton up { "^" }, down { "v" }, del { "x" };
            FxRow() { addAndMakeVisible (en); addAndMakeVisible (name); for (auto* b : { &up, &down, &del }) addAndMakeVisible (b); }
            void paint (juce::Graphics& g) override { g.setColour (theme::panelHi); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f); }
            void resized() override
            {
                auto r = getLocalBounds().reduced (2);
                en.setBounds (r.removeFromLeft (24));
                del.setBounds (r.removeFromRight (24).reduced (1));
                down.setBounds (r.removeFromRight (24).reduced (1));
                up.setBounds (r.removeFromRight (24).reduced (1));
                name.setBounds (r);
            }
        };
        auto* row = new FxRow();
        row->en.setToggleState (fx.enabled, juce::dontSendNotification);
        row->name.setText (def->name + "  [" + def->category + "]", juce::dontSendNotification);
        row->name.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        if (def->description.isNotEmpty()) row->name.setTooltip (def->description);
        const auto fxUid = fx.uid;
        row->en.onClick = [this, row, owner, fxUid] { proc.scene.setEffectEnabled (owner, fxUid, row->en.getToggleState()); };
        row->up.onClick = [this, owner, fxUid] { proc.scene.moveEffect (owner, fxUid, -1); };
        row->down.onClick = [this, owner, fxUid] { proc.scene.moveEffect (owner, fxUid, 1); };
        row->del.onClick = [this, owner, fxUid] { proc.scene.removeEffect (owner, fxUid); };
        addRow (row, 26);
        addRow (new ParamSlider (proc, owner + "." + fx.uid + ".mix", "Mix"), 24);
        for (auto& p : def->params)
            addRow (new ParamSlider (proc, owner + "." + fx.uid + "." + p.key), 24);
    }
    layoutContent();
    viewport.setViewPosition (0, scrollY);
}

void InspectorPanel::layoutContent()
{
    const int w = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness());
    int y = 0;
    for (auto& r : rows) { r.c->setBounds (4, y, w - 8, r.h); y += r.h + 2; }
    content.setSize (w, y + 10);
}

void InspectorPanel::resized()
{
    viewport.setBounds (getLocalBounds().reduced (6));
    layoutContent();
}

void InspectorPanel::paint (juce::Graphics& g) { panelBg (g, getLocalBounds()); }

//==============================================================================
ModMatrixPanel::ModMatrixPanel (MoshDeckProcessor& p) : proc (p)
{
    add.onClick = [this]
    {
        juce::PopupMenu src;
        ModSourceMenu::add (src, 1000);
        const auto params = proc.scene.getAllParams();
        juce::PopupMenu link;
        ParamMenu::add (link, params, 100000);
        src.addSubMenu ("Another parameter (link)", link);
        src.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&add), [this, params] (int r)
        {
            ModRoute route;
            if (r >= 100000) { route.source = ModSourceType::Param; route.sourceParam = params[(size_t) (r - 100000)].id; }
            else if (! ModSourceMenu::decode (r, 1000, route.source, route.sourceIndex)) return;
            juce::PopupMenu dest;
            ParamMenu::add (dest, params, 1, route.sourceParam);
            dest.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&add), [this, route, params] (int d) mutable
            {
                if (d <= 0) return;
                route.dest = params[(size_t) (d - 1)].id;
                proc.scene.addRoute (route, 0.3f);
            });
        });
    };
    lfoBtn.onClick = [this]
    {
        juce::StringArray ids;
        for (int i = 1; i <= 4; ++i) { ids.add ("G.lfo" + juce::String (i) + ".div"); ids.add ("G.lfo" + juce::String (i) + ".shape"); }
        auto* panel = new ParamPanel (proc, ids);
        panel->setSize (380, panel->getIdealHeight());
        juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (panel), lfoBtn.getScreenBounds(), nullptr);
    };
    hint.setText ("Every parameter can be modulated by audio, beat, LFOs, MIDI, macros or other parameters. Route amounts are parameters too, so routes can modulate routes.", juce::dontSendNotification);
    hint.setFont (juce::FontOptions (11.0f));
    hint.setColour (juce::Label::textColourId, theme::dim);
    addAndMakeVisible (add); addAndMakeVisible (lfoBtn); addAndMakeVisible (hint);
    viewport.setViewedComponent (&content, false);
    addAndMakeVisible (viewport);
    rebuild();
}

void ModMatrixPanel::rebuild()
{
    content.removeAllChildren();
    owned.clear();
    const auto routes = proc.scene.getRoutes();
    int y = 0;
    const int w = juce::jmax (300, viewport.getWidth() - 12);
    for (auto& r : routes)
    {
        Param dest;
        proc.scene.getParam (r.dest, dest);
        auto* l = new juce::Label ({}, modSourceName (r.source, r.sourceIndex, r.sourceParam) + "  ->  " + dest.group + " / " + dest.name);
        l->setFont (juce::FontOptions (12.0f));
        owned.add (l); content.addAndMakeVisible (l);
        l->setBounds (0, y, w * 45 / 100, 24);

        auto* amt = new ParamSlider (proc, r.uid + ".amt", "Amount");
        owned.add (amt); content.addAndMakeVisible (amt);
        amt->setBounds (w * 45 / 100, y, w * 30 / 100, 24);

        auto* sm = new juce::Slider (juce::Slider::LinearBar, juce::Slider::NoTextBox);
        sm->setRange (0.0, 0.99, 0.01);
        sm->setValue (r.smoothing, juce::dontSendNotification);
        sm->setTooltip ("Smoothing");
        const auto route = r;
        sm->onDragEnd = [this, sm, route] { auto x = route; x.smoothing = (float) sm->getValue(); proc.scene.updateRoute (x); };
        owned.add (sm); content.addAndMakeVisible (sm);
        sm->setBounds (w * 75 / 100, y + 2, w * 10 / 100, 20);

        auto* bip = new juce::ToggleButton ("+/-");
        bip->setToggleState (r.bipolar, juce::dontSendNotification);
        bip->setTooltip ("Bipolar: source swings both ways around the base value");
        bip->onClick = [this, bip, route] { auto x = route; x.bipolar = bip->getToggleState(); proc.scene.updateRoute (x); };
        owned.add (bip); content.addAndMakeVisible (bip);
        bip->setBounds (w * 85 / 100, y, w * 9 / 100, 24);

        auto* del = new juce::TextButton ("x");
        del->onClick = [this, route] { proc.scene.removeRoute (route.uid); };
        owned.add (del); content.addAndMakeVisible (del);
        del->setBounds (w * 94 / 100, y + 1, w * 6 / 100, 22);
        y += 26;
    }
    content.setSize (w, y + 4);
}

void ModMatrixPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (26);
    add.setBounds (top.removeFromLeft (100).reduced (1));
    lfoBtn.setBounds (top.removeFromLeft (110).reduced (1));
    hint.setBounds (top);
    viewport.setBounds (r);
    rebuild();
}

//==============================================================================
MidiPanel::MidiPanel (MoshDeckProcessor& p) : proc (p)
{
    monitor.setFont (juce::FontOptions (12.0f));
    monitor.setColour (juce::Label::textColourId, theme::midi);
    learnState.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    learnState.setColour (juce::Label::textColourId, theme::mod);
    addAndMakeVisible (monitor); addAndMakeVisible (learnState);
    clearAll.onClick = [this] { proc.midiLearn.clearAll(); rebuild(); };
    addAndMakeVisible (clearAll);

    const std::pair<const char*, const char*> actions[] = {
        { "A.deckA.next", "Deck A next" }, { "A.deckA.prev", "Deck A prev" }, { "A.deckB.next", "Deck B next" }, { "A.deckB.prev", "Deck B prev" },
        { "A.milk.next", "Milk next" }, { "A.milk.random", "Milk random" }, { "A.tap", "Tap tempo" }, { "A.xfade.cut", "X-fade cut" },
        { "A.nudge.back", "Nudge -" }, { "A.nudge.fwd", "Nudge +" },
        { "A.xfade.reverse", "X-fade reverse" }, { "A.xfade.mode", "X-fade cut/fade" }, { "A.xfade.curve", "X-fade curve" } };
    for (auto& a : actions)
    {
        auto* b = new juce::TextButton (juce::String ("Learn: ") + a.second);
        const juce::String id = a.first;
        b->onClick = [this, id] { proc.midiLearn.arm (id); };
        b->setTooltip ("Click, then press a pad / key on your controller");
        actionButtons.add (b);
        addAndMakeVisible (b);
    }

    // MIDI out controls
    auto& s = proc.midiOut.settings;
    auto toggle = [this] (const char* name, std::atomic<bool>& v)
    {
        auto* t = new juce::ToggleButton (name);
        t->setToggleState (v.load(), juce::dontSendNotification);
        t->onClick = [t, &v] { v = t->getToggleState(); };
        outControls.add (t); addAndMakeVisible (t);
    };
    auto number = [this] (const char* name, std::atomic<int>& v, int mn, int mx)
    {
        auto* sl = new juce::Slider (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);
        sl->setRange (mn, mx, 1);
        sl->setValue (v.load(), juce::dontSendNotification);
        sl->setTextValueSuffix (juce::String (" ") + name);
        sl->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 90, 20);
        sl->onValueChange = [sl, &v] { v = (int) sl->getValue(); };
        outControls.add (sl); addAndMakeVisible (sl);
    };
    toggle ("Onset notes (kick/snare/hat)", s.onsetNotes); number ("ch", s.onsetChannel, 1, 16);
    number ("kick", s.kickNote, 0, 127); number ("snare", s.snareNote, 0, 127); number ("hat", s.hatNote, 0, 127);
    toggle ("Beat / bar notes", s.beatNotes); number ("ch", s.beatChannel, 1, 16); number ("beat", s.beatNote, 0, 127); number ("bar", s.barNote, 0, 127);
    toggle ("Band energy CCs", s.bandCCs); number ("ch", s.ccChannel, 1, 16); number ("first CC", s.ccBase, 0, 123);
    toggle ("MIDI clock out", s.clock); toggle ("Controller feedback", s.feedback); toggle ("MIDI thru", s.thru);

    viewport.setViewedComponent (&content, false);
    addAndMakeVisible (viewport);
    juce::Component::SafePointer<MidiPanel> safeThis (this);
    proc.midiLearn.onMappingsChanged = [safeThis] { juce::MessageManager::callAsync ([safeThis] { if (auto* p = safeThis.getComponent()) p->rebuild(); }); };
    rebuild();
    startTimerHz (10);
}

void MidiPanel::rebuild()
{
    content.removeAllChildren();
    owned.clear();
    const auto maps = proc.midiLearn.getMappings();
    lastCount = maps.size();
    const auto params = proc.scene.getAllParams();
    const int w = juce::jmax (200, viewport.getWidth() - 12);
    int y = 0;
    for (size_t i = 0; i < maps.size(); ++i)
    {
        const auto& m = maps[i];
        juce::String targetName = m.target;
        for (auto& p : params) if (p.id == m.target) { targetName = p.group + " / " + p.name; break; }
        auto* l = new juce::Label ({}, (m.isNote ? "Note " + juce::MidiMessage::getMidiNoteName (m.number, true, true, 3) : "CC " + juce::String (m.number))
                                       + " ch" + juce::String (m.channel + 1) + "  ->  " + targetName);
        l->setFont (juce::FontOptions (12.0f));
        owned.add (l); content.addAndMakeVisible (l);
        l->setBounds (0, y, w - 120, 22);
        auto* tg = new juce::ToggleButton ("toggle");
        tg->setToggleState (m.toggle, juce::dontSendNotification);
        tg->setEnabled (m.isNote && ! m.target.startsWith ("A."));
        const int idx = (int) i;
        tg->onClick = [this, tg, idx] { auto ms = proc.midiLearn.getMappings(); if (idx < (int) ms.size()) { auto x = ms[(size_t) idx]; x.toggle = tg->getToggleState(); proc.midiLearn.setMapping (idx, x); } };
        owned.add (tg); content.addAndMakeVisible (tg);
        tg->setBounds (w - 118, y, 80, 22);
        auto* del = new juce::TextButton ("x");
        del->onClick = [this, idx] { proc.midiLearn.removeMapping (idx); };
        owned.add (del); content.addAndMakeVisible (del);
        del->setBounds (w - 34, y, 30, 22);
        y += 24;
    }
    if (maps.empty())
    {
        auto* l = new juce::Label ({}, "No mappings yet. Right-click any slider > MIDI Learn, then move a knob. Route FL's MIDI to this plugin (Channel settings > MIDI input port) and enable MIDI out via the wrapper's output port.");
        l->setFont (juce::FontOptions (12.0f)); l->setColour (juce::Label::textColourId, theme::dim);
        l->setJustificationType (juce::Justification::topLeft);
        owned.add (l); content.addAndMakeVisible (l);
        l->setBounds (0, 0, w, 60);
        y = 62;
    }
    content.setSize (w, y);
}

void MidiPanel::timerCallback()
{
    const auto ev = proc.midiState.eventCounter.load();
    if (ev != lastEvents)
    {
        lastEvents = ev;
        const int st = proc.midiState.lastStatus.load();
        monitor.setText ("IN: " + juce::String::toHexString (st) + " " + juce::String (proc.midiState.lastData1.load()) + " " + juce::String (proc.midiState.lastData2.load())
                         + "   OUT sent: " + juce::String ((int) proc.midiOut.sentCounter.load()), juce::dontSendNotification);
    }
    const auto armed = proc.midiLearn.getArmed();
    learnState.setText (armed.isEmpty() ? juce::String() : (armed.startsWith ("R.") ? "Move a control to MODULATE: " + armed.substring (2) : "LEARNING: " + armed + "  (move a control)"), juce::dontSendNotification);
}

void MidiPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (22);
    clearAll.setBounds (top.removeFromRight (140).reduced (1));
    monitor.setBounds (top.removeFromLeft (top.getWidth() / 2));
    learnState.setBounds (top);

    auto right = r.removeFromRight (juce::jmin (420, r.getWidth() / 2));
    auto acts = right.removeFromTop (((actionButtons.size() + 1) / 2) * 24);
    for (int i = 0; i < actionButtons.size(); ++i)
        actionButtons[i]->setBounds (acts.getX() + (i % 2) * acts.getWidth() / 2, acts.getY() + (i / 2) * 24, acts.getWidth() / 2 - 2, 22);
    right.removeFromTop (4);
    // midi out grid: rows of controls
    int x = right.getX(), y = right.getY();
    const int rowW = right.getWidth();
    for (auto* c : outControls)
    {
        const bool isToggle = dynamic_cast<juce::ToggleButton*> (c) != nullptr;
        const int cw = isToggle ? rowW * 55 / 100 : rowW * 22 / 100;
        if (x + cw > right.getRight() || (isToggle && x != right.getX())) { x = right.getX(); y += 24; }
        c->setBounds (x, y, cw - 2, 22);
        x += cw;
    }
    viewport.setBounds (r.withTrimmedRight (6));
    rebuild();
}

//==============================================================================
AudioPanel::AudioPanel (MoshDeckProcessor& p) : proc (p)
{
    auto setup = [this] (juce::Slider& s, juce::Label& l, const char* name, double mn, double mx, std::atomic<float>& v)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (mn, mx, 0.01);
        s.setValue (v.load(), juce::dontSendNotification);
        s.onValueChange = [&s, &v] { v = (float) s.getValue(); };
        l.setText (name, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, theme::dim);
        addAndMakeVisible (s); addAndMakeVisible (l);
    };
    setup (gain, gainL, "Input gain", 0.1, 8.0, proc.analyzer.inputGain);
    setup (smooth, smoothL, "Smoothing", 0.0, 0.95, proc.analyzer.smoothing);
    setup (sens, sensL, "Onset sensitivity", 0.3, 3.0, proc.analyzer.onsetSensitivity);
    startTimerHz (30);
}

void AudioPanel::timerCallback() { proc.analyzer.read (f); repaint (meterArea); }

void AudioPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto ctrl = r.removeFromRight (260);
    for (auto pr : { std::make_pair (&gainL, &gain), std::make_pair (&smoothL, &smooth), std::make_pair (&sensL, &sens) })
    {
        auto row = ctrl.removeFromTop (26);
        pr.first->setBounds (row.removeFromLeft (110));
        pr.second->setBounds (row.reduced (0, 3));
    }
    meterArea = r;
}

void AudioPanel::paint (juce::Graphics& g)
{
    auto r = meterArea.toFloat();
    g.setColour (juce::Colour (0xff0b0b10));
    g.fillRoundedRectangle (r, 4.0f);
    auto specR = r.removeFromLeft (r.getWidth() * 0.6f).reduced (4);
    // log spectrum bars + peaks
    const int n = AudioFeatures::numLogBins;
    const float bw = specR.getWidth() / n;
    for (int i = 0; i < n; ++i)
    {
        const float v = f.logSpectrum[(size_t) i], pk = f.logPeaks[(size_t) i];
        g.setColour (juce::Colour::fromHSV (0.85f - 0.5f * i / n, 0.8f, 1.0f, 1.0f));
        g.fillRect (specR.getX() + i * bw, specR.getBottom() - v * specR.getHeight(), juce::jmax (1.0f, bw - 1), v * specR.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillRect (specR.getX() + i * bw, specR.getBottom() - pk * specR.getHeight() - 1, juce::jmax (1.0f, bw - 1), 1.5f);
    }
    // waveform
    juce::Path wave;
    for (int i = 0; i < AudioFeatures::numWave; ++i)
    {
        const float x = specR.getX() + specR.getWidth() * i / (AudioFeatures::numWave - 1);
        const float y = specR.getCentreY() - f.wave[(size_t) i] * specR.getHeight() * 0.4f;
        if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
    }
    g.setColour (theme::accent2.withAlpha (0.6f));
    g.strokePath (wave, juce::PathStrokeType (1.0f));

    // band meters + onsets
    auto br = r.reduced (6);
    static const char* names[] = { "SUB", "BASS", "MID", "HI-MID", "TREB" };
    const float mw = br.getWidth() / 8.0f;
    for (int b = 0; b < 5; ++b)
    {
        auto m = juce::Rectangle<float> (br.getX() + b * mw, br.getY(), mw - 4, br.getHeight() - 14);
        g.setColour (theme::outline);
        g.fillRect (m);
        g.setColour (theme::accent);
        g.fillRect (m.withTop (m.getBottom() - f.bands[(size_t) b] * m.getHeight()));
        g.setColour (theme::dim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (names[b], (int) m.getX() - 4, (int) m.getBottom(), (int) m.getWidth() + 8, 12, juce::Justification::centred);
    }
    const float onsets[3] = { f.kick, f.snare, f.hat };
    static const char* on[] = { "KICK", "SNARE", "HAT" };
    for (int k = 0; k < 3; ++k)
    {
        auto c = juce::Rectangle<float> (br.getX() + (5 + k) * mw, br.getY() + 10, mw - 6, mw - 6);
        g.setColour (theme::mod.withAlpha (0.15f + 0.85f * onsets[k]));
        g.fillEllipse (c);
        g.setColour (theme::dim);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (on[k], (int) c.getX() - 6, (int) c.getBottom() + 2, (int) c.getWidth() + 12, 12, juce::Justification::centred);
    }
}
} // namespace moshdeck
