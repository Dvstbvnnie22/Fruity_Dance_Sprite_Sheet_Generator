#pragma once
#include <juce_opengl/juce_opengl.h>
#include "render/RenderEngine.h"

namespace moshdeck
{
/** An OpenGL surface that runs a RenderEngine (used for the editor preview and the output window). */
class VideoView : public juce::Component, private juce::OpenGLRenderer
{
public:
    explicit VideoView (EngineHost& host);
    ~VideoView() override;

    void start();
    void stop();
    bool isRunning() const { return running; }
    RenderEngine& getEngine() { return engine; }
    std::function<void()> onDoubleClick;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    juce::OpenGLContext context;
    RenderEngine engine;
    bool running = false;
};

/** Top-level output window - put it on the projector / second screen and go fullscreen. */
class OutputWindow : public juce::DocumentWindow
{
public:
    OutputWindow (EngineHost& host, int displayIndex, bool fullscreen);
    ~OutputWindow() override;
    void closeButtonPressed() override { if (onClose) onClose(); }
    bool keyPressed (const juce::KeyPress&) override;
    RenderEngine& getEngine() { return view->getEngine(); }
    std::function<void()> onClose;

private:
    std::unique_ptr<VideoView> view;
};
} // namespace moshdeck
