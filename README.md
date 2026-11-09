# Mini Moop

Mini Moop is a synthesizer plugin that simulates the Minimoog Model D at the level of its electronic components. It is available as a **VST3** plugin, an **AU** (Audio Unit) plugin and a **standalone** application. On top of the original monophonic instrument, it adds an optional polyphonic mode.

### How the simulation works

Most software synthesizers imitate the *sound* of an instrument with signal-processing recipes tuned by ear. Mini Moop instead imitates the *circuit*: it takes the electronic schematics of the original and calculates, many thousands of times per second, what every part of the circuit does.

- **Each block of the schematic is rebuilt from its parts.** The oscillators, the famous transistor ladder filter, the envelopes, the amplifier, the mixer and the noise source are each described by their actual components: transistors, diodes, capacitors, resistors and op-amps, with values taken from the service manual.
- **The components behave like the real ones.** A transistor is not a perfect switch or amplifier: its behaviour depends on its temperature and on small manufacturing differences. These imperfections are modelled, and they are what give the instrument its character, such as the warm overdrive when the mixer is pushed or the way the filter whistles at high resonance.
- **Every voice is a slightly different instrument.** Real components are never exactly at their nominal value. Each voice draws its own set of component values, then tunes itself the way a technician would calibrate a real unit. Changing the "Unit" setting is like playing another example of the instrument.
- **Tuning drift comes from physics, not from a random wobble.** The circuits warm up and cool down slightly over time, and the pitch drifts as a consequence, as on the original.
- **The models are checked against a reference.** The component models are compared with ngspice, a standard circuit simulator used by electronics engineers, and match it closely.
- **One specific version of the instrument.** The Model D's oscillator board went through three revisions. Mini Moop simulates **revision 2** (the "old board" with a CA3046 transistor array, serial numbers 1237 to 10174). See [Which revision and which boards are simulated](#which-revision-and-which-boards-are-simulated).

Minimoog and Model D are trademarks of Moog Music Inc. Mini Moop is an independent personal project, not affiliated with or endorsed by Moog Music, and uses no original logo or artwork.

Built with JUCE 9.0.5. Licensed under the GNU AGPLv3 or later (see [License](#license)).

## Building (macOS, Command Line Tools are enough)

```sh
brew install cmake ninja ngspice          # ngspice: only needed for the validation tests
git clone --depth 1 -b 9.0.5 https://github.com/juce-framework/JUCE.git deps/JUCE                # optional, otherwise FetchContent
git clone --depth 1 -b v3.16.1 https://github.com/catchorg/Catch2.git deps/Catch2           # optional
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
(cd build/release && ctest --output-on-failure)
```

At the end of the build, the VST3 and AU are copied to `~/Library/Audio/Plug-Ins/{VST3,Components}`. The Standalone app is in `build/release/MiniMoop_artefacts/Release/Standalone/`.

### Universal binaries (Apple Silicon and Intel)

By default the build produces **universal** binaries containing both an Apple Silicon (`arm64`) and an Intel (`x86_64`) version, for macOS 11 or later. Check with:

```sh
lipo -archs ~/Library/Audio/Plug-Ins/VST3/Mini\ Moop.vst3/Contents/MacOS/Mini\ Moop   # x86_64 arm64
```

During development, building only for your own machine is about twice as fast. Use a separate build folder so the universal build is not overwritten:

```sh
cmake -S . -B build/arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/arm64
```

The architecture setting is stored in the CMake cache, so changing it requires a new build folder (or deleting the existing one). Note that every build copies its plugins to `~/Library/Audio/Plug-Ins`, so an `arm64`-only build replaces the installed universal plugins until you rebuild `build/release`.

To test the Intel version on an Apple Silicon Mac, install Rosetta (`softwareupdate --install-rosetta --agree-to-license`) and prefix commands with `arch -x86_64`, for example `arch -x86_64 build/release/minimoop_tests`. If `arch -x86_64 auval` reports "didn't find the component" right after installing, run `killall -9 AudioComponentRegistrar` to refresh the Audio Unit registry.

`build/release/MiniMoopSnapshot_artefacts/Release/MiniMoopSnapshot panel.png` renders the panel to an image without a host.

If the link step fails with "linking with album file" after a Command Line Tools update, delete `build/release` and build again from scratch. LTO objects built by an older compiler cannot be linked by the new linker.

## Architecture

```
src/circuit/components   Component models: diode, BJT (Ebers-Moll/Gummel-Poon, as in SPICE),
                         741/301 op-amp (GBW, slew, ±10 V rails), potentiometers (tapers), tolerances
src/circuit/solver       MnaCircuit<N>: modified nodal analysis + Newton-Raphson + pnjlim limiting
src/circuit/blocks       One block per part of the schematic (see below)
src/engine               Voice (one complete "unit"), mono/poly allocation, thermal drift,
                         decimation (2x/4x/8x oversampling), SynthEngine
src/                     JUCE plugin: parameters, processor, sound banks (SoundBank)
src/ui                   Vector panel (no bitmaps), resizable, sound bank manager
presets                  Built-in Factory sound bank (JSON)
tests/                   Catch2 tests, including direct comparisons with ngspice
```

The engine (`minimoop_dsp`) is plain C++20 with no JUCE dependency. After `prepare()` it allocates no memory; a test checks this.

### "Hybrid component-level" simulation

Each block's equations are derived from the physical models of its components. The block is then solved implicitly, either with the trapezoidal rule and Newton-Raphson or by exact integration of RC segments. Blocks exchange voltages with each other, as on the real circuit boards.

| Block | Board | What is simulated |
|---|---|---|
| `ExpoConverter` | 1 (and 4) | Revision 2 CA3046 pair with tempco on board 1; the same model with estimated values for the filter cutoff on board 4. Transistor pair: Vt = kT/q at each transistor's temperature, Is temperature law (SPICE XTI/EG), +3500 ppm/K tempco resistor, Is mismatch, emitter resistance, V/oct, offset and HF trimmers |
| `VcoCore` | 1 | Expo current integrated on a 0.01 µF polystyrene capacitor (ramp from 0 to −4 V), comparator and FET reset with dead time (high notes go flat, compensated by the HF trim), shapers for triangle, shark-tooth, reverse ramp and 3 rectangles with output-stage slew. polyBLEP correction at the exact instant of each discontinuity |
| `LadderFilter` | 4 | 4 differential-pair stages (Ebers-Moll equations), 68 nF ±10 % capacitors, base current losses (β), pair offsets, zero-delay Emphasis feedback (solved implicitly), transistor noise, output op-amp |
| `Mixer` | front panel | 25 kΩ linear volume pots loaded by 33 kΩ summing resistors (11 kΩ for noise), solved with Millman's theorem. The "pushed mixer" saturation happens in the ladder's input pair (tanh) |
| `ContourGen` | 2 | 10 µF electrolytic timing capacitor, 1 MΩ audio-taper pots, end-of-attack comparator, Decay switch, leakage |
| `Vca` | 4 | Differential pair + V→I converter, control feedthrough ("thump"), 80 dB range |
| `NoiseSource` | 3 | Junction noise (Gaussian + 1/f + popcorn), −3 dB/oct RC network for pink noise |
| `Glide` | 2 | RC on 1 µF (C6), 5 MΩ glide pot |
| `DriftModel` | 1 and 4 | Board temperature (Ornstein-Uhlenbeck), local hot spots, temperature difference within each expo pair. Tuning drift **results** from the physics; it is not an added LFO |

Each voice draws its own component values from the "Unit" seed, then is **calibrated the way a technician would**: offset at A4, V/oct between A1 and A4, HF trim at A7, filter scale. Changing the "Unit" is like playing a different example of the instrument.

### Validation

- The diode and BJT models and the MNA solver are compared with **ngspice** on identical netlists: RC filter, diode clipper, differential pair. RMS error is below 0.5 %.
- The reduced ladder is compared with a **full transistor-level ladder simulated in ngspice** (10 NPN, 40 mV input, feedback k = 2.5). RMS error is about 0.01 %.
- After calibration, tuning is within 1 cent at the calibration points and within 3 cents over 6 octaves, for 4 randomly drawn units.
- The filter self-oscillates at its cutoff frequency. It tracks the keyboard at 1 octave per octave with KBD 1+2. Cutoff spans 14 Hz to 20 kHz.
- Sawtooth aliasing at 3.5 kHz with 4x oversampling stays below −60 dB.
- Nothing is allocated during audio rendering.
- pluginval at strictness level 8: the VST3 passes consistently. Apple's `auval` passes for the AU.
- pluginval on the AU fails intermittently (about 1 run in 2) on the state-restoration tests, each time on a different random parameter. This is a timing race in the validator's AU host: JUCE's AU hosting code caches parameter values and updates that cache from queued main-thread notifications, which can arrive after a state restore and overwrite it with old values. The same seed passes on some runs and fails on others, and the failure is identical with JUCE 8.0.17 and 9.0.5.

### CPU load (M3 Pro, 48 kHz)

| Quality | 1 voice | 8 voices |
|---|---|---|
| Eco 2x | ~2.5 % | ~20 % |
| High 4x | ~4.2 % | ~33 % |
| Ultra 8x | ~8 % | ~66 % |

## Which revision and which boards are simulated

Figure, table and circuit numbers below refer to the Minimoog Model D service manual.

### Oscillator board revision: revision 2

The oscillator board (board 1) exists in three revisions, and they do not drift or track in the same way:

| Revision | Serial numbers | Expo converter | Simulated |
|---|---|---|---|
| 1, "discrete" | up to 1236 | discrete 2N4058 PNP pairs, no temperature compensation | no |
| **2, "old board"** | **1237 to 10174** | **RCA CA3046 transistor array at ambient temperature, 1 kΩ +3500 ppm/K tempco resistor (Fig. 9-3, 9-4)** | **yes** |
| 3, "new board" | 10175 and above | Fairchild µA726 heated pair held at about 80 °C, no tempco (Fig. 9-1, 9-2) | no |

Mini Moop models revision 2. Each oscillator has a 741 CV summer (IC1, IC4, …) with a 1 kΩ tempco resistor in its feedback (R20, R71, …) and 51.1 kΩ 1 % inputs (R27, …). It drives the CA3046 expo pair (IC2), whose reference transistor current is set by R50 = 39 kΩ with its collector held at −4 V by a 741 (IC3), about 103 µA. The pair sits at the temperature of the board, and the drift that results is what `DriftModel` reproduces. The 94-001 modification kit for revision 2 boards is not modelled.

### The six boards and the blocks that simulate them

| Board | Moog reference | Circuit (service manual) | Simulated by | Not simulated |
|---|---|---|---|---|
| **1** Oscillator (rev 2) | 93-113, Fig. 9-3 / 9-4 | 3 × CV summer, CA3046 expo converter, sawtooth core (0.01 µF, FET reset, 0 to −4 V ramp), waveshapers | `ExpoConverter` (summer reduced to its gain and tempco), `VcoCore` | the summers' op-amp dynamics |
| **2** Contour generator and keyboard | 93-114, Fig. 9-6 / 9-7 | circuit 8: two contour generators (C5 / C2 = 10 µF) | 2 × `ContourGen` (filter, loudness) | |
| | | circuit 1: keyboard circuit, glide (C6 = 1 µF) | `Glide` | 8.33 mA current source, 43 × 10 Ω resistor string and sample & hold (C9). MIDI notes give an ideal 1 V/oct CV; lowest-note priority is handled by `VoiceAllocator` |
| **3** Power supply | 93-115, Fig. 9-8 / 9-9 | circuit 3: noise generator (white and pink) | `NoiseSource` | |
| | | circuit 15: modulation mix amplifier | in `Voice` (ideal crossfade between osc 3 and noise / filter contour) | the amplifier circuit itself |
| | | circuit 10: ±10 V regulators; circuit 11: headphone amplifier | | rails are ideal, so supply-borne oscillator bleed is absent; no headphone stage |
| **4** Filter and VCA | 93-116, Fig. 9-10 / 9-11 | circuit 6: ladder filter (C3, C7, C11, C16 = 0.068 µF), cutoff expo pair Q26 / Q28, Emphasis | `LadderFilter`, `ExpoConverter` (cutoff) | |
| | | circuit 7: voltage controlled amplifier | `Vca` | |
| | | circuit 12: external input preamp and overload lamp; circuit 13: A-440 reference oscillator | | not simulated (the External Input is mixed directly) |
| **5** Rectifier | 93-118, Fig. 9-13 | bridge rectifier, filter capacitors, fuses | | not simulated (no audible effect) |
| **6** Octave buffer | 93-119, Fig. 9-14 | three 741 followers buffering the range switches | | not simulated: range switching is ideal |
| Front panel | Fig. 9-16, Tables 7-2 / 7-3 | passive mixer (pots, on/off switches, summing resistors), controls | `Mixer`, pot tapers in each block | |
| Left-hand controller | 93-060, Fig. 9-12 | pitch and modulation wheels | ideal pitch bend and modulation amount in `Voice` | |

`DriftModel` (in `src/engine`) is not a board: it supplies the temperatures of boards 1 and 4 to the expo pairs and the ladder.

## Where the values come from: sourced or assumed

**Sourced** (Model D service manual, OCR of the parts lists, specifications):
- ±10 V supply rails;
- sawtooth core with FET reset, 0.01 µF polystyrene capacitor and a ramp from 0 to −4 V (Fig. 9-3);
- expo converter of board 1 rev 2 (Fig. 9-3): 1 kΩ tempco resistor R20, 51.1 kΩ CV input R27, reference current of about 103 µA (−4 V across R50 = 39 kΩ);
- ladder with 4 × 68 nF ±10 % (C3, C7, C11, C16, Fig. 9-11);
- self-oscillation at Emphasis 7–8;
- filter contour over 0 to 4 octaves, KBD 1/3 and 2/3;
- attack from 1 ms to 10 s and decay from 4 ms to 35 s;
- glide on 1 µF (C6, Fig. 9-7) with a 5 MΩ glide pot (R2, Table 7-3);
- mixer on the front panel (Fig. 9-16, Tables 7-2 / 7-3): 25 kΩ linear volume pots (R6, R7, R8, R10), 33 kΩ summing resistors (R45, R46, R47, R49) and 11 kΩ for noise (R48), external input volume 1 MΩ audio taper (R9) buffered by the board 4 preamp;
- contour timing capacitor 10 µF (C5 / C2, Fig. 9-7), 100 Ω attack series resistor (R7 / R42), 1 MΩ audio-taper Attack and Decay pots (Table 7-3);
- 50 kΩ audio-taper mod wheel and modulation injection over 0 to 1¼ octaves;
- linear VCA over 80 dB;
- pink noise at −3 dB/oct;
- lowest-note priority, single trigger;
- rectangle duty cycles of about 50 / 30 / 15 %;
- reverse ramp on oscillator 3.

**Assumed or estimated** (no source found; these are the parameters to refine first if you have the scanned schematics in front of you):
- mixer node resistance (4.7 k) and ladder input gain (set for mild saturation with one oscillator at 10);
- noise level reaching the mixer (scaled so that the 11 kΩ noise resistor keeps the earlier noise/oscillator balance);
- curve of the glide pot's "No. 1 taper" (modelled as an audio taper);
- Emphasis network and filter output amp values (board 4 not yet checked against Fig. 9-11);
- filter cutoff expo converter values (reference current, input resistor);
- contour decay path at minimum setting and release resistance (derived from the 4 ms decay spec);
- VCO reset dead time (1.6 µs);
- shark-tooth mix weighting;
- filter modulation depth (2 octaves);
- Frequency knob range (±7 semitones);
- pink network values;
- LO range (−7 octaves).

All these values live in the `*Params` structs in `src/circuit/blocks/`.

## Added controls (not on the original)

- **Voices**: mono (faithful) or poly with 2 to 8 voices.
- **Quality**: 2x, 4x or 8x oversampling.
- **Drift**: from 0 (stable) to 3 (hot stage).
- **Unit**: tolerance seed, i.e. which example of the instrument you are playing.
- **Bend**: pitch bend range.
- **Feedback**: the "output → External Input" patch; the level is set with the mixer's Ext. Input volume.

## Sound banks

Programs come from **sound banks**: each bank holds presets grouped by category and is stored as one JSON file. The PROGRAM display and the host's program list show the presets of the **active bank**, numbered in category order.

- The **Factory** bank (`presets/Factory.json`) is built into the plugin and is read-only. It is active by default.
- Banks live in `~/Library/Application Support/Zazart/Mini Moop/presets/`. Factory is mirrored there so it can be seen and copied. A copy edited by hand is restored from the built-in version.
- The **Sound Banks** screen (burger menu, left of the PROGRAM display) shows the banks and their content. From it you can activate a bank and load a preset. You can also create, duplicate, rename, delete (to the Trash), import and export banks, save the current sound into a user bank, and rename or delete presets.
- Each project remembers its active bank. The last bank you activate becomes the default for new instances.

Format (values in parameter units, keyed by parameter ID; a parameter that is not listed takes its default value, and Unit, Quality and Drift are never stored):

```json
{
  "format": "MiniMoopBank", "version": 1,
  "name": "My Bank", "author": "", "description": "",
  "categories": [
    { "name": "Bass", "presets": [ { "name": "Deep", "values": { "cutoff": 3.5, "osc1Wave": 2 } } ] }
  ]
}
```

Rocker switches are two-state choice parameters (Off/On) rather than boolean parameters, so they survive a host state save and restore unchanged (required to pass pluginval).

## License

Mini Moop is licensed under the **GNU Affero General Public License v3.0 or later** (`AGPL-3.0-or-later`): you may use it under the terms of version 3 or, at your option, any later version published by the Free Software Foundation. The full text of version 3 is in [`LICENSE`](LICENSE).

It is built on the JUCE framework, which is dual-licensed under the AGPLv3 and the commercial [JUCE 9 licence](https://juce.com/legal/juce-9-licence/). This project uses JUCE under the AGPLv3.

Third-party components keep their own licences:
- JUCE 9.0.5: AGPLv3 / commercial JUCE licence
- Catch2 3.16.1 (tests only): Boost Software License 1.0
