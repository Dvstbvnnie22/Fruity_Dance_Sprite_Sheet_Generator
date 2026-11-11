#pragma once
#include <juce_opengl/juce_opengl.h>
#include <unordered_map>

namespace moshdeck::gl
{
using namespace juce::gl;

/** A compiled + linked GLSL program with cached uniform locations. */
class Shader
{
public:
    Shader() = default;
    ~Shader() { release(); }
    Shader (const Shader&) = delete;
    Shader& operator= (const Shader&) = delete;

    bool build (const juce::String& vs, const juce::String& fs, juce::String& log);
    void release();
    bool valid() const { return program != 0; }
    void use() const { glUseProgram (program); }
    GLint loc (const char* name);

    void set1i (const char* n, int v)                      { const GLint l = loc (n); if (l >= 0) glUniform1i (l, v); }
    void set1f (const char* n, float v)                    { const GLint l = loc (n); if (l >= 0) glUniform1f (l, v); }
    void set2f (const char* n, float a, float b)           { const GLint l = loc (n); if (l >= 0) glUniform2f (l, a, b); }
    void set3f (const char* n, float a, float b, float c)  { const GLint l = loc (n); if (l >= 0) glUniform3f (l, a, b, c); }
    void set4f (const char* n, float a, float b, float c, float d) { const GLint l = loc (n); if (l >= 0) glUniform4f (l, a, b, c, d); }
    void set1fv (const char* n, int count, const float* v) { const GLint l = loc (n); if (l >= 0) glUniform1fv (l, count, v); }

    GLuint program = 0;
private:
    std::unordered_map<std::string, GLint> locs;
};

/** Colour render target (texture + framebuffer). */
struct Target
{
    GLuint fbo = 0, tex = 0;
    int w = 0, h = 0;
    bool wrap = false;

    ~Target() { release(); }
    Target() = default;
    Target (const Target&) = delete;
    Target& operator= (const Target&) = delete;

    void ensure (int width, int height, bool repeatWrap = false);
    void release();
    void bind() const;
    void clear (float r = 0, float g = 0, float b = 0, float a = 0) const;
    void setWrap (bool repeat);
};

/** Plain texture with CPU upload. */
struct Texture
{
    GLuint tex = 0;
    int w = 0, h = 0;
    ~Texture() { release(); }
    Texture() = default;
    Texture (const Texture&) = delete;
    Texture& operator= (const Texture&) = delete;

    void uploadRGBA (const void* pixels, int width, int height, bool bgra = false);
    void uploadFloat (const float* data, int width, int height); // single channel R32F
    void release();
};

/** Full-screen triangle-pair VAO and a dynamic VAO for lines / polygons. */
struct Geometry
{
    GLuint quadVao = 0, quadVbo = 0;
    GLuint dynVao = 0, dynVbo = 0;
    size_t dynCapacity = 0;

    void create();
    void release();
    void drawQuad() const;
    /** vertices: x,y (clip space), u,v, r,g,b,a  = 8 floats each */
    void drawDynamic (const std::vector<float>& verts, GLenum mode);
};

void bindTex (int unit, GLuint tex);
} // namespace moshdeck::gl
