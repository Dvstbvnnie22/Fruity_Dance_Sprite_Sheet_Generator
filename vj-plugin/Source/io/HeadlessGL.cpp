#include "HeadlessGL.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #pragma comment (lib, "opengl32.lib")
 #pragma comment (lib, "gdi32.lib")
 #pragma comment (lib, "user32.lib")
#elif JUCE_LINUX || JUCE_BSD
 #include <X11/Xlib.h>
 #include <GL/glx.h>
 #undef None
 #undef Bool
 #undef Status
#endif

namespace moshdeck
{
#if JUCE_WINDOWS
struct HeadlessGL::Impl
{
    HWND hwnd = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;
    std::wstring className;
    HINSTANCE module = nullptr;
};

static HINSTANCE thisModule()
{
    HMODULE m = nullptr;
    GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        reinterpret_cast<LPCWSTR> (&thisModule), &m);
    return m;
}

bool HeadlessGL::create (juce::String& error)
{
    release();
    auto& m = *impl;
    m.module = thisModule();
    static std::atomic<int> counter { 0 };
    m.className = L"MoshDeckHeadlessGL_" + std::to_wstring (GetCurrentProcessId()) + L"_" + std::to_wstring (++counter);
    WNDCLASSW wc {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = m.module;
    wc.lpszClassName = m.className.c_str();
    if (RegisterClassW (&wc) == 0) { error = "could not register a window class"; return false; }

    m.hwnd = CreateWindowExW (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, m.className.c_str(), L"MoshDeck VJ broadcast", WS_POPUP,
                              0, 0, 16, 16, nullptr, nullptr, m.module, nullptr);
    if (m.hwnd == nullptr) { error = "could not create a hidden window"; release(); return false; }
    m.dc = GetDC (m.hwnd);

    PIXELFORMATDESCRIPTOR pfd {};
    pfd.nSize = sizeof (pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    const int pf = ChoosePixelFormat (m.dc, &pfd);
    if (pf == 0 || ! SetPixelFormat (m.dc, pf, &pfd)) { error = "no OpenGL pixel format"; release(); return false; }

    HGLRC legacy = wglCreateContext (m.dc);
    if (legacy == nullptr || ! wglMakeCurrent (m.dc, legacy)) { error = "could not create an OpenGL context"; if (legacy) wglDeleteContext (legacy); release(); return false; }

    using CreateContextAttribs = HGLRC (WINAPI*) (HDC, HGLRC, const int*);
    auto createAttribs = reinterpret_cast<CreateContextAttribs> (wglGetProcAddress ("wglCreateContextAttribsARB"));
    const int attribs[] = { 0x2091 /*MAJOR*/, 3, 0x2092 /*MINOR*/, 3, 0x9126 /*PROFILE_MASK*/, 0x1 /*CORE*/, 0 };
    m.rc = createAttribs != nullptr ? createAttribs (m.dc, nullptr, attribs) : nullptr;
    wglMakeCurrent (nullptr, nullptr);
    wglDeleteContext (legacy);
    if (m.rc == nullptr) { error = "OpenGL 3.3 core is not available (update the graphics driver)"; release(); return false; }
    if (! wglMakeCurrent (m.dc, m.rc)) { error = "could not activate the OpenGL context"; release(); return false; }
    return true;
}

void HeadlessGL::pumpMessages()
{
    MSG msg;
    while (PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage (&msg);
        DispatchMessageW (&msg);
    }
}

void HeadlessGL::release()
{
    auto& m = *impl;
    if (m.rc != nullptr)
    {
        if (wglGetCurrentContext() == m.rc) wglMakeCurrent (nullptr, nullptr);
        wglDeleteContext (m.rc);
        m.rc = nullptr;
    }
    if (m.dc != nullptr) { ReleaseDC (m.hwnd, m.dc); m.dc = nullptr; }
    if (m.hwnd != nullptr) { DestroyWindow (m.hwnd); m.hwnd = nullptr; }
    if (! m.className.empty()) { UnregisterClassW (m.className.c_str(), m.module); m.className.clear(); }
}

bool HeadlessGL::isValid() const { return impl->rc != nullptr; }

#elif JUCE_LINUX || JUCE_BSD
struct HeadlessGL::Impl
{
    Display* dpy = nullptr;
    GLXPbuffer pbuffer = 0;
    GLXContext ctx = nullptr;
};

bool HeadlessGL::create (juce::String& error)
{
    release();
    auto& m = *impl;
    m.dpy = XOpenDisplay (nullptr);
    if (m.dpy == nullptr) { error = "no X display"; return false; }
    const int fbAttribs[] = { GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT,
                              GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8, 0 };
    int n = 0;
    GLXFBConfig* cfgs = glXChooseFBConfig (m.dpy, DefaultScreen (m.dpy), fbAttribs, &n);
    if (cfgs == nullptr || n == 0) { error = "no GLX pbuffer config"; release(); return false; }
    using CreateContextAttribs = GLXContext (*) (Display*, GLXFBConfig, GLXContext, int, const int*);
    auto createAttribs = reinterpret_cast<CreateContextAttribs> (glXGetProcAddressARB ((const GLubyte*) "glXCreateContextAttribsARB"));
    const int ctxAttribs[] = { GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB, 3,
                               GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, 0 };
    m.ctx = createAttribs != nullptr ? createAttribs (m.dpy, cfgs[0], nullptr, 1, ctxAttribs) : nullptr;
    const int pbAttribs[] = { GLX_PBUFFER_WIDTH, 16, GLX_PBUFFER_HEIGHT, 16, 0 };
    m.pbuffer = glXCreatePbuffer (m.dpy, cfgs[0], pbAttribs);
    XFree (cfgs);
    if (m.ctx == nullptr || m.pbuffer == 0) { error = "OpenGL 3.3 core is not available"; release(); return false; }
    if (! glXMakeContextCurrent (m.dpy, m.pbuffer, m.pbuffer, m.ctx)) { error = "could not activate the OpenGL context"; release(); return false; }
    return true;
}

void HeadlessGL::pumpMessages() {}

void HeadlessGL::release()
{
    auto& m = *impl;
    if (m.dpy != nullptr)
    {
        if (m.ctx != nullptr)
        {
            if (glXGetCurrentContext() == m.ctx) glXMakeContextCurrent (m.dpy, 0, 0, nullptr);
            glXDestroyContext (m.dpy, m.ctx);
        }
        if (m.pbuffer != 0) glXDestroyPbuffer (m.dpy, m.pbuffer);
        XCloseDisplay (m.dpy);
    }
    m = Impl();
}

bool HeadlessGL::isValid() const { return impl->ctx != nullptr; }

#else
struct HeadlessGL::Impl {};
bool HeadlessGL::create (juce::String& error) { error = "headless rendering is not supported on this platform"; return false; }
void HeadlessGL::pumpMessages() {}
void HeadlessGL::release() {}
bool HeadlessGL::isValid() const { return false; }
#endif

HeadlessGL::HeadlessGL() : impl (std::make_unique<Impl>()) {}
HeadlessGL::~HeadlessGL() { release(); }
} // namespace moshdeck
