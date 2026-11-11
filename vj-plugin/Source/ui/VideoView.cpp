#include "VideoView.h"

namespace moshdeck
{
VideoView::VideoView (EngineHost& host) : engine (host)
{
    setOpaque (true);
    context.setRenderer (this);
    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    context.setComponentPaintingEnabled (false);
    context.setContinuousRepainting (true);
}

VideoView::~VideoView() { stop(); }

void VideoView::start()
{
    if (running) return;
    context.attachTo (*this);
    running = true;
}

void VideoView::stop()
{
    if (! running) return;
    context.detach();
    running = false;
}

void VideoView::newOpenGLContextCreated() { engine.contextCreated(); }

void VideoView::renderOpenGL()
{
    const float scale = (float) context.getRenderingScale();
    engine.renderFrame (juce::roundToInt (getWidth() * scale), juce::roundToInt (getHeight() * scale), scale);
}

void VideoView::openGLContextClosing() { engine.contextClosing(); }

//==============================================================================
OutputWindow::OutputWindow (EngineHost& host, int displayIndex, bool fullscreen)
    : juce::DocumentWindow ("MoshDeck VJ Output", juce::Colours::black, juce::DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    view = std::make_unique<VideoView> (host);
    view->onDoubleClick = [this] { setFullScreen (! isFullScreen()); };
    setContentNonOwned (view.get(), false);
    setResizable (true, true);

    const auto& displays = juce::Desktop::getInstance().getDisplays().displays;
    juce::Rectangle<int> area (100, 100, 960, 540);
    if (! displays.isEmpty())
    {
        const auto& d = displays.getReference (juce::jlimit (0, displays.size() - 1, displayIndex));
        area = d.userArea.withSizeKeepingCentre (juce::jmin (960, d.userArea.getWidth()), juce::jmin (540, d.userArea.getHeight()));
    }
    setBounds (area);
    setVisible (true);
    view->start();
    if (fullscreen)
        setFullScreen (true);
    setWantsKeyboardFocus (true);
}

OutputWindow::~OutputWindow()
{
    view->stop();
    clearContentComponent();
}

bool OutputWindow::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { setFullScreen (false); return true; }
    if (k.getTextCharacter() == 'f' || k.getTextCharacter() == 'F') { setFullScreen (! isFullScreen()); return true; }
    return false;
}
} // namespace moshdeck
