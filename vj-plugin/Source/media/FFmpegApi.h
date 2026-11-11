#pragma once
#if MOSHDECK_HAVE_FFMPEG
#include <juce_core/juce_core.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace moshdeck
{
// Every FFmpeg function the decoder calls. They are looked up by name at runtime
// instead of being linked: on Windows the plugin then loads even when the FFmpeg
// DLLs are missing (video is simply disabled), and the DLLs are taken from the
// plugin's own folder rather than the host's.
#define MOSHDECK_FFMPEG_FUNCTIONS(X) \
    X (avutil,   av_frame_alloc) \
    X (avutil,   av_frame_free) \
    X (avutil,   av_frame_unref) \
    X (avutil,   av_strerror) \
    X (avcodec,  av_packet_alloc) \
    X (avcodec,  av_packet_free) \
    X (avcodec,  av_packet_unref) \
    X (avcodec,  avcodec_alloc_context3) \
    X (avcodec,  avcodec_find_decoder) \
    X (avcodec,  avcodec_flush_buffers) \
    X (avcodec,  avcodec_free_context) \
    X (avcodec,  avcodec_get_name) \
    X (avcodec,  avcodec_open2) \
    X (avcodec,  avcodec_parameters_to_context) \
    X (avcodec,  avcodec_receive_frame) \
    X (avcodec,  avcodec_send_packet) \
    X (avformat, av_find_best_stream) \
    X (avformat, av_read_frame) \
    X (avformat, av_seek_frame) \
    X (avformat, avformat_close_input) \
    X (avformat, avformat_find_stream_info) \
    X (avformat, avformat_open_input) \
    X (swscale,  sws_freeContext) \
    X (swscale,  sws_getContext) \
    X (swscale,  sws_scale)

struct FFmpegApi
{
#define MOSHDECK_FF_MEMBER(lib, fn) decltype (&::fn) fn = nullptr;
    MOSHDECK_FFMPEG_FUNCTIONS (MOSHDECK_FF_MEMBER)
#undef MOSHDECK_FF_MEMBER
};

/** Loads FFmpeg once. Returns an empty string on success, otherwise a user-facing reason. */
juce::String loadFFmpeg (const std::function<void (const juce::String&)>& log);

/** The resolved function table (valid only after loadFFmpeg() returned empty). */
const FFmpegApi& ffapi();
} // namespace moshdeck
#endif
