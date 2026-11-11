# Dubstep Machine: Patcher build guide

One FL Studio **Patcher** that holds the whole audio/visual dubstep rig:

| Plugin | Role in the rig |
|---|---|
| **Slicex** | your main audio loop, chopped into slices you trigger from pads |
| **FPC** | drums: kicks, snares, fills, one-shots on 16 pads |
| **Vital** | bass, part 1: sub and growl |
| **Odin 2** | bass, part 2: the moving reese / neuro mid-bass, playing the same notes as Vital |
| **Surge XT** | leads, screeches, risers, FX |
| **Flex** | stabs, hits, pads and atmospheres |
| **MoshDeck VJ** | the visuals, fed by the whole mix and steered by every control |

The web is held together by these stock FL helpers:
- **Control Surface**
- **Fruity Peak Controller** ×2
- **Fruity Formula Controller** ×2
- **Fruity Balance** (the "bass bus")
- **VFX Keyboard Splitter**

Build it once (about 30–40 minutes), then save it as a Patcher preset (step 9). After that you only load loops/samples and pick a sound preset in each plugin.

> **Why a guide and not a ready-made `.fst` file?** Patcher presets are an undocumented FL Studio binary format with each plugin's private state inside. A generated one would most likely fail to load or load scrambled. Built by hand following this guide, it's guaranteed to be valid for your FL version and your installed plugins.

---

## The web at a glance

```
                     ┌───────────────── VFX Keyboard Splitter ─────────────────┐
   From FL (MIDI) ──►│ <36 Vital + Odin 2 │ 36-51 FPC │ 52-59 Flex │ 60-83 Slicex │ 84+ Surge XT │
                     └──┬─────────┬────────┬──────────┬───────────┬────────────┬──────────────┘
                        ▼         ▼        ▼          ▼           ▼            ▼
                      Vital    Odin 2     FPC        Flex       Slicex      Surge XT
          audio:        └────┬────┘        │          │           │            │
                             ▼             ├──► Peak DRUMS ──┬──► XFADE BASS (b) ─► BASS BUS volume
                      BASS BUS (Fruity     │                 ├──► XFADE LEAD (b) ─► Surge XT volume
                      Balance) ──► Peak BASS ─► MoshDeck M3  └──► MoshDeck Macro 1 (flash / RGB split)
                             ▼             ▼          ▼           ▼            ▼
                             └──────────── all audio summed into MoshDeck VJ ──┘
                                                      ▼
                                          MoshDeck VJ ──► To FL (audio passes through)
                                                      ▲
   S2 MK3 + SMC-PAD ─► Control Surface ───────────────┴──► Vital / Odin 2 / Surge / Flex controls, Slicex,
                       (one control → several targets)       FPC, levels, kill switches, MoshDeck macros
```

Every hardware control lands on **one Control Surface element**. That element is cabled to an audio parameter **and** a MoshDeck macro, so each move changes the sound and the picture together. Several controls also drive **both** bass synths at once.

The two Peak Controllers close the loop:
- The drums duck the bass bus and the lead, and flash the visuals.
- The bass bus (Vital + Odin 2) pumps your footage into the Milk feedback.

---

## 0. Before you start

1. **Install the plugins:** MoshDeck VJ, Vital, Surge XT and Odin 2 (all VST3). Then run **Options → Manage plugins → Find plugins**.
2. **Connect the controllers:** go to **Options → MIDI settings** and enable both input devices. Give them different **Port** numbers (S2 MK3 = port 1, SMC-PAD = port 2) and set the controller type to *(generic controller)*. The S2 MK3 must be in MIDI mode; see [CONTROLLER_MAP.md](CONTROLLER_MAP.md#traktor-kontrol-s2-mk3).
3. **Have the visuals ready:** you'll load `dubstep-machine.mdvj` into MoshDeck VJ in step 6.

## 1. Create the Patcher

Add **Patcher** as an *instrument* on a Channel Rack channel (it holds generators). Open it. You start with **From FL** and **To FL**.

- **Adding modules:** right-click empty space and pick a plugin, or drag one in from the browser.
- **Cables:** drag from an output port to an input port. Audio ports and event/control ports are different colours. Several cables into one audio input are summed.

## 2. Add the modules

Add these, roughly left to right:

1. **VFX Keyboard Splitter** (if you don't have it, see the *No splitter* note in step 3)
2. **Vital**, **Odin 2**, **FPC**, **Flex**, **Slicex**, **Surge XT**
3. **Fruity Balance**: rename it **BASS BUS** (right-click module → Rename)
4. **Fruity Peak Controller** ×2: rename them **DRUMS** and **BASS**
5. **Fruity Formula Controller** ×2: rename them **XFADE BASS** and **XFADE LEAD**
6. **MoshDeck VJ**
7. **Control Surface**

## 3. Notes: split the keyboard so each plugin has its own zone

Cable **From FL (MIDI out) → VFX Keyboard Splitter**. Set up 5 zones and cable each zone output to its plugin(s):

| Zone (MIDI note numbers) | FL note names* | → plugin(s) | played by |
|---|---|---|---|
| 0 – 35 | up to B2 | **Vital** and **Odin 2** (two cables from the same zone output) | piano roll (bass lines; MIDI 29, which FL calls F2, is the classic dubstep root) |
| 36 – 51 | C3 – D#4 | **FPC** | SMC-PAD bank A (16 pads) |
| 52 – 59 | E4 – B4 | **Flex** | SMC-PAD bank C (8 pads) |
| 60 – 83 | C5 – B6 | **Slicex** | SMC-PAD bank B (slices 1–16) + piano roll |
| 84 – 127 | C7 and up | **Surge XT** | piano roll; set Surge's scene **Octave** to −3 so these notes sound in the lead range |

\* FL Studio calls MIDI note 60 "C5". Most other apps and controller editors call it C4. Go by the **numbers**.

- **FPC:** set the 16 pads to notes 36–51 (select a pad and set its note in the pad properties).
- **Slicex:** set its root/first-slice note so slice 1 = note 60 (this is Slicex's default).
- **Vital + Odin 2 stack:** for a clean low end, let **Vital** carry the sub. On **Odin 2**, roll off the lows with its filter or a high-pass, so the reese sits above the sub. If both presets are full-range, the two synths will fight in the low end.
- **No splitter?** Cable From FL to all six instruments. Then use separate MIDI channels instead: give the SMC-PAD banks different channels in the M-VAVE app, and put piano-roll notes for the bass and Surge in their own patterns. Expect some overlap.

## 4. Audio cables

1. **Vital + Odin 2 (audio out) → BASS BUS (audio in).** Both go into the same input and are summed.
2. **BASS BUS, FPC, Flex, Slicex, Surge XT (audio out) → MoshDeck VJ (audio in).**
3. **MoshDeck VJ (audio out) → To FL.** MoshDeck passes audio through untouched, so this is your master mix.
4. **FPC (audio out) → Peak Controller DRUMS (audio in)**: a second cable from the same output.
5. **BASS BUS (audio out) → Peak Controller BASS (audio in)**: a second cable. The visuals now react to Vital *and* Odin 2.

The Peak Controllers produce only control signals, so their audio outputs stay unconnected.

## 5. Exposing plugin parameters as Patcher inputs

To cable a control into a plugin parameter, the parameter needs an input port on the plugin's box:

- Right-click the plugin's box in Patcher and tick the parameter in its list of inputs. Parameters you moved recently appear first; if one is missing, wiggle it inside the plugin, then look again.

Expose these parameters:

| Plugin | Expose |
|---|---|
| **Vital** | *Macro 1–4*, *Volume* |
| **Odin 2** | the **XY pad X** and **Y**, **Filter 1 frequency** and its **master volume** (exact names depend on the Odin 2 version; look for "XY", "Filter 1 Freq" and "Master"/"Volume") |
| **Surge XT** | *Macro 1–8* (may show as M1–M8 or under the preset's macro names), *Volume* |
| **Flex** | macro knobs *1–4*, *Volume* |
| **Slicex** | *Volume*, the *Mod X / Mod Y* knobs |
| **FPC** | *Volume* |
| **BASS BUS** (Fruity Balance) | *Volume* |
| **MoshDeck VJ** | *Macro 1 … Macro 16*, *Crossfader A/B*, *Master Brightness* |

**Choose presets whose macros do something dubstep-y.** In Vital and Surge, the factory/community dubstep presets usually put growl, wobble rate, filter and FM/distortion on the macros. If a preset leaves a macro empty, drag the macro onto any knob inside the synth. The rig doesn't care what a macro does, only that it's assigned.

**Odin 2 has no macro knobs; its XY pad plays that role.** In Odin 2's modulation matrix, route **XY X** and **XY Y** to whatever should move. For a reese/neuro bass, good targets are:
- **XY X →** filter 1 frequency and/or oscillator FM / detune amount (the "snarl")
- **XY Y →** LFO 1 frequency (the wobble rate) and/or filter resonance

Save the result as your own Odin 2 preset, so the routing is there every time you load it.

## 6. Load the visuals

Open **MoshDeck VJ** (double-click its box) → **Load Scene** → `dubstep-machine.mdvj`. You get:

- **Decks A/B:** your clips/art, with a datamosh *Pixel Bloom* and *Glitch Slices* waiting on Macros 11 and 16. Deck A clips loop every 2 bars locked to FL, and Deck B's playlist steps every 8 bars.
- **Milk Feedback** ("Bass Tunnel"): eats the decks below it, injects the picture on every kick, moves to the next preset every 32 beats, and pumps with Macro 3 (the bass bus).
- **Circle Spectrum** with Deck B's cover art in the middle, plus **Particles**.
- **Master:** hue shift, beat flash, RGB split, strobe and invert-on-kick.

Load a few video loops or images into **Deck A** and cover art into **Deck B**, then press **Output Window / Fullscreen** for the projector. What each macro does is listed in [CONTROLLER_MAP.md](CONTROLLER_MAP.md#moshdeck-macro-reference).

## 7. The automatic links (Peak, Formula and Balance)

The two Formula Controllers mix three things into one volume each: the crossfader (*a*), the drum sidechain (*b*) and a kill switch (*c*). That gives every volume input exactly one cable.

| Module | Inputs | Formula / settings | Output → |
|---|---|---|---|
| **Peak DRUMS** | audio from FPC | not inverted; DECAY ~ 100–200 ms; TENSION shapes the pump | three cables: **XFADE BASS** input *b*, **XFADE LEAD** input *b*, and **MoshDeck** Macro 1 |
| **Peak BASS** | audio from BASS BUS | not inverted, slow decay | **MoshDeck** Macro 3 (more bass → more footage injected into the Milk feedback) |
| **XFADE BASS** | *a* = CS **XFADE**, *b* = Peak DRUMS, *c* = CS **KILL BASS** | `(1 - a) * (1 - b * 0.8) * c` | **BASS BUS** · Volume (both Vital and Odin 2) |
| **XFADE LEAD** | *a* = CS **XFADE**, *b* = Peak DRUMS, *c* = CS **KILL LEAD** | `a * (1 - b * 0.8) * c` | **Surge XT** · Volume |

- **Crossfader:** left = bass (Vital + Odin 2), right = lead (Surge).
- **Ducking:** the `0.8` sets how deep the bass and lead duck under each kick. Lower it for a gentler pump, or use the SMC-PAD **PUMP** knob, which controls Peak DRUMS' volume.
- **Each synth's own level:** Vital and Odin 2 each keep their own *Volume* for the SMC-PAD level knobs, so you can balance sub against reese inside the bus.

## 8. Control Surface: the hub

Build the Control Surface elements listed in [CONTROLLER_MAP.md](CONTROLLER_MAP.md). Each row there says:

1. which element to create (knob / fader / button, and its name),
2. which hardware control to link to it (right-click the element → **Link to controller** → move the hardware control → Accept),
3. which inputs to cable it to (one element → several cables is what makes the web).

**Jog wheels and encoders** send relative messages. In the *Link to controller* dialog set **Mode** to a relative/jog setting (FL offers jog/relative modes there). Otherwise the value jumps.

**Buttons:**
- Make DROP and the kill switches *latching*, and strobe/flash *momentary*.
- Kill switches must start ON (value 1). They multiply the volume, so 0 = silent.

## 9. Save it

Patcher's menu → **Save preset as…** → `Dubstep Machine.fst`. Saving through the Patcher menu keeps every plugin, cable, controller link and the MoshDeck scene. Next time, drop the preset on a channel and you're back.

## 10. Playing it

1. Drag a loop into **Slicex**, drums/one-shots onto **FPC** pads, and pick presets in **Vital, Odin 2, Surge XT, Flex**.
2. Write a bass line (notes below MIDI 36) in the piano roll. Vital and Odin 2 both play it. Write a lead pattern (notes 84+) for Surge, or just jam on the pads.
3. Press play. MoshDeck follows FL's tempo, and the visuals already react through the audio analysis and the Peak Controllers.
4. Ride the S2:
   - **Mix:** faders and crossfader.
   - **Growl:** filters, EQs and gains. Filter A and the A-side mid EQ move Vital *and* Odin 2 together.
   - **FX:** jogs for scratchy FX.
   - **PLAY A = DROP.**

## Troubleshooting

- **A Vital/Surge macro does nothing:** the preset hasn't assigned it (step 5).
- **Odin 2's XY does nothing:** route XY X/Y in Odin's modulation matrix (step 5) and save it into your preset.
- **The bass is muddy:** high-pass Odin 2 so Vital alone carries the sub (step 3).
- **No bass at all:** KILL BASS is OFF, or the crossfader is fully right (lead side).
- **Visuals don't react to a control:** check the cable goes into the right *MoshDeck Macro N*. Inside MoshDeck, the Modulation Matrix tab lists every Macro → target route; increase its Amount there if the effect is too subtle.
- **Notes trigger the wrong plugin:** recheck the splitter zones and the SMC-PAD bank note numbers (CONTROLLER_MAP.md).
- **Pumping too strong or weak:** the PUMP knob, Peak DRUMS DECAY/TENSION, or the `0.8` in the formulas.
- **CPU:** set MoshDeck's resolution to 960×540 in its top bar, and freeze/bounce what you don't play live.
