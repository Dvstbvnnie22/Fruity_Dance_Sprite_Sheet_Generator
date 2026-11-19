#include "LiveSources.h"
#include "NdiApi.h"
#include "SpoutIO.h"

namespace moshdeck::live
{
bool isLivePath (const juce::String& p)
{
    return p.startsWithIgnoreCase (spoutPrefix) || p.startsWithIgnoreCase (ndiPrefix);
}

juce::String spoutPath (const juce::String& n) { return spoutPrefix + n; }
juce::String ndiPath (const juce::String& n) { return ndiPrefix + n; }

juce::String displayName (const juce::String& p)
{
    if (p.startsWithIgnoreCase (spoutPrefix)) return "Spout: " + p.substring ((int) strlen (spoutPrefix));
    if (p.startsWithIgnoreCase (ndiPrefix)) return "NDI: " + p.substring ((int) strlen (ndiPrefix));
    return juce::File (p).getFileName();
}

void listSources (std::vector<SourceInfo>& spoutOut, std::vector<SourceInfo>& ndiOut, juce::String& notes)
{
    spoutOut.clear();
    ndiOut.clear();
    notes.clear();
    if (spout::isAvailable())
        for (auto& n : spout::listSenders()) spoutOut.push_back ({ n, spoutPath (n) });
    else
        notes << "Spout is only available on Windows.\n";

    juce::String problem;
    if (ndi::api (&problem) != nullptr)
        for (auto& n : ndi::Finder::get().getSources()) ndiOut.push_back ({ n, ndiPath (n) });
    else
        notes << problem << "\n";
    notes = notes.trim();
}

//==============================================================================
namespace
{
    struct SpoutLive : Receiver
    {
        spout::Receiver rx;
        int receive (std::vector<uint8_t>& px, int& w, int& h, bool& bgra, juce::String& status) override
        {
            return rx.receive (px, w, h, bgra, status);
        }
    };

    struct NdiLive : Receiver
    {
        ndi::Receiver rx;
        int receive (std::vector<uint8_t>& px, int& w, int& h, bool& bgra, juce::String& status) override
        {
            bgra = false;
            const int r = rx.receive (px, w, h, 20, status);
            return r > 0 ? 1 : 0;
        }
    };
} // namespace

std::unique_ptr<Receiver> Receiver::create (const juce::String& path, juce::String& status)
{
    if (path.startsWithIgnoreCase (spoutPrefix))
    {
        auto r = std::make_unique<SpoutLive>();
        if (! r->rx.open (path.substring ((int) strlen (spoutPrefix)), status)) return nullptr;
        return r;
    }
    if (path.startsWithIgnoreCase (ndiPrefix))
    {
        auto r = std::make_unique<NdiLive>();
        if (! r->rx.open (path.substring ((int) strlen (ndiPrefix)), status)) return nullptr;
        return r;
    }
    status = "Unknown live source";
    return nullptr;
}
} // namespace moshdeck::live
