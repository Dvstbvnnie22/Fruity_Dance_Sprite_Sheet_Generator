#include "FFmpegApi.h"

#if MOSHDECK_HAVE_FFMPEG
#include <map>
#include <mutex>

#if JUCE_WINDOWS
extern "C" __declspec (dllimport) void* __stdcall LoadLibraryW (const wchar_t*);
extern "C" __declspec (dllimport) void* __stdcall GetProcAddress (void*, const char*);
#else
#include <dlfcn.h>
#endif

namespace moshdeck
{
static FFmpegApi api;

const FFmpegApi& ffapi() { return api; }

juce::String loadFFmpeg (const std::function<void (const juce::String&)>& log)
{
    static std::once_flag once;
    static juce::String problem;
    std::call_once (once, [&log]
    {
        std::map<std::string, void*> modules;
#if JUCE_WINDOWS
        // The FFmpeg DLLs sit next to the plugin binary (inside the .vst3 bundle) or the
        // standalone .exe. Load them by full path, dependencies first.
        const auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        log ("FFmpeg: looking for DLLs in " + dir.getFullPathName());
        for (auto* lib : { "avutil", "swresample", "swscale", "avcodec", "avformat" })
        {
            auto found = dir.findChildFiles (juce::File::findFiles, false, juce::String (lib) + "-*.dll");
            if (found.isEmpty())
            {
                if (juce::String (lib) == "swresample") continue; // only a dependency of avcodec
                problem = "Video disabled: " + juce::String (lib) + "-*.dll not found next to the plugin in " + dir.getFullPathName()
                        + ". Copy the whole folder from the download, including the DLLs.";
                return;
            }
            void* h = LoadLibraryW (found[0].getFullPathName().toWideCharPointer());
            if (h == nullptr)
            {
                problem = "Video disabled: could not load " + found[0].getFileName();
                return;
            }
            log ("FFmpeg: loaded " + found[0].getFileName());
            modules[lib] = h;
        }
        auto resolve = [&modules] (const char* lib, const char* fn) -> void* { auto it = modules.find (lib); return it != modules.end() ? GetProcAddress (it->second, fn) : nullptr; };
#else
        // Linux/macOS: open each library by its versioned name (matching the headers we
        // were compiled against) and look the functions up per library, exactly like Windows.
        const std::pair<const char*, int> libs[] = { { "avutil", LIBAVUTIL_VERSION_MAJOR }, { "swscale", LIBSWSCALE_VERSION_MAJOR },
                                                     { "avcodec", LIBAVCODEC_VERSION_MAJOR }, { "avformat", LIBAVFORMAT_VERSION_MAJOR } };
        for (auto& [lib, major] : libs)
        {
           #if JUCE_MAC
            const auto file = "lib" + juce::String (lib) + "." + juce::String (major) + ".dylib";
           #else
            const auto file = "lib" + juce::String (lib) + ".so." + juce::String (major);
           #endif
            void* h = dlopen (file.toRawUTF8(), RTLD_NOW | RTLD_GLOBAL);
            if (h == nullptr)
            {
                problem = "Video disabled: could not load " + file;
                return;
            }
            log ("FFmpeg: loaded " + file);
            modules[lib] = h;
        }
        auto resolve = [&modules] (const char* lib, const char* fn) -> void* { auto it = modules.find (lib); return it != modules.end() ? dlsym (it->second, fn) : nullptr; };
#endif
#define MOSHDECK_FF_RESOLVE(lib, fn) \
        api.fn = reinterpret_cast<decltype (api.fn)> (resolve (#lib, #fn)); \
        if (api.fn == nullptr && problem.isEmpty()) problem = "Video disabled: FFmpeg function " #fn " not found (incompatible FFmpeg version?)";
        MOSHDECK_FFMPEG_FUNCTIONS (MOSHDECK_FF_RESOLVE)
#undef MOSHDECK_FF_RESOLVE
        if (problem.isEmpty()) log ("FFmpeg: all functions resolved");
    });
    if (problem.isNotEmpty()) log (problem);
    return problem;
}
} // namespace moshdeck
#endif
