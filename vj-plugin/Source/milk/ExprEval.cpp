#include "ExprEval.h"
#include <cctype>
#include <cmath>
#include <cstring>
#include <random>

namespace moshdeck::milk
{
int VarTable::slot (const std::string& rawName)
{
    std::string name = juce::String (rawName).toLowerCase().toStdString();
    auto it = names.find (name);
    if (it != names.end()) return it->second;
    const int i = (int) values.size();
    names[name] = i;
    values.push_back (0.0);
    return i;
}

int VarTable::find (const std::string& rawName) const
{
    auto it = names.find (juce::String (rawName).toLowerCase().toStdString());
    return it == names.end() ? -1 : it->second;
}

//==============================================================================
enum class Op
{
    Const, Var, Assign, AddAssign, SubAssign, MulAssign, DivAssign, ModAssign,
    Add, Sub, Mul, Div, Mod, Pow, Neg, Not, BitOr, BitAnd, And, Or,
    Eq, Ne, Lt, Gt, Le, Ge, Ternary, Seq, Func, MegaBuf, GMegaBuf
};

enum class Fn
{
    Sin, Cos, Tan, Asin, Acos, Atan, Atan2, Sqr, Sqrt, Pow, Exp, Log, Log10, Abs, Min, Max, Sign,
    Rand, Int, Floor, Ceil, Above, Below, Equal, If, Bnot, Band, Bor, Sigmoid, Fmod, Loop, While,
    Exec2, Exec3, Invsqrt, Unknown
};

struct Node
{
    Op op = Op::Const;
    double value = 0.0;
    int var = -1;
    Fn fn = Fn::Unknown;
    std::vector<NodePtr> kids;
};

static NodePtr mk (Op op) { auto n = std::make_unique<Node>(); n->op = op; return n; }

static double& megaRef (std::vector<double>& buf, double idx)
{
    static double dummy = 0.0;
    const auto i = (long long) std::floor (idx + 0.0001);
    if (i < 0 || i >= (1 << 20)) { dummy = 0.0; return dummy; }
    if ((size_t) i >= buf.size()) buf.resize ((size_t) juce::jmin ((long long) (1 << 20), (i + 1) * 2), 0.0);
    return buf[(size_t) i];
}

static double eval (const Node& n, VarTable& v);

static double& lvalue (const Node& n, VarTable& v)
{
    if (n.op == Op::MegaBuf)  return megaRef (v.megabuf, eval (*n.kids[0], v));
    if (n.op == Op::GMegaBuf) return megaRef (v.gmegabuf != nullptr ? *v.gmegabuf : v.megabuf, eval (*n.kids[0], v));
    return v[n.var];
}

static thread_local std::minstd_rand rng { 12345u };

static double callFn (const Node& n, VarTable& v)
{
    auto a = [&] (size_t i) { return i < n.kids.size() ? eval (*n.kids[i], v) : 0.0; };
    switch (n.fn)
    {
        case Fn::Sin:   return std::sin (a (0));
        case Fn::Cos:   return std::cos (a (0));
        case Fn::Tan:   return std::tan (a (0));
        case Fn::Asin:  return std::asin (juce::jlimit (-1.0, 1.0, a (0)));
        case Fn::Acos:  return std::acos (juce::jlimit (-1.0, 1.0, a (0)));
        case Fn::Atan:  return std::atan (a (0));
        case Fn::Atan2: { const double y = a (0); return std::atan2 (y, a (1)); }
        case Fn::Sqr:   { const double x = a (0); return x * x; }
        case Fn::Sqrt:  return std::sqrt (std::abs (a (0)));
        case Fn::Invsqrt: { const double x = std::abs (a (0)); return x > 0 ? 1.0 / std::sqrt (x) : 0.0; }
        case Fn::Pow:   { const double x = a (0); const double y = a (1); const double r = std::pow (x, y); return std::isfinite (r) ? r : 0.0; }
        case Fn::Exp:   return std::exp (juce::jmin (a (0), 700.0));
        case Fn::Log:   { const double x = a (0); return x > 0 ? std::log (x) : 0.0; }
        case Fn::Log10: { const double x = a (0); return x > 0 ? std::log10 (x) : 0.0; }
        case Fn::Abs:   return std::abs (a (0));
        case Fn::Min:   { const double x = a (0); return juce::jmin (x, a (1)); }
        case Fn::Max:   { const double x = a (0); return juce::jmax (x, a (1)); }
        case Fn::Sign:  { const double x = a (0); return x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0); }
        case Fn::Rand:  { const double x = std::floor (a (0)); if (x < 1.0) return (double) (rng() % 1000000) / 1000000.0; return (double) (rng() % (uint32_t) juce::jmin (x, 4.0e9)); }
        case Fn::Int:   { const double x = a (0); return x >= 0 ? std::floor (x) : std::ceil (x); }
        case Fn::Floor: return std::floor (a (0));
        case Fn::Ceil:  return std::ceil (a (0));
        case Fn::Above: { const double x = a (0); return x > a (1) ? 1.0 : 0.0; }
        case Fn::Below: { const double x = a (0); return x < a (1) ? 1.0 : 0.0; }
        case Fn::Equal: { const double x = a (0); return std::abs (x - a (1)) < 1.0e-5 ? 1.0 : 0.0; }
        case Fn::If:    return std::abs (a (0)) > 1.0e-5 ? a (1) : a (2);
        case Fn::Bnot:  return std::abs (a (0)) < 1.0e-5 ? 1.0 : 0.0;
        case Fn::Band:  { const double x = a (0); return (std::abs (x) > 1.0e-5 && std::abs (a (1)) > 1.0e-5) ? 1.0 : 0.0; }
        case Fn::Bor:   { const double x = a (0); return (std::abs (x) > 1.0e-5 || std::abs (a (1)) > 1.0e-5) ? 1.0 : 0.0; }
        case Fn::Sigmoid: { const double x = a (0); const double k = a (1); const double t = 1.0 + std::exp (-x * k); return std::abs (t) > 1.0e-5 ? 1.0 / t : 0.0; }
        case Fn::Fmod:  { const double x = a (0); const double y = a (1); return y != 0 ? std::fmod (x, y) : 0.0; }
        case Fn::Loop:
        {
            const int count = juce::jlimit (0, 1 << 20, (int) a (0));
            double r = 0.0;
            for (int i = 0; i < count; ++i) r = a (1);
            return r;
        }
        case Fn::While:
        {
            int guard = 1 << 20;
            double r = 0.0;
            while (guard-- > 0 && std::abs (r = a (0)) > 1.0e-5) {}
            return r;
        }
        case Fn::Exec2: a (0); return a (1);
        case Fn::Exec3: a (0); a (1); return a (2);
        default: break;
    }
    return 0.0;
}

static double eval (const Node& n, VarTable& v)
{
    switch (n.op)
    {
        case Op::Const: return n.value;
        case Op::Var:   return v[n.var];
        case Op::MegaBuf: case Op::GMegaBuf: return lvalue (n, v);
        case Op::Assign:    { const double x = eval (*n.kids[1], v); return lvalue (*n.kids[0], v) = x; }
        case Op::AddAssign: { const double x = eval (*n.kids[1], v); return lvalue (*n.kids[0], v) += x; }
        case Op::SubAssign: { const double x = eval (*n.kids[1], v); return lvalue (*n.kids[0], v) -= x; }
        case Op::MulAssign: { const double x = eval (*n.kids[1], v); return lvalue (*n.kids[0], v) *= x; }
        case Op::DivAssign: { const double x = eval (*n.kids[1], v); auto& l = lvalue (*n.kids[0], v); l = x != 0 ? l / x : 0.0; return l; }
        case Op::ModAssign: { const double x = eval (*n.kids[1], v); auto& l = lvalue (*n.kids[0], v); const auto d = (long long) x; l = d != 0 ? (double) ((long long) l % d) : 0.0; return l; }
        case Op::Add: return eval (*n.kids[0], v) + eval (*n.kids[1], v);
        case Op::Sub: return eval (*n.kids[0], v) - eval (*n.kids[1], v);
        case Op::Mul: return eval (*n.kids[0], v) * eval (*n.kids[1], v);
        case Op::Div: { const double a = eval (*n.kids[0], v); const double b = eval (*n.kids[1], v); return b != 0 ? a / b : 0.0; }
        case Op::Mod: { const auto a = (long long) eval (*n.kids[0], v); const auto b = (long long) eval (*n.kids[1], v); return b != 0 ? (double) (a % b) : 0.0; }
        case Op::Pow: { const double a = eval (*n.kids[0], v); const double r = std::pow (a, eval (*n.kids[1], v)); return std::isfinite (r) ? r : 0.0; }
        case Op::Neg: return -eval (*n.kids[0], v);
        case Op::Not: return std::abs (eval (*n.kids[0], v)) < 1.0e-5 ? 1.0 : 0.0;
        case Op::BitOr:  return (double) ((long long) eval (*n.kids[0], v) | (long long) eval (*n.kids[1], v));
        case Op::BitAnd: return (double) ((long long) eval (*n.kids[0], v) & (long long) eval (*n.kids[1], v));
        case Op::And: return (std::abs (eval (*n.kids[0], v)) > 1.0e-5 && std::abs (eval (*n.kids[1], v)) > 1.0e-5) ? 1.0 : 0.0;
        case Op::Or:  return (std::abs (eval (*n.kids[0], v)) > 1.0e-5 || std::abs (eval (*n.kids[1], v)) > 1.0e-5) ? 1.0 : 0.0;
        case Op::Eq: return std::abs (eval (*n.kids[0], v) - eval (*n.kids[1], v)) < 1.0e-5 ? 1.0 : 0.0;
        case Op::Ne: return std::abs (eval (*n.kids[0], v) - eval (*n.kids[1], v)) >= 1.0e-5 ? 1.0 : 0.0;
        case Op::Lt: return eval (*n.kids[0], v) <  eval (*n.kids[1], v) ? 1.0 : 0.0;
        case Op::Gt: return eval (*n.kids[0], v) >  eval (*n.kids[1], v) ? 1.0 : 0.0;
        case Op::Le: return eval (*n.kids[0], v) <= eval (*n.kids[1], v) ? 1.0 : 0.0;
        case Op::Ge: return eval (*n.kids[0], v) >= eval (*n.kids[1], v) ? 1.0 : 0.0;
        case Op::Ternary: return std::abs (eval (*n.kids[0], v)) > 1.0e-5 ? eval (*n.kids[1], v) : eval (*n.kids[2], v);
        case Op::Seq: { double r = 0.0; for (auto& k : n.kids) r = eval (*k, v); return r; }
        case Op::Func: return callFn (n, v);
    }
    return 0.0;
}

//==============================================================================
namespace
{
struct Parser
{
    const std::string& s;
    size_t pos = 0;
    VarTable& vars;
    juce::String error;

    Parser (const std::string& src, VarTable& v) : s (src), vars (v) {}

    void skipWs()
    {
        for (;;)
        {
            while (pos < s.size() && std::isspace ((unsigned char) s[pos])) ++pos;
            if (pos + 1 < s.size() && s[pos] == '/' && s[pos + 1] == '/')
            {
                while (pos < s.size() && s[pos] != '\n') ++pos;
                continue;
            }
            if (pos + 1 < s.size() && s[pos] == '/' && s[pos + 1] == '*')
            {
                pos += 2;
                while (pos + 1 < s.size() && ! (s[pos] == '*' && s[pos + 1] == '/')) ++pos;
                pos = juce::jmin (s.size(), pos + 2);
                continue;
            }
            break;
        }
    }
    bool peek (const char* t) { skipWs(); return s.compare (pos, std::strlen (t), t) == 0; }
    bool accept (const char* t)
    {
        if (! peek (t)) return false;
        pos += std::strlen (t);
        return true;
    }
    void fail (const juce::String& m) { if (error.isEmpty()) error = m + " at " + juce::String ((int) pos); }

    static Fn lookupFn (const std::string& n)
    {
        static const std::unordered_map<std::string, Fn> fns = {
            { "sin", Fn::Sin }, { "cos", Fn::Cos }, { "tan", Fn::Tan }, { "asin", Fn::Asin }, { "acos", Fn::Acos },
            { "atan", Fn::Atan }, { "atan2", Fn::Atan2 }, { "sqr", Fn::Sqr }, { "sqrt", Fn::Sqrt }, { "pow", Fn::Pow },
            { "exp", Fn::Exp }, { "log", Fn::Log }, { "log10", Fn::Log10 }, { "abs", Fn::Abs }, { "min", Fn::Min },
            { "max", Fn::Max }, { "sign", Fn::Sign }, { "rand", Fn::Rand }, { "int", Fn::Int }, { "floor", Fn::Floor },
            { "ceil", Fn::Ceil }, { "above", Fn::Above }, { "below", Fn::Below }, { "equal", Fn::Equal }, { "if", Fn::If },
            { "bnot", Fn::Bnot }, { "band", Fn::Band }, { "bor", Fn::Bor }, { "sigmoid", Fn::Sigmoid }, { "fmod", Fn::Fmod },
            { "loop", Fn::Loop }, { "while", Fn::While }, { "exec2", Fn::Exec2 }, { "exec3", Fn::Exec3 }, { "invsqrt", Fn::Invsqrt } };
        auto it = fns.find (n);
        return it == fns.end() ? Fn::Unknown : it->second;
    }

    NodePtr statementList (char terminator)
    {
        auto seq = mk (Op::Seq);
        for (;;)
        {
            skipWs();
            if (pos >= s.size() || (terminator != 0 && s[pos] == terminator)) break;
            if (accept (";")) continue;
            auto e = expression();
            if (! e) return nullptr;
            seq->kids.push_back (std::move (e));
            skipWs();
            if (pos < s.size() && s[pos] == ';') { ++pos; continue; }
            if (pos >= s.size() || (terminator != 0 && s[pos] == terminator)) break;
            if (terminator == ',' || terminator == ')') break;
            if (terminator == 0) continue; // lenient: MilkDrop lines without ';'
            fail ("expected ';'");
            return nullptr;
        }
        return seq;
    }

    NodePtr expression() { return assignment(); }

    NodePtr assignment()
    {
        auto lhs = ternary();
        if (! lhs) return nullptr;
        const bool isL = lhs->op == Op::Var || lhs->op == Op::MegaBuf || lhs->op == Op::GMegaBuf;
        static const std::pair<const char*, Op> ops[] = { { "+=", Op::AddAssign }, { "-=", Op::SubAssign }, { "*=", Op::MulAssign },
                                                          { "/=", Op::DivAssign }, { "%=", Op::ModAssign } };
        if (isL)
        {
            for (auto& o : ops)
                if (accept (o.first))
                {
                    auto n = mk (o.second);
                    n->kids.push_back (std::move (lhs));
                    auto r = assignment(); if (! r) return nullptr;
                    n->kids.push_back (std::move (r));
                    return n;
                }
            if (peek ("=") && ! peek ("=="))
            {
                ++pos;
                auto n = mk (Op::Assign);
                n->kids.push_back (std::move (lhs));
                auto r = assignment(); if (! r) return nullptr;
                n->kids.push_back (std::move (r));
                return n;
            }
        }
        return lhs;
    }

    NodePtr ternary()
    {
        auto c = logicalOr();
        if (! c) return nullptr;
        if (accept ("?"))
        {
            auto a = assignment(); if (! a) return nullptr;
            NodePtr b;
            if (accept (":")) { b = assignment(); if (! b) return nullptr; }
            else { b = mk (Op::Const); }
            auto n = mk (Op::Ternary);
            n->kids.push_back (std::move (c)); n->kids.push_back (std::move (a)); n->kids.push_back (std::move (b));
            return n;
        }
        return c;
    }

    template <typename Next>
    NodePtr binary (Next next, std::initializer_list<std::pair<const char*, Op>> ops)
    {
        auto l = (this->*next)();
        if (! l) return nullptr;
        for (;;)
        {
            bool matched = false;
            for (auto& o : ops)
            {
                skipWs();
                // don't mistake '=' of an assignment / compound for comparison, nor '&&' for '&'
                if (std::strcmp (o.first, "|") == 0 && (peek ("||") || peek ("|="))) continue;
                if (std::strcmp (o.first, "&") == 0 && (peek ("&&") || peek ("&="))) continue;
                if ((std::strcmp (o.first, "<") == 0 && peek ("<=")) || (std::strcmp (o.first, ">") == 0 && peek (">="))) continue;
                if ((std::strcmp (o.first, "*") == 0 && peek ("*=")) || (std::strcmp (o.first, "/") == 0 && peek ("/="))
                    || (std::strcmp (o.first, "%") == 0 && peek ("%=")) || (std::strcmp (o.first, "+") == 0 && peek ("+="))
                    || (std::strcmp (o.first, "-") == 0 && peek ("-="))) continue;
                if (accept (o.first))
                {
                    auto n = mk (o.second);
                    n->kids.push_back (std::move (l));
                    auto r = (this->*next)(); if (! r) return nullptr;
                    n->kids.push_back (std::move (r));
                    l = std::move (n);
                    matched = true;
                    break;
                }
            }
            if (! matched) break;
        }
        return l;
    }

    NodePtr logicalOr()  { return binary (&Parser::logicalAnd, { { "||", Op::Or } }); }
    NodePtr logicalAnd() { return binary (&Parser::bitOr, { { "&&", Op::And } }); }
    NodePtr bitOr()      { return binary (&Parser::bitAnd, { { "|", Op::BitOr } }); }
    NodePtr bitAnd()     { return binary (&Parser::compare, { { "&", Op::BitAnd } }); }
    NodePtr compare()    { return binary (&Parser::additive, { { "==", Op::Eq }, { "!=", Op::Ne }, { "<=", Op::Le }, { ">=", Op::Ge }, { "<", Op::Lt }, { ">", Op::Gt } }); }
    NodePtr additive()   { return binary (&Parser::multiplicative, { { "+", Op::Add }, { "-", Op::Sub } }); }
    NodePtr multiplicative() { return binary (&Parser::unary, { { "*", Op::Mul }, { "/", Op::Div }, { "%", Op::Mod } }); }

    NodePtr unary()
    {
        if (accept ("-")) { auto n = mk (Op::Neg); auto k = unary(); if (! k) return nullptr; n->kids.push_back (std::move (k)); return n; }
        if (accept ("+")) return unary();
        if (peek ("!") && ! peek ("!=")) { ++pos; auto n = mk (Op::Not); auto k = unary(); if (! k) return nullptr; n->kids.push_back (std::move (k)); return n; }
        return power();
    }

    NodePtr power()
    {
        auto b = primary();
        if (! b) return nullptr;
        if (accept ("^"))
        {
            auto n = mk (Op::Pow);
            n->kids.push_back (std::move (b));
            auto e = unary(); if (! e) return nullptr;
            n->kids.push_back (std::move (e));
            return n;
        }
        return b;
    }

    NodePtr primary()
    {
        skipWs();
        if (pos >= s.size()) { fail ("unexpected end"); return nullptr; }
        const char c = s[pos];
        if (c == '(')
        {
            ++pos;
            auto e = statementList (')');
            if (! e) return nullptr;
            if (! accept (")")) { fail ("expected ')'"); return nullptr; }
            if (e->kids.size() == 1) return std::move (e->kids[0]);
            return e;
        }
        if (std::isdigit ((unsigned char) c) || c == '.')
        {
            size_t end = pos;
            if (c == '0' && pos + 1 < s.size() && (s[pos + 1] == 'x' || s[pos + 1] == 'X'))
            {
                end = pos + 2;
                while (end < s.size() && std::isxdigit ((unsigned char) s[end])) ++end;
                auto n = mk (Op::Const); n->value = (double) std::strtoll (s.substr (pos + 2, end - pos - 2).c_str(), nullptr, 16);
                pos = end; return n;
            }
            while (end < s.size() && (std::isdigit ((unsigned char) s[end]) || s[end] == '.')) ++end;
            if (end < s.size() && (s[end] == 'e' || s[end] == 'E'))
            {
                size_t e2 = end + 1;
                if (e2 < s.size() && (s[e2] == '+' || s[e2] == '-')) ++e2;
                if (e2 < s.size() && std::isdigit ((unsigned char) s[e2]))
                {
                    end = e2;
                    while (end < s.size() && std::isdigit ((unsigned char) s[end])) ++end;
                }
            }
            auto n = mk (Op::Const);
            n->value = std::atof (s.substr (pos, end - pos).c_str());
            pos = end;
            return n;
        }
        if (c == '$')
        {
            // $PI, $E, $PHI, $x hex
            size_t end = pos + 1;
            while (end < s.size() && (std::isalnum ((unsigned char) s[end]) || s[end] == '_')) ++end;
            auto name = juce::String (s.substr (pos + 1, end - pos - 1)).toLowerCase();
            pos = end;
            auto n = mk (Op::Const);
            if (name == "pi") n->value = juce::MathConstants<double>::pi;
            else if (name == "e") n->value = std::exp (1.0);
            else if (name == "phi") n->value = 1.61803398875;
            else if (name.startsWith ("x")) n->value = (double) name.substring (1).getHexValue64();
            return n;
        }
        if (std::isalpha ((unsigned char) c) || c == '_')
        {
            size_t end = pos;
            while (end < s.size() && (std::isalnum ((unsigned char) s[end]) || s[end] == '_' || s[end] == '.')) ++end;
            std::string name = juce::String (s.substr (pos, end - pos)).toLowerCase().toStdString();
            pos = end;
            if (accept ("("))
            {
                std::vector<NodePtr> args;
                skipWs();
                if (! accept (")"))
                {
                    for (;;)
                    {
                        auto a = statementList (',');
                        if (! a) return nullptr;
                        args.push_back (a->kids.size() == 1 ? std::move (a->kids[0]) : std::move (a));
                        if (accept (",")) continue;
                        if (accept (")")) break;
                        fail ("expected ')' in call to " + juce::String (name));
                        return nullptr;
                    }
                }
                if (name == "megabuf" || name == "gmegabuf")
                {
                    auto n = mk (name == "megabuf" ? Op::MegaBuf : Op::GMegaBuf);
                    if (args.empty()) args.push_back (mk (Op::Const));
                    n->kids.push_back (std::move (args[0]));
                    return n;
                }
                auto n = mk (Op::Func);
                n->fn = lookupFn (name);
                if (n->fn == Fn::Unknown) { fail ("unknown function " + juce::String (name)); return nullptr; }
                n->kids = std::move (args);
                return n;
            }
            auto n = mk (Op::Var);
            n->var = vars.slot (name);
            return n;
        }
        fail (juce::String ("unexpected '") + c + "'");
        return nullptr;
    }
};
} // namespace

Program::Program() = default;
Program::~Program() = default;
Program::Program (Program&&) noexcept = default;
Program& Program::operator= (Program&&) noexcept = default;

bool Program::compile (const std::string& source, VarTable& vars, juce::String* error)
{
    statements.clear();
    Parser p (source, vars);
    auto seq = p.statementList (0);
    if (! seq || p.error.isNotEmpty())
    {
        if (error) *error = p.error.isEmpty() ? juce::String ("parse error") : p.error;
        return false;
    }
    statements = std::move (seq->kids);
    return true;
}

double Program::run (VarTable& vars) const
{
    double r = 0.0;
    for (auto& s : statements)
        r = eval (*s, vars);
    return r;
}
} // namespace moshdeck::milk
