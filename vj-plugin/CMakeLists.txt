cmake_minimum_required(VERSION 3.22)
project(MoshDeckVJ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MOSHDECK_USE_FFMPEG "Decode video files with FFmpeg (libavformat/libavcodec/libswscale)" ON)
option(MOSHDECK_USE_SPOUT "Spout video sharing in/out (Windows only; Spout2 SDK is fetched automatically)" ON)
option(MOSHDECK_USE_NDI "NDI video in/out (the NDI runtime is loaded at run time; no SDK needed to build)" ON)
set(MOSHDECK_FORMATS "VST3;Standalone" CACHE STRING "Plugin formats to build")

# ---------------------------------------------------------------- JUCE
# Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to use a local checkout.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MoshDeckVJ
    COMPANY_NAME            "MoshDeck"
    BUNDLE_ID               com.moshdeck.vj
    PLUGIN_MANUFACTURER_CODE Mshd
    PLUGIN_CODE             MdVj
    FORMATS                 ${MOSHDECK_FORMATS}
    PRODUCT_NAME            "MoshDeck VJ"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Fx Analyzer
    MICROPHONE_PERMISSION_ENABLED FALSE)

file(GLOB_RECURSE MOSHDECK_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)
target_sources(MoshDeckVJ PRIVATE ${MOSHDECK_SOURCES})
target_include_directories(MoshDeckVJ PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_compile_definitions(MoshDeckVJ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MoshDeckVJ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- Spout / NDI video I/O
# Spout: the SpoutDX (DirectX 11) part of the Spout2 SDK (BSD-2-Clause), compiled into a static library.
# Pass -DFETCHCONTENT_SOURCE_DIR_SPOUT2=/path/to/Spout2 to use a local checkout.
if(WIN32 AND MOSHDECK_USE_SPOUT)
    FetchContent_Declare(Spout2
        URL https://github.com/leadedge/Spout2/archive/6b68a5b4fdeca6130b851e5f9366467a844860e5.zip   # 2.007.018
        SOURCE_SUBDIR _moshdeck_no_cmake)   # only download; we compile the files we need below
    FetchContent_MakeAvailable(Spout2)
    set(SPOUT_SDK ${spout2_SOURCE_DIR}/SPOUTSDK)
    add_library(moshdeck_spout STATIC
        ${SPOUT_SDK}/SpoutDirectX/SpoutDX/SpoutDX.cpp
        ${SPOUT_SDK}/SpoutGL/SpoutCopy.cpp
        ${SPOUT_SDK}/SpoutGL/SpoutDirectX.cpp
        ${SPOUT_SDK}/SpoutGL/SpoutFrameCount.cpp
        ${SPOUT_SDK}/SpoutGL/SpoutSenderNames.cpp
        ${SPOUT_SDK}/SpoutGL/SpoutSharedMemory.cpp
        ${SPOUT_SDK}/SpoutGL/SpoutUtils.cpp)
    target_include_directories(moshdeck_spout PUBLIC ${SPOUT_SDK}/SpoutDirectX/SpoutDX ${SPOUT_SDK}/SpoutGL)
    target_compile_definitions(moshdeck_spout PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    if(MSVC)
        target_compile_options(moshdeck_spout PRIVATE /W0 /utf-8)
    endif()
    target_link_libraries(moshdeck_spout PUBLIC d3d11 dxgi winmm psapi shell32 advapi32 version comctl32)
    set(MOSHDECK_HAVE_SPOUT ON)
    message(STATUS "MoshDeck: Spout input/output enabled (SpoutDX ${SPOUT_SDK})")
endif()

# Adds the Spout / NDI / headless-GL support files and their dependencies to a target.
function(moshdeck_add_video_io target)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/io/LiveSources.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/io/NdiApi.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/io/SpoutIO.cpp)
    if(MOSHDECK_HAVE_SPOUT)
        target_link_libraries(${target} PRIVATE moshdeck_spout)
        target_compile_definitions(${target} PRIVATE MOSHDECK_HAVE_SPOUT=1)
    endif()
    if(MOSHDECK_USE_NDI)
        target_compile_definitions(${target} PRIVATE MOSHDECK_HAVE_NDI=1)
    endif()
endfunction()

function(moshdeck_add_headless_gl target)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/io/HeadlessGL.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/io/Broadcaster.cpp)
    if(UNIX AND NOT APPLE)
        find_package(X11 REQUIRED)
        find_package(OpenGL REQUIRED)
        target_link_libraries(${target} PRIVATE X11::X11 OpenGL::GL)
    endif()
endfunction()

# The plugin already compiles everything under Source/ (GLOB above); add the libraries and defines.
if(MOSHDECK_HAVE_SPOUT)
    target_link_libraries(MoshDeckVJ PRIVATE moshdeck_spout)
    target_compile_definitions(MoshDeckVJ PRIVATE MOSHDECK_HAVE_SPOUT=1)
endif()
if(MOSHDECK_USE_NDI)
    target_compile_definitions(MoshDeckVJ PRIVATE MOSHDECK_HAVE_NDI=1)
endif()
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(MoshDeckVJ PRIVATE X11::X11 OpenGL::GL)
endif()

# ---------------------------------------------------------------- FFmpeg
if(MOSHDECK_USE_FFMPEG)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG IMPORTED_TARGET libavformat libavcodec libswscale libavutil)
    endif()
    if(FFMPEG_FOUND)
        target_link_libraries(MoshDeckVJ PRIVATE PkgConfig::FFMPEG)
        target_compile_definitions(MoshDeckVJ PRIVATE MOSHDECK_HAVE_FFMPEG=1)
        message(STATUS "MoshDeck: FFmpeg video decoding enabled")
    else()
        # vcpkg / manual install on Windows: set FFMPEG_ROOT to a folder containing include/ and lib/
        if(DEFINED FFMPEG_ROOT)
            file(TO_CMAKE_PATH "${FFMPEG_ROOT}" FFMPEG_ROOT)   # backslash paths break file(GLOB)
            # Headers only: the FFmpeg DLLs are loaded at runtime from the plugin's own folder
            # (Source/media/FFmpegApi.cpp), so nothing is linked and the plugin always loads.
            if(NOT EXISTS ${FFMPEG_ROOT}/include/libavformat/avformat.h)
                message(FATAL_ERROR "MoshDeck: ${FFMPEG_ROOT}/include/libavformat/avformat.h not found")
            endif()
            target_include_directories(MoshDeckVJ PRIVATE ${FFMPEG_ROOT}/include)
            target_compile_definitions(MoshDeckVJ PRIVATE MOSHDECK_HAVE_FFMPEG=1)
            set(MOSHDECK_FFMPEG_HEADERS_ONLY ON)
            message(STATUS "MoshDeck: FFmpeg from FFMPEG_ROOT=${FFMPEG_ROOT}")
        else()
            message(WARNING "MoshDeck: FFmpeg not found - decks will only load still images / image sequences")
        endif()
    endif()
endif()

# ---------------------------------------------------------------- headless tests
option(MOSHDECK_BUILD_TESTS "Build the console test runner" ON)
if(MOSHDECK_BUILD_TESTS)
    juce_add_console_app(MoshDeckTests PRODUCT_NAME "MoshDeckTests")
    target_sources(MoshDeckTests PRIVATE
        Tests/TestMain.cpp
        Source/milk/ExprEval.cpp
        Source/milk/MilkPreset.cpp
        Source/milk/BuiltinMilkPresets.cpp
        Source/audio/AudioAnalyzer.cpp
        Source/model/Scene.cpp
        Source/model/Crossfader.cpp
        Source/model/ModMatrix.cpp
        Source/render/ShadertoyImporter.cpp
        Source/render/EffectLibrary.cpp
        Source/render/GeneratorLibrary.cpp
        Source/render/VizLibrary.cpp)
    target_include_directories(MoshDeckTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(MoshDeckTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 MOSHDECK_TESTS=1)
    target_link_libraries(MoshDeckTests PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_data_structures juce::juce_events
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    juce_add_console_app(MoshDeckRigBuilder PRODUCT_NAME "MoshDeckRigBuilder")
    target_sources(MoshDeckRigBuilder PRIVATE Tests/BuildDubstepRig.cpp Source/model/Scene.cpp Source/render/EffectLibrary.cpp Source/render/GeneratorLibrary.cpp)
    target_include_directories(MoshDeckRigBuilder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(MoshDeckRigBuilder PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MoshDeckRigBuilder PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_data_structures juce::juce_events
        PUBLIC juce::juce_recommended_config_flags)
endif()

# ---------------------------------------------------------------- media decode test
if(MOSHDECK_BUILD_TESTS)
    juce_add_console_app(MoshDeckMediaTest PRODUCT_NAME "MoshDeckMediaTest")
    target_sources(MoshDeckMediaTest PRIVATE Tests/MediaTest.cpp Source/media/MediaDeck.cpp Source/media/FFmpegApi.cpp)
    target_include_directories(MoshDeckMediaTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    moshdeck_add_video_io(MoshDeckMediaTest)
    target_compile_definitions(MoshDeckMediaTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MoshDeckMediaTest PRIVATE juce::juce_graphics PUBLIC juce::juce_recommended_config_flags)
    if(TARGET PkgConfig::FFMPEG)
        target_link_libraries(MoshDeckMediaTest PRIVATE PkgConfig::FFMPEG)
        target_compile_definitions(MoshDeckMediaTest PRIVATE MOSHDECK_HAVE_FFMPEG=1)
    elseif(MOSHDECK_FFMPEG_HEADERS_ONLY)
        target_include_directories(MoshDeckMediaTest PRIVATE ${FFMPEG_ROOT}/include)
        target_compile_definitions(MoshDeckMediaTest PRIVATE MOSHDECK_HAVE_FFMPEG=1)
    endif()
endif()

# ---------------------------------------------------------------- GPU render smoke test
option(MOSHDECK_BUILD_RENDER_TEST "Build the OpenGL render smoke test app" ON)
if(MOSHDECK_BUILD_RENDER_TEST)
    juce_add_gui_app(MoshDeckRenderTest PRODUCT_NAME "MoshDeckRenderTest")
    file(GLOB_RECURSE MOSHDECK_ENGINE_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/audio/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/milk/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/model/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/media/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/render/*.cpp)
    list(FILTER MOSHDECK_ENGINE_SOURCES EXCLUDE REGEX ".*BeatClock.cpp$")
    target_sources(MoshDeckRenderTest PRIVATE Tests/RenderTest.cpp ${MOSHDECK_ENGINE_SOURCES})
    moshdeck_add_video_io(MoshDeckRenderTest)
    target_include_directories(MoshDeckRenderTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(MoshDeckRenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MoshDeckRenderTest PRIVATE juce::juce_gui_extra juce::juce_opengl juce::juce_dsp juce::juce_audio_basics
        PUBLIC juce::juce_recommended_config_flags)
    if(FFMPEG_FOUND)
        target_link_libraries(MoshDeckRenderTest PRIVATE PkgConfig::FFMPEG)
        target_compile_definitions(MoshDeckRenderTest PRIVATE MOSHDECK_HAVE_FFMPEG=1)
    endif()
endif()

# ---------------------------------------------------------------- Spout / NDI loopback test
# Renders a known test pattern through the headless broadcaster, sends it over Spout and NDI,
# receives it back through the deck input path and checks size, orientation and colours.
option(MOSHDECK_BUILD_IO_TEST "Build the Spout/NDI loopback test app" ON)
if(MOSHDECK_BUILD_IO_TEST)
    juce_add_console_app(MoshDeckIoTest PRODUCT_NAME "MoshDeckIoTest")
    file(GLOB_RECURSE MOSHDECK_IO_ENGINE CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/audio/AudioAnalyzer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/milk/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/model/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/media/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/render/*.cpp)
    target_sources(MoshDeckIoTest PRIVATE Tests/IoTest.cpp ${MOSHDECK_IO_ENGINE})
    target_include_directories(MoshDeckIoTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(MoshDeckIoTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MoshDeckIoTest PRIVATE juce::juce_opengl juce::juce_gui_extra juce::juce_dsp juce::juce_audio_basics
        PUBLIC juce::juce_recommended_config_flags)
    moshdeck_add_video_io(MoshDeckIoTest)
    moshdeck_add_headless_gl(MoshDeckIoTest)
    if(TARGET PkgConfig::FFMPEG)
        target_link_libraries(MoshDeckIoTest PRIVATE PkgConfig::FFMPEG)
        target_compile_definitions(MoshDeckIoTest PRIVATE MOSHDECK_HAVE_FFMPEG=1)
    elseif(MOSHDECK_FFMPEG_HEADERS_ONLY)
        target_include_directories(MoshDeckIoTest PRIVATE ${FFMPEG_ROOT}/include)
        target_compile_definitions(MoshDeckIoTest PRIVATE MOSHDECK_HAVE_FFMPEG=1)
    endif()
endif()

# ---------------------------------------------------------------- VST3 host test
option(MOSHDECK_BUILD_HOST_TEST "Build a minimal VST3 host that loads and exercises the plugin" ON)
if(MOSHDECK_BUILD_HOST_TEST)
    juce_add_gui_app(MoshDeckHostTest PRODUCT_NAME "MoshDeckHostTest")
    target_sources(MoshDeckHostTest PRIVATE Tests/HostTest.cpp)
    target_compile_definitions(MoshDeckHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MoshDeckHostTest PRIVATE juce::juce_audio_processors juce::juce_gui_extra PUBLIC juce::juce_recommended_config_flags)
endif()
