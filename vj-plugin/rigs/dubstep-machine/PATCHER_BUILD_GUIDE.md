# Dubstep Machine: Patcher build guide

One FL Studio **Patcher** that holds the whole audio/visual dubstep rig:

| Plugin | Role in the rig |
|---|---|
| **Slicex** | your main audio loop, chopped into slices you trigger from pads |
| **FPC** | drums: kicks, snares, fills, one-shots on 16 pads |
| **Vital** | the bass (growls, wobbles, subs) |
| **Surge XT** | leads, screeches, risers, FX |
| **Flex** | stabs, hits, pads and atmospheres |
| **MoshDeck VJ** | the visuals, fed by the whole mix and steered by every control |

Plus four stock FL helpers that make the "spiderweb" links: **Control Surface**, **Fruity Peak Controller** (×2), **Fruity Formula Controller**, and **VFX Keyboard Splitter**.

Build it once (about 30 minutes), then save it as a Patcher preset (step 9). After that you only load loops/samples and pick a sound preset in each plugin.

> **Why a guide and not a ready-made `.fst` file?** Patcher presets are an undocumented FL Studio binary format with each plugin's private state inside. A generated one would most likely fail to load or load scrambled. Built by hand following this guide, it's guaranteed to be valid for your FL version and your installed plugins.

---

## The web at a glance

```
                     ┌──────────── VFX Keyboard Splitter ────────────┐
   From FL (MIDI) ──►│ <36 Vital │36-51 FPC │52-59 Flex │60-83 Slicex │84+ Surge XT │
                     └──┬───────────┬──────────┬──────────┬─────────────┬───────────┘
                        ▼           ▼          ▼          ▼             ▼
                      Vital        FPC        Flex      Slicex       Surge XT
          audio:        │           │          │          │             │
                        │           ├──► Peak Controller "DRUMS" ──► ducks Vital + Surge volume
                        │           │                              └► MoshDeck Macro 1 (flash / RGB split)
                        ├──► Peak Controller "BASS" ─────────────────► MoshDeck Macro 3 (Milk inject)
                        ▼           ▼          ▼          ▼             ▼
                        └───────────┴──── all audio summed ───────────┘
                                              ▼
                                        MoshDeck VJ ──► To FL (audio passes through)
                                              ▲
   S2 MK3 + SMC-PAD ─► Control Surface ───────┴──► Vital / Surge / Flex macros, Slicex, FPC,
                       (one control → several targets)   volumes, MoshDeck macros 2, 4–16 + crossfader
```

Every hardware control lands on **one Control Surface element**. That element is cabled to an audio parameter **and** a MoshDeck macro, so each move changes the sound and the picture together. The two Peak Controllers close the loop: the drums duck the bass and flash the visuals, and the bass energy pumps the Milk feedback.

---

## 0. Before you start

1. Install **MoshDeck VJ** (VST3), **Vital** and **Surge XT** (VST3), then run **Options → Manage plugins → Find plugins**.
2. **Connect the controllers:** **Options → MIDI settings**, enable both input devices, and give them different **Port** numbers (S2 MK3 = port 1, SMC-PAD = port 2). Set the controller type to *(generic controller)*. The S2 MK3 must be in MIDI mode; see [CONTROLLER_MAP.md](CONTROLLER_MAP.md#traktor-kontrol-s2-mk3).
3. Load `dubstep-machine.mdvj` into MoshDeck VJ later (step 6).

## 1. Create the Patcher

Add **Patcher** as an *instrument* on a Channel Rack channel (it holds generators). Open it. You start with **From FL** and **To FL**.

Adding modules: right-click empty space → pick a plugin, or drag one in from the browser. Cables: drag from an output port to an input port. Audio ports and event/control ports are different colours. Several cables into one audio input are summed.

## 2. Add the modules

Add these, roughly left to right:

1. **VFX Keyboard Splitter** (if you don't have it, see the *No splitter* note in step 3)
2. **Vital**, **FPC**, **Flex**, **Slicex**, **Surge XT**
3. **Fruity Peak Controller**: add two, rename one **DRUMS** and one **BASS** (right-click module → Rename)
4. **Fruity Formula Controller**: rename it **XFADE INV**
5. **MoshDeck VJ**
6. **Control Surface**

## 3. Notes: split the keyboard so each plugin has its own zone

Cable **From FL (MIDI out) → VFX Keyboard Splitter**. Set up 5 zones and cable each zone output to its plugin:

| Zone (MIDI note numbers) | FL note names* | → plugin | played by |
|---|---|---|---|
| 0 – 35 | up to B2 | **Vital** | piano roll (bass lines; MIDI 29, which FL calls F2, is the classic dubstep root) |
| 36 – 51 | C3 – D#4 | **FPC** | SMC-PAD bank A (16 pads) |
| 52 – 59 | E4 – B4 | **Flex** | SMC-PAD bank C (8 pads) |
| 60 – 83 | C5 – B6 | **Slicex** | SMC-PAD bank B (slices 1–16) + piano roll |
| 84 – 127 | C7 and up | **Surge XT** | piano roll; set Surge's scene **Octave** to −3 so these notes sound in the lead range |

\* FL Studio calls MIDI note 60 "C5". Most other apps and controller editors call it C4. Go by the **numbers**.

- **FPC:** set the 16 pads to notes 36–51 (select a pad and set its note in the pad properties).
- **Slicex:** set its root/first-slice note so slice 1 = note 60 (this is Slicex's default).
- **No splitter?** Cable From FL to all five plugins. Then use separate MIDI channels instead: give the SMC-PAD banks different channels in the M-VAVE app, and put piano-roll notes for Vital/Surge in their own patterns. Expect some overlap.

## 4. Audio cables

1. **Vital, FPC, Flex, Slicex, Surge XT (audio out) → MoshDeck VJ (audio in).** All five go into the same input and are summed.
2. **MoshDeck VJ (audio out) → To FL.** MoshDeck passes audio through untouched, so this is your master mix.
3. **FPC (audio out) → Peak Controller DRUMS (audio in)**: a second cable from the same FPC output.
4. **Vital (audio out) → Peak Controller BASS (audio in).**

The Peak Controllers produce only control signals, so their audio outputs stay unconnected.

## 5. Exposing plugin parameters as Patcher inputs

To cable a control into a plugin parameter, the parameter needs an input port on the plugin's box:

- Right-click the plugin's box in Patcher and tick the parameter in its list of inputs. Parameters you moved recently appear first; if one is missing, wiggle it inside the plugin, then look again.
- For **Vital** expose *Macro 1–4* and *Volume*.
- For **Surge XT** expose *Macro 1–8* (they may show as M1–M8 or under the preset's macro names) and *Volume*.
- For **Flex** expose its macro knobs *1–4* and *Volume*.
- For **Slicex** expose *Volume* and the *Mod X / Mod Y* knobs.
- For **FPC** expose *Volume*.
- For **MoshDeck VJ** expose *Macro 1 … Macro 16*, *Crossfader A/B* and *Master Brightness*.

**Choose presets whose macros do something dubstep-y.** In Vital and Surge, the factory/community dubstep presets usually put growl, wobble rate, filter and FM/distortion on the macros. If a preset leaves a macro empty, drag the macro onto any knob inside the synth. The rig doesn't care what a macro does, only that it's assigned.

## 6. Load the visuals

Open **MoshDeck VJ** (double-click its box) → **Load Scene** → `dubstep-machine.mdvj`. You get:

- **Decks A/B:** your clips/art, with a datamosh *Pixel Bloom* and *Glitch Slices* waiting on Macros 11 and 16. Deck A clips loop every 2 bars locked to FL, and Deck B's playlist steps every 8 bars.
- **Milk Feedback** ("Bass Tunnel"): eats the decks below it, injects the picture on every kick, moves to the next preset every 32 beats, and pumps with Macro 3 (bass).
- **Circle Spectrum** with Deck B's cover art in the middle, plus **Particles**.
- **Master:** hue shift, beat flash, RGB split, strobe and invert-on-kick.

Load a few video loops or images into **Deck A** and cover art into **Deck B**, then press **Output Window / Fullscreen** for the projector. (What each macro does is listed in [CONTROLLER_MAP.md](CONTROLLER_MAP.md#moshdeck-macro-reference).)

## 7. The automatic links (Peak & Formula controllers)

| From | → To | Settings | Effect |
|---|---|---|---|
| **Peak DRUMS** *Peak* out | **Vital** Volume, **Surge XT** Volume | inverted: VOL negative / BASE up, TENSION for the pump curve, DECAY ~ 100–200 ms | sidechain pump; the bass ducks under every kick |
| **Peak DRUMS** *Peak* out | **MoshDeck** Macro 1 | not inverted | kick → flash + RGB split |
| **Peak BASS** *Peak* out | **MoshDeck** Macro 3 | not inverted, slow decay | heavier bass → more of your footage injected into the Milk feedback |
| **Control Surface** XFADE → **XFADE INV** input *a* | formula `1 - a` → **Vital** Volume | | the crossfader becomes a bass↔lead fader |

**Two cables into one Volume input:** if Patcher won't let the DRUMS peak and XFADE INV both drive Vital's volume, feed the peak into the Formula Controller as *b* instead. Use the formula `(1 - a) * (1 - b * 0.8)` and cable the one result to Vital Volume. Do the same for Surge with `a * (1 - b * 0.8)`.

## 8. Control Surface: the hub

Build the Control Surface elements listed in [CONTROLLER_MAP.md](CONTROLLER_MAP.md). Each row there says:

1. which element to create (knob / fader / button, and its name),
2. which hardware control to link to it (right-click the element → **Link to controller** → move the hardware control → Accept),
3. which inputs to cable it to (one element → several cables is what makes the web).

**Jog wheels and encoders** send relative messages. In the *Link to controller* dialog set **Mode** to a relative/jog setting (FL offers jog/relative modes there). Otherwise the value jumps.

**Buttons:** make them *latching* for DROP/kill switches and *momentary* for strobe/flash. Kill switches should start ON (value 1), since they're cabled to volumes.

## 9. Save it

Patcher's menu → **Save preset as…** → `Dubstep Machine.fst`. Saving through the Patcher menu keeps every plugin, cable, controller link and the MoshDeck scene. Next time, drop the preset on a channel and you're back.

## 10. Playing it

1. Drag a loop into **Slicex**, drums/one-shots onto **FPC** pads, and pick presets in **Vital, Surge XT, Flex**.
2. Write a bass line (notes below MIDI 36) for Vital and a lead pattern (notes 84+) for Surge in the piano roll, or just jam on the pads.
3. Press play. MoshDeck follows FL's tempo, and the visuals already react through the audio analysis and the Peak Controllers.
4. Ride the S2: faders and crossfader for the mix, filters/EQs/gains for the growl, jogs for scratchy FX, **PLAY A = DROP**.

## Troubleshooting

- **A Vital/Surge macro does nothing:** the preset hasn't assigned it (step 5).
- **Visuals don't react to a control:** check the cable goes into the right *MoshDeck Macro N*. Inside MoshDeck, the Modulation Matrix tab lists every Macro → target route; increase its Amount there if the effect is too subtle.
- **Notes trigger the wrong plugin:** recheck the splitter zones and the SMC-PAD bank note numbers (CONTROLLER_MAP.md).
- **Pumping too strong or weak:** Peak DRUMS VOL/TENSION/DECAY.
- **CPU:** set MoshDeck's resolution to 960×540 in its top bar, and freeze/bounce what you don't play live.
