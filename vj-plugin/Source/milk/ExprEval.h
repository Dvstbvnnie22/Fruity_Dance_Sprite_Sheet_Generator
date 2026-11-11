#pragma once
#include <juce_core/juce_core.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace moshdeck::milk
{
/** Variable storage shared by a set of compiled programs (case-insensitive names). */
class VarTable
{
public:
    int slot (const std::string& name);
    int find (const std::string& name) const;
    double& operator[] (int i) { return values[(size_t) i]; }
    double get (const std::string& name) const { const int i = find (name); return i >= 0 ? values[(size_t) i] : 0.0; }
    void set (const std::string& name, double v) { values[(size_t) slot (name)] = v; }
    size_t size() const { return values.size(); }
    std::vector<double> values;
    std::vector<double> megabuf;  // eel "megabuf()" memory
    std::vector<double>* gmegabuf = nullptr; // shared across a preset
private:
    std::unordered_map<std::string, int> names;
};

struct Node;
using NodePtr = std::unique_ptr<Node>;

/**
    Compiler + tree-walking interpreter for MilkDrop / ns-eel2 style expressions:
    statements separated by ';', assignments (= += -= *= /= %=), arithmetic, bitwise | &,
    comparisons, logical && || !, ternary ?:, and the usual function set
    (sin cos tan asin acos atan atan2 sqr sqrt pow exp log log10 abs min max sign rand
    int floor ceil above below equal if bnot band bor sigmoid fmod loop while exec2 exec3
    megabuf gmegabuf).
*/
class Program
{
public:
    Program();
    ~Program();
    Program (Program&&) noexcept;
    Program& operator= (Program&&) noexcept;

    bool compile (const std::string& source, VarTable& vars, juce::String* error = nullptr);
    double run (VarTable& vars) const;
    bool empty() const { return statements.empty(); }

private:
    std::vector<NodePtr> statements;
};
} // namespace moshdeck::milk
