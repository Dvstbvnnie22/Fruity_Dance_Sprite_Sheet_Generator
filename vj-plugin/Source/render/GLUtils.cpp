#include "GLUtils.h"

namespace moshdeck::gl
{
static GLuint compileStage (GLenum type, const juce::String& src, juce::String& log)
{
    const GLuint s = glCreateShader (type);
    const char* c = src.toRawUTF8();
    glShaderSource (s, 1, &c, nullptr);
    glCompileShader (s);
    GLint ok = 0;
    glGetShaderiv (s, GL_COMPILE_STATUS, &ok);
    if (! ok)
    {
        char buf[4096] = {};
        GLsizei len = 0;
        glGetShaderInfoLog (s, (GLsizei) sizeof (buf) - 1, &len, buf);
        log << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment") << " shader error:\n" << juce::String (buf, (size_t) len) << "\n";
        glDeleteShader (s);
        return 0;
    }
    return s;
}

bool Shader::build (const juce::String& vs, const juce::String& fs, juce::String& log)
{
    release();
    const GLuint v = compileStage (GL_VERTEX_SHADER, vs, log);
    const GLuint f = compileStage (GL_FRAGMENT_SHADER, fs, log);
    if (v == 0 || f == 0)
    {
        if (v) glDeleteShader (v);
        if (f) glDeleteShader (f);
        return false;
    }
    program = glCreateProgram();
    glAttachShader (program, v);
    glAttachShader (program, f);
    glBindAttribLocation (program, 0, "aPos");
    glLinkProgram (program);
    glDeleteShader (v);
    glDeleteShader (f);
    GLint ok = 0;
    glGetProgramiv (program, GL_LINK_STATUS, &ok);
    if (! ok)
    {
        char buf[4096] = {};
        GLsizei len = 0;
        glGetProgramInfoLog (program, (GLsizei) sizeof (buf) - 1, &len, buf);
        log << "Link error:\n" << juce::String (buf, (size_t) len);
        release();
        return false;
    }
    return true;
}

void Shader::release()
{
    if (program != 0)
        glDeleteProgram (program);
    program = 0;
    locs.clear();
}

GLint Shader::loc (const char* name)
{
    auto it = locs.find (name);
    if (it != locs.end()) return it->second;
    const GLint l = program != 0 ? glGetUniformLocation (program, name) : -1;
    locs[name] = l;
    return l;
}

//==============================================================================
void Target::ensure (int width, int height, bool repeatWrap)
{
    width = juce::jmax (1, width); height = juce::jmax (1, height);
    if (fbo != 0 && w == width && h == height)
    {
        if (repeatWrap != wrap) setWrap (repeatWrap);
        return;
    }
    release();
    w = width; h = height; wrap = repeatWrap;
    glGenTextures (1, &tex);
    glBindTexture (GL_TEXTURE_2D, tex);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glGenFramebuffers (1, &fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    clear();
}

void Target::setWrap (bool repeat)
{
    wrap = repeat;
    glBindTexture (GL_TEXTURE_2D, tex);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}

void Target::release()
{
    if (fbo) glDeleteFramebuffers (1, &fbo);
    if (tex) glDeleteTextures (1, &tex);
    fbo = tex = 0;
    w = h = 0;
}

void Target::bind() const
{
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glViewport (0, 0, w, h);
}

void Target::clear (float r, float g, float b, float a) const
{
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glViewport (0, 0, w, h);
    glClearColor (r, g, b, a);
    glClear (GL_COLOR_BUFFER_BIT);
}

//==============================================================================
void Texture::uploadRGBA (const void* pixels, int width, int height, bool bgra)
{
    if (tex == 0)
    {
        glGenTextures (1, &tex);
        glBindTexture (GL_TEXTURE_2D, tex);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture (GL_TEXTURE_2D, tex);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    if (width != w || height != h)
    {
        w = width; h = height;
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, bgra ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    else
        glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, w, h, bgra ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void Texture::uploadFloat (const float* data, int width, int height)
{
    if (tex == 0)
    {
        glGenTextures (1, &tex);
        glBindTexture (GL_TEXTURE_2D, tex);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture (GL_TEXTURE_2D, tex);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
    if (width != w || height != h)
    {
        w = width; h = height;
        glTexImage2D (GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, data);
    }
    else
        glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_FLOAT, data);
}

void Texture::release()
{
    if (tex) glDeleteTextures (1, &tex);
    tex = 0; w = h = 0;
}

//==============================================================================
void Geometry::create()
{
    static const float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    glGenVertexArrays (1, &quadVao);
    glBindVertexArray (quadVao);
    glGenBuffers (1, &quadVbo);
    glBindBuffer (GL_ARRAY_BUFFER, quadVbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenVertexArrays (1, &dynVao);
    glBindVertexArray (dynVao);
    glGenBuffers (1, &dynVbo);
    glBindBuffer (GL_ARRAY_BUFFER, dynVbo);
    dynCapacity = 0;
    glBindVertexArray (0);
}

void Geometry::release()
{
    if (quadVbo) glDeleteBuffers (1, &quadVbo);
    if (quadVao) glDeleteVertexArrays (1, &quadVao);
    if (dynVbo) glDeleteBuffers (1, &dynVbo);
    if (dynVao) glDeleteVertexArrays (1, &dynVao);
    quadVao = quadVbo = dynVao = dynVbo = 0;
}

void Geometry::drawQuad() const
{
    glBindVertexArray (quadVao);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
}

void Geometry::drawDynamic (const std::vector<float>& verts, GLenum mode)
{
    if (verts.empty()) return;
    glBindVertexArray (dynVao);
    glBindBuffer (GL_ARRAY_BUFFER, dynVbo);
    const size_t bytes = verts.size() * sizeof (float);
    if (bytes > dynCapacity)
    {
        dynCapacity = bytes * 2;
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) dynCapacity, nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData (GL_ARRAY_BUFFER, 0, (GLsizeiptr) bytes, verts.data());
    const GLsizei stride = 8 * sizeof (float);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, stride, (void*) (2 * sizeof (float)));
    glEnableVertexAttribArray (2);
    glVertexAttribPointer (2, 4, GL_FLOAT, GL_FALSE, stride, (void*) (4 * sizeof (float)));
    glDrawArrays (mode, 0, (GLsizei) (verts.size() / 8));
}

void bindTex (int unit, GLuint tex)
{
    glActiveTexture ((GLenum) (GL_TEXTURE0 + unit));
    glBindTexture (GL_TEXTURE_2D, tex);
}
} // namespace moshdeck::gl
