# Dubstep Machine: controller map

Every hardware control is linked (FL's **Link to controller**) to one element on the Patcher's **Control Surface**. That element is cabled to the targets listed here. One control → sound **and** picture.

Because the links are made with *Link to controller* (FL listens and learns), you don't need to know which MIDI messages the S2 MK3 sends. Just move the control when FL asks.

Columns: **CS element** = what to create on the Control Surface · **Sound** = Patcher cables to audio plugins · **Visual** = cable to the MoshDeck VJ input.

## Traktor Kontrol S2 MK3

**MIDI mode first:** out of the box the S2 MK3 talks to Traktor (HID), not to FL. It has to send MIDI before FL can see it. Native Instruments provides a MIDI mode for the S2 MK3 (see NI's support pages for the S2 MK3 MIDI mode or Controller Editor for your driver version). You're ready when FL's **Options → MIDI settings** lists it as an input and FL's hint bar shows MIDI activity when you move a fader. Give it **port 1**.

### Mixer

| S2 MK3 control | CS element | Sound | Visual |
|---|---|---|---|
| Channel fader A | fader **LOOPS** | Slicex · Volume | Macro 9: Decks layer opacity |
| Channel fader B | fader **DRUMS** | FPC · Volume | Macro 10: Circle Spectrum visualizer |
| Crossfader | fader **XFADE** | Surge XT · Volume, and → XFADE INV (`1-a`) → Vital · Volume | MoshDeck · **Crossfader A/B** (Deck A ↔ Deck B) |
| Gain A | knob **GROWL** | Vital · Macro 1 | n/a |
| Gain B | knob **SCREECH** | Surge XT · Macro 1 | n/a |
| EQ High A | knob **BASS M3** | Vital · Macro 3 | n/a |
| EQ Mid A | knob **WOBBLE** | Vital · Macro 4 | n/a |
| EQ Low A | knob **STAB 1** | Flex · Macro 1 | n/a |
| EQ High B | knob **LEAD M3** | Surge XT · Macro 3 | n/a |
| EQ Mid B | knob **LEAD M4** | Surge XT · Macro 4 | n/a |
| EQ Low B | knob **STAB 2** | Flex · Macro 2 | n/a |
| Filter A | knob **BASS FILTER** | Vital · Macro 2 | Macro 2: Milk warp |
| Filter B | knob **LEAD FILTER** | Surge XT · Macro 2 | Macro 4: visualizer hue and bass-react |

### Decks

| S2 MK3 control | CS element | Sound | Visual |
|---|---|---|---|
| Tempo fader A | fader **CHOP X** | Slicex · Mod X | Macro 5: Deck A clip speed (center = normal) |
| Tempo fader B | fader **CHOP Y** | Slicex · Mod Y | Macro 6: Milk zoom in/out |
| Jog wheel A | knob **RISER** (relative) | Surge XT · Macro 5 | Macro 7: datamosh motion gain |
| Jog wheel B | knob **STAB 3** (relative) | Flex · Macro 3 | Macro 8: Milk rotation |
| Loop encoder A | knob **LEAD M6** (relative) | Surge XT · Macro 6 | Macro 14: kaleidoscope (more = more segments) |
| Loop encoder B | knob **STAB 4** (relative) | Flex · Macro 4 | n/a |
| Browse encoder | knob **COLOUR** (relative) | Surge XT · Macro 7 | Macro 15: master hue shift |
| PLAY A | button **DROP** (latching) | Surge XT · Macro 8 | Macro 11: **DROP** (Pixel Bloom datamosh + invert on kicks) |
| PLAY B | (same **DROP** button: link both) | as above | as above |
| CUE A / CUE B | button **STROBE** (momentary) | n/a | Macro 12: 8th-note strobe |
| SYNC A / SYNC B | button **FLASH** (momentary) | n/a | Macro 13: white beat flash |
| FX assign buttons | button **GLITCH** (latching) | n/a | Macro 16: glitch slices |

### Performance pads (S2 MK3)

The S2 MK3's pads are used as buttons (linked to Control Surface buttons), so their note numbers don't matter. If your unit shows only 4 pads per deck at once, put the second four on SHIFT.

| Pad | Deck A | Deck B |
|---|---|---|
| 1 | **DROP** (same element as PLAY) | spare: assign your own |
| 2 | **STROBE** | spare |
| 3 | **FLASH** | spare |
| 4 | **GLITCH** | spare |
| 5 | button **KILL DRUMS** (latching, starts ON) → FPC · Volume\* | spare |
| 6 | button **KILL LOOP** → Slicex · Volume\* | spare |
| 7 | button **KILL BASS** → Vital · Volume\* | spare |
| 8 | button **KILL LEAD** → Surge XT · Volume\* | spare |

\* A volume input that already has a fader cabled into it: put the kill button and the fader through a Formula Controller (`a * b`) and cable its output to the volume.

## M-VAVE SMC-PAD

Give it **port 2** in FL's MIDI settings. Set the pad notes in the **M-VAVE app** (or check what it already sends using FL's hint bar), so they land in the right zones of the Keyboard Splitter (see the build guide, step 3).

### Pads (notes, so they play the instruments)

| Bank | Pads 1–16 send | → plays |
|---|---|---|
| **A** | MIDI notes **36–51** | **FPC** pads 1–16 (drums) |
| **B** | MIDI notes **60–75** | **Slicex** slices 1–16 (your loop, chopped) |
| **C** | MIDI notes **52–59** on pads 1–8 (9–16 spare) | **Flex** (stabs / hits; play the preset at different pitches) |
| **D** | free | spare: e.g. extra FPC kits or Slicex slices 17–24 (notes 76–83) |

### Knobs (Link to controller)

| SMC-PAD knob | CS element | Target |
|---|---|---|
| 1 | knob **VITAL LVL** | Vital · Volume (via the XFADE INV formula as a multiplier, `(1-a)*c`) |
| 2 | knob **SURGE LVL** | Surge XT · Volume (via a Formula Controller `a*b` with XFADE) |
| 3 | knob **FLEX LVL** | Flex · Volume |
| 4 | knob **PUMP** | Peak Controller DRUMS · VOL (sidechain depth) |
| 5 | knob **BASS→VISUAL** | Peak Controller BASS · VOL (how hard the bass drives the Milk feedback) |
| 6 | knob **BRIGHTNESS** | MoshDeck · Master Brightness |
| 7 | spare | assign your own |
| 8 | spare | assign your own |

---

## MoshDeck macro reference

This is what `dubstep-machine.mdvj` wires each MoshDeck **Macro** (an FL-automatable parameter) to. You can change any of it in MoshDeck's **Modulation Matrix** tab.

| Macro | Driven by | Visual result |
|---|---|---|
| 1 | Peak DRUMS (automatic) | master Beat Flash + RGB split widen on every kick |
| 2 | Filter A | Milk feedback warp amount |
| 3 | Peak BASS (automatic) | more of your footage injected into the Milk feedback |
| 4 | Filter B | Circle Spectrum hue + bass reaction |
| 5 | Tempo A | Deck A clip speed (±, center = normal) |
| 6 | Tempo B | Milk zoom (±) |
| 7 | Jog A | Pixel Bloom datamosh motion gain |
| 8 | Jog B | Milk layer rotation (±) |
| 9 | Fader A | Decks layer opacity (40 % → 100 %) |
| 10 | Fader B | Circle Spectrum visualizer opacity |
| 11 | DROP | Pixel Bloom datamosh fades in (keyframe every bar) + invert on kicks |
| 12 | STROBE | 8th-note strobe |
| 13 | FLASH | white beat flash |
| 14 | Loop encoder A | kaleidoscope on + more segments |
| 15 | Browse encoder | master hue shift |
| 16 | GLITCH | glitch slices (kick-boosted) |
| Crossfader A/B | Crossfader | Deck A ↔ Deck B |

Always on, with no control needed: the Milk preset changes every 32 beats, Deck A clips loop every 2 bars in time with FL, Deck B's playlist advances every 8 bars, and kicks boost the glitch slices and inject footage into the Milk.

## Optional extra web strands

- **Visuals → sound:** in MoshDeck's *MIDI Learn / Out* tab, *Band energy CCs* sends CC 20–24 (sub/bass/mid/hi-mid/treble) on channel 1. Set MoshDeck's wrapper **MIDI output port** to 5 and Vital's **input port** to 5, then MIDI-learn a Vital macro to CC 21. The bass energy MoshDeck measures now pushes Vital: a deliberate feedback loop, so start with a small macro range.
- **Kick notes:** the same output port carries MoshDeck's kick/snare/hat onset notes (channel 10, notes 36/38/42). Any VST listening on port 5 can retrigger on them.
- **Actions:** if MIDI from the SMC-PAD reaches MoshDeck (set MoshDeck's wrapper **MIDI input port** to 2), you can MIDI-learn bank D pads to *Milk next/random*, *Deck A/B next clip* and *Tap tempo* in MoshDeck's MIDI tab.
