#pragma once
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

/*
    Live video inputs for the decks. A deck playlist entry is either a file
    path or a live source written as "spout:<sender name>" or
    "ndi:<NDI source name>". The deck's worker thread pulls frames from the
    receiver exactly like it decodes video, so every render engine (preview,
    output window, Spout/NDI broadcaster) sees the same frames.
*/
namespace moshdeck::live
{
inline const char* spoutPrefix = "spout:";
inline const char* ndiPrefix = "ndi:";

bool isLivePath (const juce::String& path);
juce::String spoutPath (const juce::String& senderName);
juce::String ndiPath (const juce::String& sourceName);
/** "Spout: name", "NDI: name", or the file name for ordinary files. */
juce::String displayName (const juce::String& path);

struct SourceInfo
{
    juce::String label, path;
};
/** Currently visible Spout senders and NDI sources. `notes` explains missing runtimes. */
void listSources (std::vector<SourceInfo>& spout, std::vector<SourceInfo>& ndi, juce::String& notes);

class Receiver
{
public:
    virtual ~Receiver() = default;
    /** Creates a receiver for a live path (nullptr if the kind is unknown or unavailable; status says why). */
    static std::unique_ptr<Receiver> create (const juce::String& path, juce::String& status);
    /** Returns 1 for a new frame (rows top-to-bottom). May block for a few milliseconds. */
    virtual int receive (std::vector<uint8_t>& pixels, int& w, int& h, bool& bgra, juce::String& status) = 0;
};
} // namespace moshdeck::live
