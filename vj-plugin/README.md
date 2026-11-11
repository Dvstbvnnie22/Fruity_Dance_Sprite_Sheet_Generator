# MoshDeck VJ

An audio-synchronised VJ plugin (VST3 + Standalone, built with JUCE/OpenGL) made for FL Studio.
Drop it on a mixer insert and it listens to the audio, follows FL's tempo and song position,
and renders a layered live visual show into a preview and a separate (fullscreen) output window.

| Feature you asked for | What MoshDeck does |
|---|---|
| **Shadertoy import** | Paste or import any `mainImage()` shader, or a Shadertoy **JSON export** (Common + Buffer A–D + Image multipass). Fetch by ID through the official Shadertoy API (needs your API key). The usual Shadertoy uniforms are supported: `iTime`, `iResolution`, `iMouse`, `iFrame`, `iDate`, `iChannel0-3`, `iChannelResolution`… Extra uniforms `iParam0-3` (modulatable), plus `iBass`, `iMid`, `iTreble`, `iLevel`, `iKick`, `iBeat`, `iBpm`. Channels can be the layer input, Deck A/B, layers below, the audio FFT/waveform texture, noise, or another buffer. |
| **Large datamosh library** | 30 real-time datamosh effects: Pixel Bloom, Mosh Melt, I-Frame Drop, Block Freeze, Macroblock Shift, **Motion Transfer** (moves one deck with the motion of the other), Keyframe Bleed, DCT Crush, Chroma Bleed, Motion Vector View, genuine **Pixel Sort**, Glitch Slices, Block Displace, Data Corrupt, Directional Smear, Ghost Trails, Scanline Tear, Bad Signal, Interlace Glitch, Bit Crush, Quantized Motion, Codec Drift, Frame Stutter, Wave Mosh, RGB Flow Split, JPEG Ringing, Static Mosh, Deck Block Swap, Gravity Melt… "Keyframes" (refresh) can trigger on the beat, the bar or the kick. Another 46 effects cover colour, distortion, stylize, feedback/strobe and keying (76 in total). |
| **Two main media tracks** | Deck A and Deck B play video (MP4/MOV/MKV/WebM/AVI… through FFmpeg), animated GIFs and still images, each with a playlist. Settings: speed, **beat-locked looping** (the clip is stretched to loop every N beats), cue offset, and "next clip every N bars". An A/B crossfader is automatable from FL. |
| **Generative visuals like NestDrop, using the main tracks as the base** | NestDrop is built on MilkDrop presets, so MoshDeck has its **own MilkDrop-format engine**. It runs per-frame, per-pixel, custom-shape and custom-wave equations (a full ns-eel style expression language), the warp mesh, decay, waveforms (8 modes), shapes, borders, motion vectors, video echo, gamma, invert, solarize and so on. Presets cross-fade smoothly. It has **28 original built-in presets** and loads **any folder of `.milk` presets** (for example the preset folders you already have from NestDrop or MilkDrop). The **Base Inject** control (constant, on beat, on kick, or luma-gated) feeds Deck A, Deck B, the A/B mix or the layers below *into the feedback buffer*, so the preset warps your footage. There are also 24 generator shaders (fractals, tunnels, raymarching, voronoi, metaballs, liquid chrome…), and every one has a "Base" input too. |
| **Layer, blend and link everything** | An unlimited layer stack with 21 blend modes (Add, Screen, Multiply, Overlay, Difference, Hue/Colour/Luminosity, Luma key…), per-layer effect chains, and a master effect chain. **Every** parameter can be modulated by audio bands, onsets, beat/bar ramps, beat-synced LFOs, MIDI CC/notes, 16 host macros, or **any other parameter** (link). Route amounts are parameters too, so routes can modulate routes. |
| **MIDI connect & learn** | Right-click any slider and choose **MIDI Learn**, then move a knob. Notes can act as momentary or toggle. Learnable actions: next/prev clip, next/random Milk preset, tap tempo, crossfader cut, beat nudge. "Modulate with MIDI" turns the next control you move into a mod source. Mapped controls get CC feedback (for motorised faders / LED rings). |
| **Audio reactive inputs like Avee Player** | 12 visualizer elements that you stack as layers: spectrum bars, mirror bars, **circle spectrum with cover image**, circle wave, waveform, spectrum fill, particles, pulsing image, peak bars with falling caps, polygon ring, dual spectrum and glow orb. Each has size, position, bass-react, hue, hue spread, detail and thickness controls. One-click templates build multi-element compositions. Analysis covers FFT, log spectrum with peak-hold, 5 bands, kick/snare/hat onset detection, and gain/smoothing/sensitivity settings. |
| **Auto BPM synced with FL** | Reads FL's tempo, PPQ position, play state and time signature, and extrapolates between audio blocks so the visuals are smooth. When FL is stopped it keeps free-running. Other modes: **auto-detect** the tempo from the audio, or tap tempo; plus beat nudge. |
| **MIDI OUT** | Kick/snare/hat onset notes (with velocity), beat and bar notes locked to FL's position, band energies as CCs, MIDI clock (24 ppqn with start/stop), and controller feedback. |

> **Honest notes**
> * The MilkDrop engine is a clean-room implementation of the classic preset pipeline. MilkDrop 2 presets that depend on HLSL `warp_`/`comp_` **pixel shaders** load and run their equations, but those HLSL shaders are not executed, so such presets will look different from NestDrop. The plugin tells you when a preset contains them.
> * Avee Player `.viz` template files can't be imported (closed format); the element types and their controls are re-created as layers instead.
> * The datamosh effects are GPU emulations (optical-flow motion compensation, residual drift, keyframe dropping…), not bitstream corruption, so they work live on any source and react to the beat.
> * Shadertoy cubemap, keyboard, sound-output and VR passes are not supported. Fetching shaders by ID only works for shaders their authors published as "Public + API".
> * The macOS build from CI shows still images only; to play video on macOS, build locally with FFmpeg installed.

## Using it in FL Studio

1. Copy `MoshDeck VJ.vst3` to `C:\Program Files\Common Files\VST3` (macOS: `~/Library/Audio/Plug-Ins/VST3`), then run **Options → Manage plugins → Find plugins**.
2. Put **MoshDeck VJ** on a mixer insert (the master, or a bus with your drums for tighter reactions). The audio passes through untouched.
3. Click **Output Window** (choose the display first), or **Fullscreen** for the projector. Double-click the output to toggle fullscreen; press `Esc` to leave it. The output keeps running when you close the plugin window.
4. **MIDI in** (for MIDI Learn): open the plugin's wrapper settings (gear icon) → *MIDI* → set an **Input port** (e.g. 1). Then route your controller, or an FL *MIDI Out* channel set to the same port, to it.
5. **MIDI out**: wrapper settings → *MIDI* → set an **Output port**. Any instrument or MIDI Out channel listening on that port receives MoshDeck's onset, beat and CC messages. Configure them in the **MIDI Learn / Out** tab.
6. Host automation: *Crossfader*, *Master* and *Macro 1–16* show up in FL's automation and Multilink. Assign a macro as a mod source anywhere ("Modulate with → Macros").
7. The whole setup is saved with the FL project. **Save Scene / Load Scene** stores `.mdvj` scene files you can reuse across projects.

### Quick recipes
* *NestDrop-style feedback over your clip:* load a video into Deck A → **+ Layer → Milk preset**, set Input = Deck A, blend = Screen, raise **Base Inject**, set Inject Mode = 2 (kick). Set *Auto Next* to 16 to change preset every 16 beats.
* *Classic datamosh between two clips:* load clips into both decks, select the Deck A layer → **+ Add effect → Datamosh → Motion Transfer** with Refresh = 2 (bar).
* *Avee-style music video:* **+ Layer → Visualizer template → Circle spectrum + cover image + particles**, then load cover art into Deck A.
* *Link parameters:* right-click a slider → **Link to parameter**. The slider then follows the other parameter by the route amount (negative amounts invert it).

## Building

Requirements: CMake ≥ 3.22 and a C++17 compiler. JUCE 8.0.9 is fetched automatically. FFmpeg is optional (without it, decks show still images only).

```bash
# Linux (Debian/Ubuntu)
sudo apt install libgl1-mesa-dev libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
                 libxcomposite-dev libfreetype-dev libavcodec-dev libavformat-dev libswscale-dev libavutil-dev
cmake -S vj-plugin -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Windows (Visual Studio 2022) - FFMPEG_ROOT is an FFmpeg "shared" build with include/ lib/ bin/
cmake -S vj-plugin -B build -G "Visual Studio 17 2022" -A x64 -DFFMPEG_ROOT=C:/ffmpeg
cmake --build build --config Release
# then copy C:/ffmpeg/bin/*.dll into "MoshDeck VJ.vst3/Contents/x86_64-win/" (and next to the Standalone .exe).
# The plugin loads FFmpeg from its own folder at runtime; without the DLLs it still loads, with video disabled.
```

The GitHub Actions workflow (`.github/workflows/moshdeck-vj.yml`) builds Windows (with FFmpeg bundled), macOS and Linux, and uploads the `.vst3` files as build artifacts.

Useful options: `-DMOSHDECK_FORMATS="VST3;Standalone"`, `-DMOSHDECK_USE_FFMPEG=OFF`, `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE`.

## Tests

* `MoshDeckTests`: the expression language, `.milk` parsing and execution of every built-in preset, the scene model, the mod matrix and parameter linking, state round-trip, and Shadertoy JSON import. Pass a folder argument to dump all 112 effect/generator/visualizer shaders plus a Shadertoy sample shader for `glslangValidator`.
* `MoshDeckRenderTest`: renders every generator, visualizer, Milk preset and effect, plus Shadertoy (single- and multi-pass) and a full layer stack, through the real OpenGL engine with synthetic 120 BPM audio. It reports shader/GL errors and black output and writes contact sheets (`sheet_*.png`).
* `MoshDeckHostTest`: loads the built VST3 the way a DAW does. It checks that audio passes through unchanged, MIDI out and onset notes appear, state saves and restores, and the editor opens.

## Source layout

```
Source/
  PluginProcessor / PluginEditor   plugin glue, state, FL transport, MIDI routing
  audio/    AudioAnalyzer (FFT, bands, onsets, tempo detect), BeatClock (host sync)
  midi/     MidiLearn (learn + feedback), MidiOut (onset/beat notes, CCs, clock)
  model/    Scene (layers, effects, params, decks, persistence), ModMatrix (modulation & links)
  media/    MediaDeck (image / GIF / FFmpeg video decoding on a worker thread)
  milk/     ExprEval (MilkDrop equation language), MilkPreset (parser + per-frame/per-pixel state), built-in presets
  render/   RenderEngine (layer compositor), MilkRenderer, ShadertoyImporter, EffectLibrary,
            GeneratorLibrary, VizLibrary, ShaderPrelude, GLUtils
  ui/       panels, ParamSlider (right-click learn/modulate/link), VideoView + OutputWindow
Tests/      unit tests, GPU render test, VST3 host test
```
