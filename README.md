# Mini Moop

Mini Moop is a synthesizer plugin that simulates the Minimoog Model D at the level of its electronic components. It is available as a **VST3** plugin, an **AU** (Audio Unit) plugin and a **standalone** application. On top of the original monophonic instrument, it adds an optional polyphonic mode.

### How the simulation works

Most software synthesizers imitate the *sound* of an instrument with signal-processing recipes tuned by ear. Mini Moop instead imitates the *circuit*: it takes the electronic schematics of the original and calculates, many thousands of times per second, what every part of the circuit does.

- **Each block of the schematic is rebuilt from its parts.** The oscillators, the famous transistor ladder filter, the envelopes, the amplifier, the mixer and the noise source are each described by their actual components: transistors, diodes, capacitors, resistors and op-amps, with values taken from the service manual.
- **The components behave like the real ones.** A transistor is not a perfect switch or amplifier: its behaviour depends on its temperature and on small manufacturing differences. These imperfections are modelled, and they are what give the instrument its character, such as the warm overdrive when the mixer is pushed or the way the filter whistles at high resonance.
- **Every voice is a slightly different instrument.** Real components are never exactly at their nominal value. Each voice draws its own set of component values, then goes through the adjustment procedures of Moog's service manual, trimmer by trimmer, as a technician would calibrate a real unit. Changing the "Unit" setting is like playing another example of the instrument.
- **Tuning drift comes from physics, not from a random wobble.** The circuits warm up and cool down slightly over time, and the pitch drifts as a consequence, as on the original.
- **The models are checked against the documentation.** The circuits are transcribed from the service manual's schematics into netlists for ngspice, a standard circuit simulator used by electronics engineers, and the models are compared with it. The finished units are then put through the service manual's own acceptance checks (tuning, ranges, levels).
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
                         741/301 op-amp (GBW, slew, ±10 V rails; not used by the blocks yet), potentiometers (tapers), tolerances,
                         band-limited steps and ramps (BLEP / BLAMP)
src/circuit/solver       MnaCircuit<N>: modified nodal analysis + Newton-Raphson + pnjlim limiting
                         (used by the tests to check the device models against ngspice)
src/circuit/blocks       One block per circuit of the schematic (see "Which revision and which boards are simulated")
src/engine               Voice (one complete "unit": component draw, bench calibration, rendering),
                         mono/poly allocation, thermal drift, 2x/4x/8x oversampling, SynthEngine
src/                     JUCE plugin: parameters, processor, sound banks (SoundBank)
src/ui                   Vector panel (no bitmaps), resizable, sound bank manager
presets                  Built-in Factory sound bank (JSON)
tools/spice              The netlists transcribed from the service manual's figures, the benches run on
                         them and the generator of the tables the blocks tabulate (see its README)
tests/                   Catch2 tests: the service manual's checks, comparisons with ngspice
```

The engine (`minimoop_dsp`) is plain C++20 with no JUCE dependency. After `prepare()` it allocates no memory; a test checks this. Drawing and calibrating the eight units (`build`) takes about 0.35 s: it renders short bench measurements (the filter singing, noise through the whole chain, read at its real level), and is done when the plugin loads and when Unit changes.

### Validation

Each claim below is checked by a test in `tests/` (Catch2), run by `ctest` or `build/release/minimoop_tests`. A tag runs a group, for example `minimoop_tests "[manual]"`. The tests tagged `[spice]` run ngspice at test time and are skipped when it is not installed; the others that compare with ngspice hold its results as reference values (the netlists and benches that produced them are in `tools/spice`).

**The service manual's checks** (`test_service_manual.cpp`, tag `[manual]`). Section numbers are the service manual's; "Folkman" is R. J. Folkman's field service manual. Levels in dB are read as on a meter whose 0 dB is 0.775 V.

| Check | Manual | Simulated units |
|---|---|---|
| Oscillator range and scale (5.31 to 5.33; Folkman) | 3520 Hz on high A and 440 Hz on low A with RANGE at 2' | within 2 cents on both; a few cents in between and up to about 10 cents in the low octaves, unit by unit |
| Octave adjust (Folkman; 5.34) | oscillator 1 at 8' zero-beats two octaves below 2' (the later 5.34 uses 16') | within 1 cent; the other steps and oscillators within 5 to 10 cents; LO 4.75 octaves below 32' |
| Range of TUNE and FREQUENCY (5.35) | 4 to 6 and 14 to 17 semitones | 5.4 and 16.8 with nominal pots (15 to 18 with 20 % pots) |
| Oscillator 3 wide range (5.36) | OSC. 3 CONTROL off, LO, FREQUENCY at minimum: a click every 2 to 5 s; the keyboard has no effect; LO overlaps 32' | a click every 3 to 3.5 s; FREQUENCY spans 6 octaves |
| Modulation of the oscillators (5.37) | 13 to 23 semitones with the MOD wheel fully up | 18.9 |
| Amount of modulation (5.19) | the filter from 440 Hz to at least 2.4 kHz | 5 octaves |
| Emphasis calibration (5.12; Folkman) | regeneration starts between 7 and 8 | does not at 7, does at 8, on every unit |
| Filter cutoff (5.13; Folkman) | 440 Hz at -1; at least 16 kHz fully clockwise; still singing under 300 Hz | within 5 cents; 28 kHz; sings at 220 Hz |
| Filter scale, KEYBOARD 1/3 (5.14, 5.15) | two then three octaves of keys track with switches 1 and 2; 880 +/- 50 Hz three octaves up with switch 1 | within 10 and 40 cents; within the 50 Hz |
| Amount of contour, filter control (5.17, 5.18) | fully clockwise: "35kHz +/- 10kHz"; two octaves for about two divisions | 38 to 40 kHz (the converter's limit); 1.96 octaves |
| Levels (5.8, 5.27, 5.7, 5.24) | triangle +1 +/- 3 dB; white and pink noise -5 +/- 3 dB; A-440 -8 +/- 2 dB at 440 +/- 1 Hz; residual noise -65 dB | +2.5 dB; -8.0 and -2.0 dB (read over 48 kHz, as a wideband meter; this unit's two lie 5.9 dB apart, close to the 6 dB the check allows); -8.0 dB; -80 dB |
| Contour generator balance (5.25) | -55 dB maximum | -63 dB |
| Keyboard and glide (2.19, specifications) | 1 V an octave; 1 ms to 1 s an octave; the pitch is held when the key is released | GLIDE 10: 0.59 s an octave up, 1.06 down (ngspice on the drawn circuit: 0.58 and 1.07); held, drifting by a few cents a minute |
| Short term oscillator stability (specifications) | better than 0.25 % | 3.6 cents over a minute at Drift 1 |

**The models against ngspice and against the drawn networks**

| What is checked | Result | Test (`tests/…`: test case) |
|---|---|---|
| Diode and BJT models and the MNA solver against ngspice on identical netlists | RMS error below 0.5 % | `test_spice_validation.cpp`: "RC low-pass matches ngspice", "Diode clipper matches ngspice", "BJT differential pair with collector capacitor matches ngspice" |
| Solver and helpers | LU with pivoting, RC step against the analytic solution, deterministic tolerance draws, audio taper | `test_solver.cpp` |
| Reduced ladder against a transistor-level ladder in ngspice (10 NPN, 40 mV input, k = 2.5) | RMS error about 0.01 % | `test_ladder.cpp`: "Ladder reduced model matches transistor-level ngspice ladder" |
| Ladder small-signal response; self-oscillation | -12 dB at the cutoff; oscillates at the cutoff | `test_ladder.cpp`; `test_voice.cpp`: "Filter self-oscillates with emphasis at 10 and no input" |
| EMPHASIS: the drawn feedback path and REGEN CAL's range | loop gain 0.17 at 0; 2.3 to 9.2 at 10 over the trimmer; the trimmer can put the threshold at 7.5 | `test_ladder.cpp`: "EMPHASIS: the drawn feedback path, REGEN CAL's range" |
| Filter output stage (Q5 to Q8) against ngspice; passband gain against the whole of circuit 6 | table within 20 mV; 56.7; clipping at +6.4 / -3.7 V | `test_filter_output.cpp` (four test cases) |
| VCA (circuit 7) against ngspice: Q18's current, gain against the contour, clipping | within 3 %; within 1 dB (1.97 at a 5 V contour); +/-2.8 V against +3.05 / -2.84 | `test_vca.cpp` (three test cases) |
| Contour levels against ngspice; times from the drawn networks | rest, peaks (5.22 V loudness, 4.43 V filter) and the levels held at six SUSTAIN settings within 0.04 V of `contour.cir`; 1.4 ms to 11 s | `test_contour.cpp` |
| Oscillator aliasing in the audio band, against the fundamental | sawtooth and rectangles below -100 dB at 3.5 and 7 kHz (2x and 4x); triangle below -75 dB | `test_vco.cpp`: "Band-limited steps and corners keep aliasing low at high pitch" |
| Waveform levels, the triangle's shape and the rectangle widths | +/-2.09 V sawtooth, +/-1.75 V triangle, 3.5 V rectangle; the triangle within 40 mV of ngspice's transfer; 50.1, 31.7 and 19.9 % (ngspice) | `test_vco.cpp` |
| Shark tooth's mix and the outputs' loading of the VOLUME pot | 17.8 % sawtooth; -4.2 / -1.3 / -0.4 dB at VOLUME 10 (shark tooth, sawtooth, triangle) | `test_vco.cpp`: "Shark tooth: the front panel's R030 / R031 against the board's outputs" |
| KEYBOARD CONTROL, modulation line and its polarity through the whole instrument | 1/3, 2/3 and 1 octave an octave; +-0.74 octave at full wheel, half way 54 %; filter 3 times as deep; a rising oscillator 3 lowers the pitch | `test_service_manual.cpp` |
| Pink noise against white; red against pink | the drawn network's transfer within 1 dB from 200 Hz to 10 kHz; ngspice's within 1 dB from 20 to 500 Hz | `test_service_manual.cpp`: "Pink and red noise follow board 3's drawn networks" |
| External input pre-amp and OVERLOAD lamp; its interpolation to the oversampled rate | gain 201, rails +9.8 / -7.7 V, lamp from about 2.1 V; images below -80 dB | `test_extpreamp.cpp`; `test_service_manual.cpp` |
| MODULATION MIX, main VOLUME, A-440, noise levels independent of the sample rate | as wired on the original | `test_noise_a440.cpp` |
| Voice behaviour: level, extreme settings stay finite, poly, MOD. AMOUNT | | `test_voice.cpp` |
| No memory allocated during audio rendering, note events and panel changes | | `test_voice.cpp`: "Rendering, note events and panel changes do not allocate" |
| CPU load, 1 and 8 voices at 2x, 4x and 8x | reported (table below); one voice at 4x must stay under 10 % of a core | `test_voice.cpp`: "CPU load: 1 and 8 voices at 2x, 4x and 8x oversampling" |
| Sound banks: parsing, round trip, invalid banks, library, import / export | | `test_soundbank.cpp` (`minimoop_bank_tests`) |

### CPU load (Apple M3 Pro, 48 kHz)

Share of one core, real time being 100 %: three oscillators, EMPHASIS 6, DRIFT 1; 8 voices are an 8-note chord in POLY. Measured by `test_voice.cpp`: "CPU load: 1 and 8 voices at 2x, 4x and 8x oversampling" (run `minimoop_tests "[perf]"`; the figures vary by about 10 % from run to run).

| Quality | 1 voice | 8 voices |
|---|---|---|
| Eco 2x | ~2.5 % | ~21 % |
| High 4x | ~5 % | ~39 % |
| Ultra 8x | ~9 % | ~77 % |

### Outside the test suite

- pluginval at strictness level 8: the VST3 passes consistently. Apple's `auval` passes for the AU.
- pluginval on the AU fails intermittently (about 1 run in 2) on the state-restoration tests, each time on a different random parameter. This is a timing race in the validator's AU host: JUCE's AU hosting code caches parameter values and updates that cache from queued main-thread notifications, which can arrive after a state restore and overwrite it with old values. The same seed passes on some runs and fails on others, and the failure is identical with JUCE 8.0.17 and 9.0.5.

## Which revision and which boards are simulated

Figure, table and circuit numbers below refer to the Minimoog Model D service manual.

A **board** is a physical part of the instrument: one of its six printed circuit boards, numbered 1 to 6 in the service manual (plus the front panel and the left-hand controller). A board carries several *circuits* (the service manual numbers them too: board 4 holds circuit 6, the filter, and circuit 7, the VCA, among others). A **block** is the code that simulates one of these circuits: a C++ class in `src/circuit/blocks/`, such as `LadderFilter` for circuit 6 or `Vca` for circuit 7. A board is therefore simulated by several blocks, and one block can serve more than once (`ExpoConverter` for the three oscillators, `ContourGen` for both contours of board 2). The table below says which blocks simulate each board, and what is left out.

Each block's equations are derived from the physical models of its components. The block is then solved implicitly, either with the trapezoidal rule and Newton-Raphson or by exact integration of RC segments. Blocks exchange voltages with each other, as the circuits do on the real boards.

Each voice draws its own component values from the "Unit" seed, then is **calibrated as the service manual prescribes** (`Voice::build`), each trimmer by its own procedure:

| Trimmer | Procedure (service manual section 5; R. J. Folkman's field service manual) |
|---|---|
| Oscillator RANGE (R11) and SCALE (R8), each oscillator | RANGE at 2': 3520 Hz on high A, 440 Hz on low A, repeated. The old board has no high-frequency trimmer: its compensation is the fixed R42 |
| OCTAVE (R168) | Folkman's procedure: oscillator 1 at 2', the second A tuned to the A-440 with TUNE, then at 8' the same key zero-beaten by the trimmer (the later service manual, 5.34, uses 16'). The other steps and oscillators keep the mismatch of their matched resistors |
| REGEN CAL (R73) | the filter starts to sing at EMPHASIS 7.5 with CUTOFF at -1 |
| Filter SCALE (R49) | KEYBOARD CONTROL 1 and 2 on: two octaves of keys for two octaves of the singing filter |
| Filter RANGE (R39) | KEYBOARD CONTROL off, CUTOFF at -1, EMPHASIS at 10: zero beats with the A-440 |
| 1ST VCA BAL (R14) | the contour's feedthrough nulled to the manual's -55 dB |
| NOISE LEVEL (R26) | white and pink noise centred on -5 dB at the main output, read at the real level; "so that both white and pink fall within specification": a little further where the two lie close to 6 dB apart |

The filter's trimmers are set on what the unit does (the pitch the filter sings at is measured), not on its formulas. Changing the "Unit" is like playing a different example of the instrument.

### Oscillator board revision: revision 2

The oscillator board (board 1) exists in three revisions, and they do not drift or track in the same way:

| Revision | Serial numbers | Expo converter | Simulated |
|---|---|---|---|
| 1, "discrete" | up to 1236 | discrete 2N4058 PNP pairs, no temperature compensation | no |
| **2, "old board"** | **1237 to 10174** | **RCA CA3046 transistor array at ambient temperature, 1 kΩ +3500 ppm/K tempco resistor (Fig. 9-3, 9-4)** | **yes** |
| 3, "new board" | 10175 and above | Fairchild µA726 heated pair held at about 80 °C, no tempco (Fig. 9-1, 9-2) | no |

Mini Moop models revision 2 as Fig. 9-3 draws it, that is with the 94-001 kit fitted (R69 / R105 / R141 15 kΩ, the RC networks at R78 / R106 / R128, R181 51 kΩ, R170 15 kΩ 1 %). Each oscillator has a 741 CV summer (IC1, IC4, …) working around the −5 V reference, with a 1 kΩ tempco resistor in its feedback (R20, R71, …) and 51.1 kΩ 1 % inputs (R27, …). It drives the CA3046 expo pair (IC2), whose reference transistor current is set by R50 = 39 kΩ with its collector held by a 741 (IC3) on the "−4 V" line, the divider R166 6.2 kΩ / R174 3.9 kΩ from −10 V (−3.86 V): about 99 µA. The pair sits at the temperature of the board, and the drift that results is what `DriftModel` reproduces. The front panel is Fig. 9-16's, with the octave buffer board.

### The six boards and the blocks that simulate them

| Board | Moog reference | Circuit (service manual) | Simulated by | Not simulated |
|---|---|---|---|---|
| **1** Oscillator (rev 2) | 93-113, Fig. 9-3 / 9-4 | 3 × CV summer, CA3046 expo converter with its fixed high-frequency compensation, sawtooth core (0.01 µF, FET reset, 0 to −4 V ramp), waveshapers, −5 V reference and OCTAVE trim | `ExpoConverter` (summer reduced to its gain and tempco), `VcoCore`, the RANGE string in `Voice` | the summers' op-amp dynamics; the triangle shaper Q2 as a circuit (a rounded fold fitted to ngspice's transfer) |
| **2** Contour generator and keyboard | 93-114, Fig. 9-6 / 9-7 | circuit 8: two contour generators (C5 / C2 = 10 µF) | 2 × `ContourGen` (filter, loudness) | the trigger section's delay |
| | | circuit 1: keyboard circuit: 43 × 10 Ω key string, track and hold with glide (C6 = 1 µF) | the key string's tolerances in `Voice`, `Glide` (the hold when the key is released, the glide's two rails, the hold's sag) | the current source (its value goes into the oscillators' SCALE), C9 on the pitch input. Lowest-note priority and single triggering are handled by `VoiceAllocator` |
| **3** Power supply | 93-115, Fig. 9-8 / 9-9 | circuit 3: noise generator (white, pink, red) | `NoiseSource` | |
| | | circuit 15: modulation mix | `ModulationMix` (R3 25 kΩ linear pot with grounded wiper between the two 24 kΩ inputs, 43 kΩ summing resistors) | the amplifier circuit itself |
| | | circuit 10: ±10 V regulators; circuit 11: headphone amplifier | | rails are ideal, so supply-borne oscillator bleed is absent; no headphone stage |
| **4** Filter and VCA | 93-116, Fig. 9-10 / 9-11 | circuit 6: ladder filter (C3, C7, C11, C16 = 0.068 µF), output stage Q5 to Q8, emphasis path with REGEN CAL, cutoff converter Q26 / Q28 | `LadderFilter`, `FilterOutputStage`, `CutoffConverter` | C10 and C27's low-frequency corners |
| | | circuit 7: first VCA (loudness contour), second VCA (EXT. LOUDNESS), output stage Q12 / Q17 | `Vca` | the EXT. LOUDNESS input (its jack is left unplugged) |
| | | circuit 12: external input pre-amp and OVERLOAD lamp driver | `ExtPreamp` (the panel lamp shows the brightest lamp among the sounding units) | |
| | | circuit 13: A-440 reference oscillator | its level, pitch and place in the signal path (after both VCAs) in `SynthEngine` | the Wien bridge as a circuit: a sine with a few estimated harmonics |
| **5** Rectifier | 93-118, Fig. 9-13 | bridge rectifier, filter capacitors, fuses | | not simulated (no audible effect) |
| **6** Octave buffer | 93-119, Fig. 9-14 | three 741 followers buffering the range switches | ideal followers: the string's taps are not loaded | their offsets (the RANGE trims absorb them) |
| Front panel | Fig. 9-16, Tables 7-2 / 7-3 | passive mixer (pots, on/off switches, summing resistors), RANGE string, TUNE, FREQUENCY, the filter's control resistors | `Mixer`, `FrontPanel`, pot tapers in each block | |
| Left-hand controller | 93-060, Fig. 9-12 | pitch and modulation wheels, GLIDE and DECAY switches | the MOD wheel as the drawn rheostat; the pitch wheel's range is the Bend setting | the pitch wheel's network |

`DriftModel` (in `src/engine`) is not a board: it supplies the temperatures of boards 1 and 4 to the expo pairs, the cutoff converter and the ladder.

## Where the values come from

Every parameter of the model has one of these sources:

| Tag | Source |
|---|---|
| **Moog** | Moog's period documentation: the Model D service manual (figures, parts lists in Tables 7-x, circuit descriptions in section 2, adjustment procedures in section 5, specifications), R. J. Folkman's *Model D Technical Service Manual* (7/73) and the owner's manual (`doc/`). "Moog, ngspice" marks a figure computed by ngspice on the netlist transcribed from the named figure (`tools/spice`, whose README lists what was read on the scans and how surely) |
| **Other** | Component data sheets, standard SPICE models, usual part tolerances |
| **Assumed** | No source found: estimated. These are the values to refine first |

No value comes from another emulation or from a modern recreation of the instrument. All block values live in the `*Params` structs in `src/circuit/blocks/`; the panel's networks in `FrontPanel.h`, the bench calibration in `src/engine/Voice.cpp`.

### Oscillators (board 1, revision 2)

| Parameter | Value | Source |
|---|---|---|
| Summer inputs | keyboard, modulation, external 51.1 kΩ 1 %; BEND 150 kΩ; TUNE 560 kΩ; RNG 15 kΩ 1 %; feedback R20 1 kΩ +3500 ppm/K | Moog (Fig. 9-3) |
| Reference current | the "−4 V" line (R166 / R174 from −10 V: −3.86 V) across R50 39 kΩ (≈99 µA) | Moog (Fig. 9-3) |
| High-frequency compensation | R42 160 kΩ from the tail's R79 1 kΩ to the summing node: 319 V of control voltage per ampere, not adjustable | Moog (Fig. 9-3; 2.16) |
| The pair's emitter and base resistance | 4.7 Ω ± 15 %: what that compensation, less the reset time's share, cancels on a nominal unit | Assumed |
| CA3046 transistors: saturation current | 6.734 fA (generic; the trims absorb it) | Other |
| Timing capacitor, ramp | 0.01 µF polystyrene 2 %, 0 to −4 V | Moog (Fig. 9-3, 2.16) |
| Sawtooth level | +4 to −4 V through R34 4300 / R33 4700: ±2.09 V, 2.25 kΩ source | Moog (Fig. 9-3) |
| Triangle, reverse sawtooth, rectangle levels | triangle ±1.75 V (ngspice on the drawn shaper; the drawing: "+1.7 TO −1.7V"), 0.69 kΩ source; "+1.7 TO −1.7V LOADED"; "(3.5V)" (ngspice on the transcribed Schmitt trigger gives 4.3 V unloaded: not reconciled, the drawn level is kept) | Moog, ngspice (Fig. 9-3; `shapers.cir`) |
| Rectangle widths | 50.1, 31.7 and 19.9 %: bias of 0, −1.5 and −2.5 V from −10 V, 7.5 kΩ, 1 kΩ, 1.5 kΩ, against the ±4 V sawtooth through equal 43 kΩ into the Schmitt trigger Q11 / Q12 ("from a 50 percent to a 15 percent duty cycle", 2.16) | Moog, ngspice (Fig. 9-16, 9-3; `shapers.cir`) |
| Shark-tooth mix | sawtooth through R030 47 kΩ, triangle through R031 10 kΩ, against the outputs' dividers: 17.8 % sawtooth, 8.8 kΩ source | Moog (Fig. 9-16, Table 7-2, Fig. 9-3) |
| Reset dead time | 1.6 µs | Assumed |
| Triangle fold rounding | 2.1 % of the swing: fitted to ngspice's transfer (11 mV rms) | Moog, ngspice (`shapers.cir`) |
| Rectangle slew, comparator jitter | 2 V/µs, 4·10⁻⁵ | Assumed |
| Rectangles' and reverse sawtooth's source resistance | not modelled (0) | Assumed |
| Band-limiting kernel (not a circuit value) | windowed sinc at 0.48 of the rate, Kaiser β 8, 32 taps | a numerical choice, held to its aliasing by `test_vco.cpp` |
| RANGE | one string for the three switches: R29 4.75 kΩ 1 % (LO to 32'), then R25 to R28 1 kΩ matched (0.1 %), trimmed as a whole by OCTAVE: LO is 4.75 octaves below 32' | Moog (Fig. 9-16, Table 7-2, 2.15) |
| TUNE | R1 5 kΩ linear from +10 V to R22 5.1 kΩ to ground, into 560 kΩ: 5.4 semitones over the travel (manual: 4 to 6) | Moog (Fig. 9-16, 9-3; 5.35) |
| FREQUENCY (oscillators 2 and 3) | 5 kΩ linear from +10 V to 3.3 kΩ to ground (R35, R41), into R95 220 kΩ: 16.8 semitones over the travel, linear (manual: 14 to 17) | Moog (Fig. 9-16, 9-3; 5.35) |
| OSC. 3 CONTROL off | takes keyboard, modulation, pitch wheel and TUNE off oscillator 3; R180 15 kΩ joins R181 51 kΩ: FREQUENCY 4.4 times wider, 6 octaves | Moog (Fig. 9-3, 9-16; 2.18; owner's manual) |
| Oscillator 3's pitch with OSC. 3 CONTROL off | 1.7 octaves above its pitch on the lowest key: the middle of the window the manual's check leaves (LO, FREQUENCY at minimum: a click every 2 to 5 s) | Moog (5.36), middle of its window |
| Tuning error left by ear | 0.5 cent rms | Assumed |

### Mixer and external input (front panel, board 4)

| Parameter | Value | Source |
|---|---|---|
| VOLUME pots, summing resistors | 25 kΩ linear (R6, R7, R8, R10); 33 kΩ (R45, R46, R47, R49), 11 kΩ for noise (R48); a channel switched off goes to ground, not to the bus | Moog (Fig. 9-16, Tables 7-2 / 7-3) |
| Summing node into the ladder's input pair | C27 10 µF, R54 470 Ω | Moog (Fig. 9-11) |
| EXTERNAL INPUT volume, pre-amp gain, couplings | R9 1 MΩ audio, R66 100 kΩ, gain 201 (1 + R61/R62), C23, C26, C20 | Moog (Fig. 9-11, 9-16) |
| Pre-amp saturation | +9.8 / −7.7 V | Moog (derived from the rails, R57 and the mixer load) |
| OVERLOAD lamp driver: C14 peak detector, R56 / R48, threshold | ≈2.1 V peak (the manual's 2.12 says "approximately 1.2 volts": not reconciled) | Moog (Fig. 9-11) |
| Lamp filament heating / cooling | 25 / 60 ms | Assumed |

### Filter (board 4, circuit 6)

| Parameter | Value | Source |
|---|---|---|
| Ladder capacitors | 4 × 68 nF ±10 % (C3, C7, C11, C16) | Moog (Fig. 9-11) |
| Ladder transistors | TIS97 matched pairs: about 1 mV of offset; current gain 440 (bin 250 to 700); input noise 2 µV rms | Moog (Table 7-8) / Other / Assumed (noise) |
| CUTOFF | R11 5 kΩ linear between +10 V and −10 V through R55 200 kΩ to the control node, where 100 kΩ is an octave a volt: 10 octaves over the dial, an octave a mark ("approximately 2 divisions" for two octaves, 5.18). 440 Hz at −1 by Filter Range: 27.5 Hz to 28 kHz | Moog (Fig. 9-16, 9-11; 5.13, 5.18; Folkman) |
| KEYBOARD CONTROL | R53 300 kΩ and R54 150 kΩ (5 %): 1/3 and 2/3, together the octave a volt that Filter Scale trims | Moog (Fig. 9-16; 5.14, 5.15) |
| Keyboard voltage zero | the lowest key, F (MIDI 41) | Moog (Fig. 9-7: the key string's grounded end) |
| AMOUNT OF CONTOUR | R17 5 kΩ linear from the filter contour, its wiper through R74 47 kΩ: 2.1 octaves per volt of contour at 10 (the specification's "0 to 4 octaves" is exceeded: the manual's own check, 5.17, drives the filter to its top) | Moog (Fig. 9-16, 9-11; 5.17) |
| FILTER MODULATION | the modulation line through R52 33 kΩ: 100K / 33K as deep as the oscillators' | Moog (Fig. 9-16) |
| Cutoff converter | PNP follower Q26 (TIS93) into Q28 (TIS92), no temperature compensation: the scale falls by 0.33 % a kelvin about the CUTOFF knob's centre. RANGE (R39) and SCALE (R49) set on the singing filter | Moog (Fig. 9-11; 2.8) |
| The converter's limit | Q28 against R60 680 Ω: about 1.75 mA, 38 kHz ("35kHz ± 10kHz", 5.17) | Moog (Fig. 9-11; 5.17) |
| Output stage Q5 to Q8: transfer, gain 68.0, clipping +6.4 / −3.7 V | table; the gain follows the TIS97's current gain (63 to 71 over its bin), drawn per unit | Moog, ngspice (Fig. 9-11); Other (TIS97: hFE 250 to 700) |
| EMPHASIS path | the output's load tap (R7 180 Ω over R3 1 kΩ + R7), C10, R14 50 kΩ reverse audio as a rheostat, REGEN CAL R73 1 kΩ with its wiper on Q30's base, R76 330 Ω: loop gain 0.2 at 0 (passband 56.7 for a stage gain of 68.0) | Moog, ngspice (Fig. 9-11, 9-16) |
| Self-oscillation | from EMPHASIS 7.5, by Regeneration Cal | Moog (5.12; Folkman) |
| Output coupling | C5 / C1 0.22 µF into R31 / R38 47 kΩ | Moog (Fig. 9-11) |

### Contour generators (board 2, circuit 8)

| Parameter | Value | Source |
|---|---|---|
| Timing capacitors, series resistors, ATTACK / DECAY pots | C5 / C2 10 µF, R7 / R42 100 Ω, 1 MΩ audio rheostats: 1.4 ms to 11 s to the peak, decay time constant 1.3 ms to 10 s ("10 milliseconds to 10 seconds", specifications) | Moog (Fig. 9-7, Table 7-3) |
| +9.3 V rail, peak detector, sustain networks | Q26; R24 6.8 kΩ / R33 5.6 kΩ against R27 / R29 16.9 kΩ; R19 / R18 5 kΩ, R57 / R13 4.7 kΩ, 4.7 kΩ, R1 3 kΩ, R17 150 kΩ | Moog (Fig. 9-7) |
| Peak detector's diodes and threshold | CR6 / CR3 1N34A (germanium) into the flip-flop: peaks of 5.22 V (loudness) and 4.43 V (filter); "approximately 5 volts" (2.10). R. A. Moog drawing 1437 marks "+4.5 V" and "+4.0 V": with the drawn dividers no single junction voltage gives both, and they are read as rounded marks | Moog, ngspice (Fig. 9-7; 2.10; `contour.cir`) |
| Junction drops: output followers, sustain follower, rest | 1.127 / 0.505 V (the mean over the output's range, ±0.02 V); the sustain follower's Vbe and base load fitted to six held levels (0.746 V and 0.988; filter 0.739 V and 0.990, within 3 mV); 0.645 V | Moog, ngspice (Fig. 9-7; `contour.cir`); the device cards: Other |
| Release dump with DECAY off | R1401 1.5 kΩ | Moog (Fig. 9-12; 2.10) |
| Decay path at minimum setting | R11 100 Ω | Moog (Fig. 9-7) |
| Lowest-note priority, single trigger | | Moog (2.19) |

### VCA and output (board 4, circuit 7, front panel)

| Parameter | Value | Source |
|---|---|---|
| Input | R2 82 kΩ, C6 0.33 µF, R34 820 Ω | Moog (Fig. 9-11) |
| First VCA: Q16 / Q15, loads, V→I converter Q18 | R19 / R23 100 Ω, R14 25 Ω; R59 68 kΩ, R37 6.8 kΩ, R43 270 kΩ to −10 V, R35 1 kΩ | Moog (Fig. 9-11; 2.9) |
| Second VCA: Q14 / Q13, loads, tail | R18 / R21 330 Ω, R12 100 Ω; Q21 held on by the EXT. LOUDNESS jack (+10 V through R56 and R51 33 kΩ): 1.37 mA | Moog, ngspice (Fig. 9-11, 9-16; 2.9) |
| Output stage: Q12 / Q17, current source Q1 | R8 / R28 8.2 Ω, 9.7 mA, R29 680 Ω: swings ±2.8 V | Moog, ngspice (Fig. 9-11) |
| Gain from the filter's output | 1.97 at a 5 V contour; Q18 saturates from about 4.3 V | Moog, ngspice (Fig. 9-11) |
| Q18 and the pairs (TIS97) | hFE 440 (bin 250 to 700), saturation current 10 fA (generic) | Other |
| Balance left after 1ST VCA BAL; closed VCA | −55 dB or better (5.25); 80 dB (specifications) | Moog (5.25, specifications) |
| Main VOLUME | C2 10 µF, R75 1 kΩ into R20, 5 kΩ linear | Moog (Fig. 9-11, 9-16) |
| A-440 | into the second VCA's load through R40 and C8: after both VCAs; −8 dB at the main output, 440 ± 1 Hz | Moog (Fig. 9-11; 5.7) |
| A-440's harmonics | 3 %, 1.5 %, 0.7 % (2nd, 3rd, 4th): "a somewhat imperfect sinewave" (Folkman) | Assumed |
| The manual's 0 dB | 0.775 V: with it the circuit gives the manual's A-440 and triangle levels | Assumed |
| Load on the output | 10 kΩ | Assumed |
| Digital full scale | ±2.85 V | Assumed (a level choice, not part of the circuit) |

### Noise (board 3, circuit 3) and modulation (circuit 15, left-hand controller)

| Parameter | Value | Source |
|---|---|---|
| Pink network | R16 10 kΩ; C3 0.12 µF with R8 3.3 kΩ; C2 0.033 µF with R13 220 Ω; C1 2.5 µF into Q3 | Moog (Fig. 9-8) |
| Q3's input resistance and gain | 26.9 kΩ, 16.9: Q3 at 2.3 mA between +10 V and the board's −10 V rail (drawing 1431: its collector at +3.8 V) | Moog, ngspice (Fig. 9-8; drawing 1431) |
| Red noise | R12 10 kΩ / C7 0.15 µF into the common emitter stage Q6 (R24 10 kΩ, R15 3.9 kΩ, gain 1.81), C22 0.15 µF across its load: two poles at 106 and 149 Hz | Moog, ngspice (Fig. 9-8; drawing 1431) |
| Output couplings | white C18 0.15 µF, pink C20 2.5 µF, red C28 10 µF, into about 25 kΩ | Moog (Fig. 9-8) |
| Noise level | NOISE LEVEL trimmed for white and pink centred on −5 dB at the main output: about 0.18 V rms (−12 dBm) of white noise at the board. Drawing 1431 (1970) marks −4 dBm, which would put the main output near +3 dB, outside 5.27: only its relative levels are used | Moog (2.5, 5.27; Folkman) |
| White noise bandwidth | 37 kHz: white, pink and red come out closest to one level (pink +1.1 dB, red −0.7 dB against white), as drawing 1431 marks them ("−4 dBm" each) | Moog, ngspice (drawing 1431; `tools/spice/noise_levels.py`) |
| Junction noise: 1/f corner, popcorn | 20 Hz, rare 0.03 steps | Assumed |
| MODULATION MIX | R3 25 kΩ linear with its wiper grounded, 24 kΩ inputs, 43 kΩ into the amplifier's virtual ground, R60 91 kΩ feedback | Moog (Fig. 9-8, 9-16) |
| Modulation line | the amplifier through R57 1 kΩ, against the MOD. wheel and the loads the switches connect (the oscillators' 51.1 kΩ MOD inputs, oscillator 3's only with OSC. 3 CONTROL on; R52 33 kΩ) | Moog (Fig. 9-8, 9-12, 9-3) |
| MOD. wheel | R1402 50 kΩ audio used as a rheostat from the line to ground, "90° rotation used from counterclockwise stop", "1.2K when fully forward" | Moog (Fig. 9-12) |
| MOD. wheel's shape over its travel | the audio taper's first 30 %, scaled to the drawn 1.2 kΩ (the taper alone would give 1.7 kΩ) | Assumed |
| Modulation polarity | inverted: the amplifier inverts and the line enters the oscillators' summer as the keyboard does | Moog (Fig. 9-3, 9-8) |

### Keyboard and glide (board 2, circuit 1)

| Parameter | Value | Source |
|---|---|---|
| Key string | 43 resistors of 10 Ω 1 % under a constant current; low A and high A, on which the oscillators are scaled, are exact | Moog (Fig. 9-7; 2.19) |
| Glide capacitor, GLIDE pot | C6 1 µF, R2 5 MΩ "No. 1 taper", R61 330 Ω | Moog (Fig. 9-7, Table 7-3) |
| Glide law | C6 charged through the pot from the saturated amplifier: towards +9.87 V going up, towards −4.44 V (R59 3.9 kΩ / R54 1.5 kΩ) going down, seen through the follower Q10 (out = 0.927 C6 + 0.88 V); 0.59 and 1.06 s an octave at 10 ("1 millisecond to 1 second/octave") | Moog, ngspice (Fig. 9-7; 2.19; specifications; `keyboard.cir`) |
| Hold | the glide stops and the voltage is held when the last key is released | Moog (2.19) |
| The amplifier's gain, Q13's channel, the hold's leakage | 2000, 250 Ω, the net of the two FETs' gate leakages, 30 pA either way (a few cents a minute) | Assumed / Other (2N4303) |

### Components, tolerances and drift

| Parameter | Value | Source |
|---|---|---|
| Supply rails | ±10 V (ideal) | Moog |
| Potentiometer tapers | linear as named; "audio" and "reverse audio": logarithmic law with 10 % of the track at half rotation | Moog (the names, Table 7-3); Assumed (the law) |
| BJT and diode models | Ebers-Moll / Gummel-Poon as in SPICE; `tools/spice/models.lib`: only the type and the gain bin are from data sheets | Other |
| Tolerances | rated values where the drawing or the parts lists give them (1 %, 10 %, matched sets, R20's 3 %, the contours' electrolytics "+50/−10 %", Tables 7-5 to 7-8); otherwise 5 % resistors, 20 % pots and electrolytics | Moog / Other |
| Board temperatures and drift (`DriftModel`) | time constants, amplitudes; within the specification's "short term oscillator stability: better than 0.25 %" at Drift 1 | Assumed |
| Pitch wheel | its range is the Bend setting (default 7 semitones each way). The manual asks for 13 to 17 semitones over the travel (5.35); the drawn network (R1403 25 kΩ, "90° rotation used", R12 150 kΩ) comes to 11 to 12 | Moog (5.35) / Assumed |

## Added controls (not on the original)

- **Voices**: mono (faithful) or poly with 2 to 8 voices.
- **Quality**: 2x, 4x or 8x oversampling.
- **Drift**: from 0 (stable) to 3 (hot stage).
- **Unit**: tolerance seed, i.e. which example of the instrument you are playing.
- **Bend**: pitch bend range (7 semitones each way by default, about the original's).
- **Mod. Amt**: modulation depth stored with the sound, as if the MOD. wheel were left in position. The wheel can only add to it (the deeper of the two is used). Presets converted from synths that store the depth (Arturia Mini V4) need it.
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

## References

Period documents the simulation is built from. Local copies are kept in `doc/` (not tracked by git).

- Moog Music, *Minimoog Service Manual* (Model 204D): circuit descriptions (section 2), calibration procedure (section 5), parts lists (section 7), schematics and board figures (section 9) and specifications. `tools/spice/README.md` lists what was read on the figures.
  - Complete scan, with the calibration procedure: [archive.org/details/moog_Moog_Minimoog_Service_Manual](https://archive.org/details/moog_Moog_Minimoog_Service_Manual)
  - Scan of sections 8 and 9 (`doc/Minimoog_Service_Manual.pdf`): [archive.org/details/JL11387](https://archive.org/details/JL11387)
- R. J. Folkman, *The minimoog Model D Technical Service Manual* (`doc/Minimoog_Model_D_Technical_Service_Manual_Folkman.pdf`): [synfo.nl](https://www.synfo.nl/servicemanuals/Moog/MINIMOOG-D_SERVICE_MANUAL.pdf), mirrored at [archive.org/details/moog_MINIMOOG-D_SERVICE_MANUAL](https://archive.org/details/moog_MINIMOOG-D_SERVICE_MANUAL)
- Moog Music, *A Guide to the Operation of the Mini Moog Model D*, the original owner's manual (`doc/minimoog_operation_manual_1.pdf`): [archive.org/details/synthmanual-moog-minimoog-owners-manual](https://archive.org/details/synthmanual-moog-minimoog-owners-manual)

Also consulted, not a period document: the reissue's *Minimoog Model D Manual* (`doc/Minimoog_Model_D_Manual.pdf`), for the switches only found on the reissues: [Moog / inMusic](https://cdn.inmusicbrands.com/Moog/Model%20D/Minimoog_Model_D_Manual.pdf)

## License

Mini Moop is licensed under the **GNU Affero General Public License v3.0 or later** (`AGPL-3.0-or-later`): you may use it under the terms of version 3 or, at your option, any later version published by the Free Software Foundation. The full text of version 3 is in [`LICENSE`](LICENSE).

It is built on the JUCE framework, which is dual-licensed under the AGPLv3 and the commercial [JUCE 9 licence](https://juce.com/legal/juce-9-licence/). This project uses JUCE under the AGPLv3.

Third-party components keep their own licences:
- JUCE 9.0.5: AGPLv3 / commercial JUCE licence
- Catch2 3.16.1 (tests only): Boost Software License 1.0
