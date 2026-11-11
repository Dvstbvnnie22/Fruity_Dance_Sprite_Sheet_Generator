cmake_minimum_required(VERSION 3.22)
project(MoshDeckVJ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MOSHDECK_USE_FFMPEG "Decode video files with FFmpeg (libavformat/libavcodec/libswscale)" ON)
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
            target_include_directories(MoshDeckVJ PRIVATE ${FFMPEG_ROOT}/include)
            target_link_directories(MoshDeckVJ PRIVATE ${FFMPEG_ROOT}/lib)
            target_link_libraries(MoshDeckVJ PRIVATE avformat avcodec swscale avutil)
            target_compile_definitions(MoshDeckVJ PRIVATE MOSHDECK_HAVE_FFMPEG=1)
            if(WIN32)
                # Delay-load the FFmpeg DLLs (they live next to the plugin binary, see MediaDeck.cpp)
                file(GLOB MOSHDECK_FFMPEG_DLLS ${FFMPEG_ROOT}/bin/av*.dll ${FFMPEG_ROOT}/bin/sw*.dll)
                foreach(dll ${MOSHDECK_FFMPEG_DLLS})
                    get_filename_component(dllname ${dll} NAME)
                    foreach(t MoshDeckVJ_VST3 MoshDeckVJ_Standalone)
                        if(TARGET ${t})
                            target_link_options(${t} PRIVATE "/DELAYLOAD:${dllname}")
                        endif()
                    endforeach()
                endforeach()
                target_link_libraries(MoshDeckVJ PRIVATE delayimp)
            endif()
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
        Source/model/ModMatrix.cpp
        Source/render/ShadertoyImporter.cpp
        Source/render/EffectLibrary.cpp
        Source/render/GeneratorLibrary.cpp
        Source/render/VizLibrary.cpp)
    target_include_directories(MoshDeckTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(MoshDeckTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 MOSHDECK_TESTS=1)
    target_link_libraries(MoshDeckTests PRIVATE juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_data_structures juce::juce_events
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
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
    target_include_directories(MoshDeckRenderTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(MoshDeckRenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MoshDeckRenderTest PRIVATE juce::juce_gui_extra juce::juce_opengl juce::juce_dsp juce::juce_audio_basics
        PUBLIC juce::juce_recommended_config_flags)
    if(FFMPEG_FOUND)
        target_link_libraries(MoshDeckRenderTest PRIVATE PkgConfig::FFMPEG)
        target_compile_definitions(MoshDeckRenderTest PRIVATE MOSHDECK_HAVE_FFMPEG=1)
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
