#pragma once
#include "model/Scene.h"

namespace moshdeck
{
/**
    A shader-based effect / generator definition.
    `glsl` is the body of `vec4 fx(vec2 uv)` (effects) or `vec4 gen(vec2 uv)` (generators),
    written against the uniforms & helpers in ShaderPrelude (see render/ShaderPrelude.h).
    Parameters are read through the macros P0..P7.
*/
struct ShaderDef
{
    juce::String id, name, category, description;
    std::vector<ParamDef> params; // keys p0..p7
    juce::String glsl;
    bool usesHistory = false;     // needs uPrev / uPrevIn (feedback, datamosh)
};

class EffectLibrary
{
public:
    static const std::vector<ShaderDef>& all();
    static const ShaderDef* find (const juce::String& id);
    static juce::StringArray categories();
};

class GeneratorLibrary
{
public:
    static const std::vector<ShaderDef>& all();
    static const ShaderDef* get (int index);
    static int count() { return (int) all().size(); }
};

/** Helper used by both libraries: makes p0..pN ParamDefs from {name,min,max,def}. */
struct P { const char* name; float mn, mx, def; };
std::vector<ParamDef> makeParams (std::initializer_list<P>);
} // namespace moshdeck
