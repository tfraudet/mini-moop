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
                         741/301 op-amp (GBW, slew, ±10 V rails), potentiometers (tapers), tolerances,
                         band-limited steps and ramps (BLEP / BLAMP)
src/circuit/solver       MnaCircuit<N>: modified nodal analysis + Newton-Raphson + pnjlim limiting
src/circuit/blocks       One block per circuit of the schematic (see "Which revision and which boards are simulated")
src/engine               Voice (one complete "unit"), mono/poly allocation, thermal drift,
                         decimation (2x/4x/8x oversampling), SynthEngine
src/                     JUCE plugin: parameters, processor, sound banks (SoundBank)
src/ui                   Vector panel (no bitmaps), resizable, sound bank manager
presets                  Built-in Factory sound bank (JSON)
tests/                   Catch2 tests, including direct comparisons with ngspice
```

The engine (`minimoop_dsp`) is plain C++20 with no JUCE dependency. After `prepare()` it allocates no memory; a test checks this.

### Validation

Each claim below is checked by a test in `tests/` (Catch2), run by `ctest` or `build/release/minimoop_tests`. A tag runs a group, for example `minimoop_tests "[vco]"`. The tests tagged `[spice]` run ngspice at test time and are skipped when it is not installed; the others that compare with ngspice hold its results as reference values (the benches that produced them are in `spikes/ngspice-*`).

| What is checked | Result | Test (`tests/…`: test case) |
|---|---|---|
| Diode and BJT models and the MNA solver against ngspice on identical netlists | RMS error below 0.5 % | `test_spice_validation.cpp`: "RC low-pass matches ngspice", "Diode clipper matches ngspice", "BJT differential pair with collector capacitor matches ngspice" |
| Solver and helpers | LU with pivoting, RC step against the analytic solution, deterministic tolerance draws, audio taper | `test_solver.cpp` |
| Reduced ladder against a transistor-level ladder in ngspice (10 NPN, 40 mV input, k = 2.5) | RMS error about 0.01 % | `test_ladder.cpp`: "Ladder reduced model matches transistor-level ngspice ladder" |
| Ladder small-signal response; self-oscillation | −12 dB at the cutoff; oscillates at the cutoff | `test_ladder.cpp`: "Ladder: -12 dB at the small-signal cutoff without emphasis", "Ladder: self-oscillates near cutoff at full emphasis"; `test_voice.cpp`: "Filter self-oscillates with emphasis at 10 and no input" |
| Filter output stage (Q5–Q8) against ngspice; passband gain against the whole of circuit 6 | table within 0.1 mV; 53.5 against 56.7 (0.5 dB); clipping at +6.4 / −3.7 V | `test_filter_output.cpp` (four test cases) |
| Contour levels against ngspice (circuit 8): rest, peak, sustain at six settings, both sections | within 0.07 V | `test_contour.cpp`: "Contour levels match the circuit (ngspice, Fig. 9-7)" |
| VCA gain against the loudness contour, against ngspice (circuit 7) | within 0.25 dB | `test_vca.cpp`: "VCA gain follows the loudness contour as the circuit does (ngspice)", "VCA small-signal gain is unity at a 5 V contour" |
| Oscillator tuning after calibration, 4 randomly drawn units | within 1 cent at the calibration points, 3 cents over 6 octaves | `test_vco.cpp`: "Calibrated oscillators track 1 V/oct to within a few cents over 6 octaves", "Simulated core frequency matches the analytic period" |
| Oscillator aliasing in the audio band, against the fundamental | sawtooth and rectangles below −100 dB at 3.5 and 7 kHz (2x and 4x); triangle below −75 dB (the polyBLEP it replaced: −52 and −44 dB at 2x) | `test_vco.cpp`: "Band-limited steps and corners keep aliasing low at high pitch" |
| Waveform levels | within the waveshaper's swing, no DC runaway | `test_vco.cpp`: "Waveforms stay within the waveshaper swing and have no DC runaway" |
| Shark tooth's mix and the outputs' loading of the VOLUME pot | 17.8 % sawtooth; −4.2 / −1.3 / −0.4 dB at VOLUME 10 (shark tooth, sawtooth, triangle) | `test_vco.cpp`: "Shark tooth: the front panel's R030 / R031 against the board's outputs" |
| KEYBOARD CONTROL (KBD 1, KBD 2, both), the filter singing alone from C3 to C4 | 1/3, 2/3 and 1 octave an octave, within 0.03 octave (both: 0.995) | `test_reference_laws.cpp`: "KEYBOARD CONTROL: the filter follows the keys at 1/3, 2/3 and 1 octave an octave" |
| Modulation line: depth at full MOD. wheel, the wheel's law, FILTER against OSCILLATOR MODULATION (oscillator 3 on LO) | oscillators 1.42 octaves peak to peak at full (ngspice: 1.46); half way 53 % of full; filter 2.96 times as deep (100K / 33K: 3.03) | `test_reference_laws.cpp`: "Modulation line: depth and MOD. wheel law from the circuit" |
| Modulation polarity (oscillator 3's sawtooth on LO) | the pitch falls along the ramp and jumps up 1.3 octaves at each reset | `test_reference_laws.cpp`: "Modulation polarity: oscillator 3's rising sawtooth lowers the pitch" |
| Laws measured on the CA-72 hardware reference: LO, FREQUENCY, GLIDE | LO 6.74 octaves; FREQUENCY within 0.05 semitone; GLIDE rates within 30 % | `test_reference_laws.cpp` (three test cases) |
| CUTOFF law from the service manual (Filter Range, Table 5-4; Filter Cutoff Adjustment, 5.13; specifications) | 440 Hz at −1 within 0.25 semitone, −3 … +3 within 1 semitone of 0.9 octave a mark (averaged over units); every unit above 16 kHz fully clockwise and under 300 Hz fully anticlockwise | `test_reference_laws.cpp`: "CUTOFF: the service manual's law, 440 Hz at -1 and 0.9 octave a mark" |
| ATTACK / DECAY pot laws; EMPHASIS passband loss | the reference's laws; self-oscillation from about 7 | `test_contour.cpp`: "ATTACK and DECAY follow the hardware reference's pot laws", "DECAY TIME spans the specification"; `test_ladder.cpp`: "EMPHASIS: the hardware reference's passband loss, self-oscillation from about 7" |
| Pink noise against white, against ngspice (circuit 3); noise colours and levels | within 0.8 dB from 100 Hz to 10 kHz | `test_reference_laws.cpp`: "Pink noise against white follows board 3's network (ngspice)"; `test_noise_a440.cpp` |
| External input pre-amp and OVERLOAD lamp | gain 201, rails +9.8 / −7.7 V, lamp from about 2.1 V | `test_extpreamp.cpp` |
| MODULATION MIX, main VOLUME, A-440 | as wired on the original | `test_noise_a440.cpp` |
| Voice behaviour: level, extreme settings stay finite, poly, MOD. AMOUNT | | `test_voice.cpp` |
| No memory allocated during audio rendering, note events and panel changes | | `test_voice.cpp`: "Rendering, note events and panel changes do not allocate" |
| CPU load, 1 and 8 voices at 2x, 4x and 8x | reported (table below); one voice at 4x must stay under 10 % of a core | `test_voice.cpp`: "CPU load: 1 and 8 voices at 2x, 4x and 8x oversampling" |
| Sound banks: parsing, round trip, invalid banks, library, import / export | | `test_soundbank.cpp` (`minimoop_bank_tests`) |

### CPU load (Apple M3 Pro, 48 kHz)

Share of one core, real time being 100 %: three oscillators, EMPHASIS 6, DRIFT 1; 8 voices are an 8-note chord in POLY. Measured by `test_voice.cpp`: "CPU load: 1 and 8 voices at 2x, 4x and 8x oversampling" (run `minimoop_tests "[perf]"`; the figures vary by about 10 % from run to run).

| Quality | 1 voice | 8 voices |
|---|---|---|
| Eco 2x | ~2.8 % | ~23 % |
| High 4x | ~5 % | ~42 % |
| Ultra 8x | ~10 % | ~78 % |

### Outside the test suite

- pluginval at strictness level 8: the VST3 passes consistently. Apple's `auval` passes for the AU.
- pluginval on the AU fails intermittently (about 1 run in 2) on the state-restoration tests, each time on a different random parameter. This is a timing race in the validator's AU host: JUCE's AU hosting code caches parameter values and updates that cache from queued main-thread notifications, which can arrive after a state restore and overwrite it with old values. The same seed passes on some runs and fails on others, and the failure is identical with JUCE 8.0.17 and 9.0.5.

## Which revision and which boards are simulated

Figure, table and circuit numbers below refer to the Minimoog Model D service manual.

A **board** is a physical part of the instrument: one of its six printed circuit boards, numbered 1 to 6 in the service manual (plus the front panel and the left-hand controller). A board carries several *circuits* (the service manual numbers them too: board 4 holds circuit 6, the filter, and circuit 7, the VCA, among others). A **block** is the code that simulates one of these circuits: a C++ class in `src/circuit/blocks/`, such as `LadderFilter` for circuit 6 or `Vca` for circuit 7. A board is therefore simulated by several blocks, and one block can serve more than once (`ExpoConverter` on boards 1 and 4, `ContourGen` for both contours of board 2). The table below says which blocks simulate each board, and what is left out.

Each block's equations are derived from the physical models of its components. The block is then solved implicitly, either with the trapezoidal rule and Newton-Raphson or by exact integration of RC segments. Blocks exchange voltages with each other, as the circuits do on the real boards.

Each voice draws its own component values from the "Unit" seed, then is **calibrated the way a technician would**: offset at A4, V/oct between A1 and A4, HF trim at A7, filter scale. Changing the "Unit" is like playing a different example of the instrument.

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
| | | circuit 15: modulation mix | `ModulationMix` (R3 25 kΩ linear pot with grounded wiper between the two 24 kΩ inputs, 43 kΩ summing resistors) | the amplifier circuit itself |
| | | circuit 10: ±10 V regulators; circuit 11: headphone amplifier | | rails are ideal, so supply-borne oscillator bleed is absent; no headphone stage |
| **4** Filter and VCA | 93-116, Fig. 9-10 / 9-11 | circuit 6: ladder filter (C3, C7, C11, C16 = 0.068 µF), cutoff expo pair Q26 / Q28, Emphasis | `LadderFilter`, `ExpoConverter` (cutoff) | |
| | | circuit 7: voltage controlled amplifier | `Vca` | |
| | | circuit 12: external input pre-amp and OVERLOAD lamp driver | `ExtPreamp` (the panel lamp shows the brightest lamp among the sounding units) | |
| | | circuit 13: A-440 reference oscillator | | not simulated (A-440 is an ideal sine) |
| **5** Rectifier | 93-118, Fig. 9-13 | bridge rectifier, filter capacitors, fuses | | not simulated (no audible effect) |
| **6** Octave buffer | 93-119, Fig. 9-14 | three 741 followers buffering the range switches | | not simulated: range switching is ideal |
| Front panel | Fig. 9-16, Tables 7-2 / 7-3 | passive mixer (pots, on/off switches, summing resistors), controls | `Mixer`, pot tapers in each block | |
| Left-hand controller | 93-060, Fig. 9-12 | pitch and modulation wheels | ideal pitch bend and modulation amount in `Voice` | |

`DriftModel` (in `src/engine`) is not a board: it supplies the temperatures of boards 1 and 4 to the expo pairs and the ladder.

## Where the values come from

Every parameter of the model has one of these sources:

| Tag | Source |
|---|---|
| **Moog** | Official Moog documentation: Model D service manual (figures, parts lists in Tables 7-x, calibration procedure, specifications) and operation manuals (`doc/`); R. J. Folkman's *Model D Technical Service Manual* (`doc/`) |
| **CA-72 sim** | Derived by ngspice simulation of the CA-72 project's netlists, which transcribe the service manual's figures ([idlefoundry/ca-72](https://github.com/idlefoundry/ca-72), GPL-3.0; benches in `spikes/ngspice-*`) |
| **CA-72 ref** | Measured by the CA-72 project on its hardware reference, a modern recreation of the Model D its owner could not tell from an original in matched recordings ([docs/calibration](https://github.com/idlefoundry/ca-72/tree/main/docs/calibration)). Used only where this model had an assumption |
| **Other** | Component data sheets, standard SPICE models, usual part tolerances |
| **Assumed** | No source found: estimated, or calibrated by ear against earlier versions. These are the values to refine first |

All block values live in the `*Params` structs in `src/circuit/blocks/`; the panel laws and conventions in `src/engine/Voice.cpp`.

### Oscillators (board 1, revision 2)

| Parameter | Value | Source |
|---|---|---|
| Expo converter: tempco resistor R20, CV input R27, reference current | 1 kΩ +3500 ppm/K, 51.1 kΩ 1 %, −4 V across R50 39 kΩ (≈103 µA) | Moog (Fig. 9-3, 9-4) |
| CA3046 transistors: saturation current, emitter resistance | 6.734 fA, 0.8 Ω | Other (generic small-signal NPN SPICE model) |
| Timing capacitor, ramp | 0.01 µF polystyrene, 0 to −4 V | Moog (Fig. 9-3) |
| Waveform outputs | ±1.7 V, rectangle 3.5 Vpp | Moog (Fig. 9-3) |
| Rectangle widths | ≈50 / 30 / 15 % | Moog (specifications) |
| Reverse ramp on oscillator 3 | | Moog (Fig. 9-3) |
| Reset dead time | 1.6 µs | Assumed |
| Triangle fold rounding, rectangle slew, comparator jitter | 3 % of the swing, 2 V/µs, 4·10⁻⁵ | Assumed |
| Shark-tooth mix | sawtooth through R030 47 kΩ, triangle through R031 10 kΩ, against the outputs' dividers: 17.8 % sawtooth | Moog (drawing 1448, Table 7-2, Fig. 9-3) |
| Output source resistances loading the VOLUME pot | sawtooth R34 / R33 (2.25 kΩ), triangle R28 / R23 behind Q1 (0.69 kΩ), shark tooth 8.8 kΩ (at VOLUME 10, against the pot and the 33 kΩ summing resistor: −4.2 dB, sawtooth −1.3 dB, triangle −0.4 dB) | Moog (Fig. 9-3, transcribed by CA-72) |
| Rectangles' and oscillator 3's reverse ramp's source resistance | not modelled (0) | Assumed |
| Band-limiting kernel (not a circuit value) | windowed sinc at 0.48 of the rate, Kaiser β 8, 32 taps | Other (the CA-72 project's choice from its kernel sweep, decision R-BLEP) |
| Calibration: offset at A4, V/oct A1–A4, HF trim at A7 | | Moog (calibration procedure) |
| Technician's tuning error | 0.5 cent rms | Assumed |
| RANGE 32' to 2' | octave steps | Moog |
| RANGE LO | 6.74 octaves below 8' | CA-72 ref |
| FREQUENCY knobs (oscillators 2 and 3) | −6.48 / +6.61 and −6.73 / +6.92 semitones at the −5 / +5 marks, −8.6 / +8.2 at the stops | CA-72 ref |
| TUNE span | ±1.5 semitones | Assumed |

### Mixer and external input (front panel, board 4)

| Parameter | Value | Source |
|---|---|---|
| VOLUME pots, summing resistors | 25 kΩ linear (R6, R7, R8, R10); 33 kΩ (R45, R46, R47, R49), 11 kΩ for noise (R48) | Moog (Fig. 9-16, Tables 7-2 / 7-3) |
| Summing node into the ladder's input pair | C27 10 µF, R54 470 Ω | Moog (Fig. 9-11) |
| EXTERNAL INPUT volume, pre-amp gain, couplings | R9 1 MΩ audio, R66 100 kΩ, gain 201 (1 + R61/R62), C23, C26, C20 | Moog (Fig. 9-11, 9-16) |
| Pre-amp saturation | +9.8 / −7.7 V | Moog (derived from the rails, R57 and the mixer load) |
| OVERLOAD lamp driver: C14 peak detector, R56 / R48, threshold | ≈2.1 V peak | Moog (Fig. 9-11) |
| Lamp filament heating / cooling | 25 / 60 ms | Assumed |
| Noise level reaching the mixer | white noise 0.8 V rms, scaled into the 11 kΩ channel | Assumed |

### Filter (board 4, circuit 6)

| Parameter | Value | Source |
|---|---|---|
| Ladder capacitors | 4 × 68 nF ±10 % (C3, C7, C11, C16) | Moog (Fig. 9-11) |
| Ladder transistors: current gain, offsets, input noise | β 200, 4 % Is spread, 2 µV rms | Other (generic small-signal NPN) / Assumed (noise) |
| CUTOFF law and the filter's position | self-oscillation at 440 Hz at the −1 mark with KEYBOARD CONTROL off (Filter Range trim, Table 5-4), 0.9 octave a mark: 9 octaves over the dial (specifications: "40Hz to 20kHz (9 octaves)"), 36 Hz … 18.5 kHz from −5 to +5. The model sings at 41 to 45 Hz at −5 (its self-oscillation rises above the law below about −3, by 3 semitones at −5) | Moog (service manual, section 5 and specifications; [archive.org copy](https://archive.org/details/moog_Moog_Minimoog_Service_Manual)) |
| Cutoff expo converter (reference current, input resistor, scale) | 20 µA, 100 kΩ, 0.85 | Assumed (they now shape only the curvature) |
| KEYBOARD CONTROL | 1/3 and 2/3 | Moog (specifications) |
| Keyboard CV zero | the lowest key, F2 (MIDI 41) | Moog (Fig. 9-7: the key string's grounded end) |
| AMOUNT OF CONTOUR | 0 to 4 octaves | Moog (specifications) |
| FILTER MODULATION depth | 100K / 33K × the oscillators' (±2.2 octaves at full wheel): the line reaches the filter's control node through R52 33 kΩ, where KEYBOARD CONTROL 1 + 2 (R53 300 kΩ ∥ R54 150 kΩ = 100 kΩ) make 1 octave a volt | Moog (Fig. 9-3, 9-11, 9-16, transcribed by CA-72) |
| EMPHASIS pot | R14 50 kΩ reverse audio rheostat, output node R7 180 Ω | Moog (Fig. 9-11, 9-16) |
| EMPHASIS law up to 8.5 | passband −2.25, −9.53, −10.43, −11.92, −12.50, −14.67 dB at 2.5, 5, 6, 7, 7.5, 8.5 | CA-72 ref |
| Self-oscillation | from about 7 | Moog (specifications: 7–8) |
| EMPHASIS shunt (R76 + part of R73) | ≈800 Ω (residual feedback at 0, law from 8.5 to 10) | Assumed |
| Output coupling | C5 / C1 0.22 µF into R31 / R38 47 kΩ | Moog (Fig. 9-11) |
| Output stage Q5–Q8: transfer, gain 68, clipping +6.4 / −3.7 V | table | CA-72 sim (TIS97 model from CA-72's data-sheet-derived library) |

### Contour generators (board 2, circuit 8)

| Parameter | Value | Source |
|---|---|---|
| Timing capacitors, attack resistors, ATTACK / DECAY pots | C5 / C2 10 µF, R7 / R42 100 Ω, 1 MΩ rheostats | Moog (Fig. 9-7, Table 7-3) |
| +9.3 V rail, peak detector, sustain networks | Q26; R24 6.8 kΩ / R33 5.6 kΩ against R27 / R29 16.9 kΩ; R19 / R18 5 kΩ, R57 / R13 4.7 kΩ, 4.7 kΩ, R1 3 kΩ, R17 150 kΩ | Moog (Fig. 9-7) |
| Release dump with DECAY off | R1401 1.5 kΩ | Moog (Fig. 9-12) |
| Junction drops: output followers, sustain follower, rest, peak detector | 1.16 / 0.52 V, 0.64 V, 0.66 V, 1.23 V | CA-72 sim |
| ATTACK / DECAY pot laws (each of the four pots) | resistance at the dial's marks, ÷ 0.89 (CA-72 models the electrolytic's dielectric absorption, this model does not) | CA-72 ref |
| Attack and decay ranges | 1 ms to 10 s, 4 ms to 35 s | Moog (specifications) |
| Decay path at minimum setting | 100 Ω | Assumed (from the 4 ms specification) |
| Lowest-note priority, single trigger | | Moog |

### VCA and output (board 4, circuit 7, front panel)

| Parameter | Value | Source |
|---|---|---|
| Input attenuator | R2 82 kΩ / R34 820 Ω | Moog (Fig. 9-11) |
| V→I converter Q18 | R59 68 kΩ, R37 6.8 kΩ, R43 270 kΩ to −10 V, R35 1 kΩ | Moog (Fig. 9-11) |
| Q18 (TIS97): saturation current, current gain | 5 fA, 420 | CA-72 sim (CA-72's data-sheet-derived library) |
| The pair's emitter node | 4.10 V | CA-72 sim (operating point) |
| Range | 80 dB | Moog (specifications) |
| Feedthrough ("thump"), output amp gain | 0.004, 0.5 | Assumed |
| Main VOLUME | R75 1 kΩ into R20, 5 kΩ linear | Moog (Fig. 9-16) |
| Load on the output | 10 kΩ | Assumed |
| Digital full scale | ±2.85 V | Assumed (a level choice, not part of the circuit) |

### Noise (board 3, circuit 3) and modulation (circuit 15, left-hand controller)

| Parameter | Value | Source |
|---|---|---|
| Pink network | R16 10 kΩ, C3 0.12 µF with R8 3.3 kΩ, C1 2.5 µF into Q3 | Moog (Fig. 9-8) |
| Pink network's upper branch | C2 0.022 µF, R13 510 Ω (drawn 0.033 µF, 220 Ω) | CA-72 ref |
| Q3's input resistance and gain | 92 kΩ, 14.6 | CA-72 sim (fitted, within 0.2 dB) |
| Red noise | R12 10 kΩ / C7 0.15 µF | Moog (Fig. 9-8) |
| Junction noise: 1/f corner, popcorn, amplifier bandwidth | 20 Hz, rare 0.03 steps, 40 kHz | Assumed |
| MODULATION MIX | R3 25 kΩ linear with its wiper grounded, 24 kΩ inputs, 43 kΩ into the amplifier's virtual ground, R60 91 kΩ feedback | Moog (Fig. 9-8, 9-16) |
| Noise level reaching MODULATION MIX | pink ≈0.68 V rms | Assumed |
| Modulation line | MODULATION MIX into the inverting amplifier's virtual ground (R60 91 kΩ feedback over the 43 kΩ inputs: 0.825 of oscillator 3's triangle fully at osc 3, with its output resistance) through R57 1 kΩ, against the MOD. wheel and the loads (three 51.1 kΩ MOD inputs, R52 33 kΩ) | Moog (Fig. 9-8, 9-12); checked with ngspice (CA-72's netlist: +0.75 / −0.71 V on the line) |
| MOD. wheel | R1402 50 kΩ audio used as a rheostat from the line to ground, 90° of rotation, 1.2 kΩ fully forward | Moog (Fig. 9-12) |
| MOD. wheel's shape over its travel | the generic audio taper's first third scaled to 1.2 kΩ (half way: 0.39 kΩ, about half the depth) | Assumed (as the CA-72 project's) |
| Oscillator modulation at full wheel | ±0.74 octave (51.1 kΩ inputs, 1 octave a volt) | Moog (derived from the line) |
| Modulation polarity | inverted: the amplifier inverts and the line enters the oscillators' summer as the keyboard does, so oscillator 3's rising sawtooth lowers the pitch and the cutoff | Moog (Fig. 9-3, 9-8) |

### Glide (board 2, circuit 1)

| Parameter | Value | Source |
|---|---|---|
| Glide capacitor, GLIDE pot | C6 1 µF, R2 5 MΩ | Moog (Fig. 9-7, Table 7-3) |
| Straight slide from the saturated hold amplifier | | CA-72 sim / ref (their circuit and measurements) |
| GLIDE law, drives | 162 kΩ, 257 kΩ, 2.1 MΩ, 5 MΩ at 2.5, 5, 7.5, 10; 8.0 V up, 4.45 V down | CA-72 ref |
| Series resistance with the switch off | 1 kΩ | Assumed |

### Components, tolerances and drift

| Parameter | Value | Source |
|---|---|---|
| Supply rails | ±10 V (ideal) | Moog |
| 741 / 301 op-amps | GBW, slew rate, output limits | Other (data sheets) |
| BJT and diode models | Ebers-Moll / Gummel-Poon as in SPICE | Other (SPICE) |
| Tolerances | rated values where the drawing gives them (1 %, 10 %); otherwise 5 % resistors, 20 % pots and electrolytics | Moog / Other |
| Board temperatures and drift (`DriftModel`) | time constants, amplitudes | Assumed |

## Added controls (not on the original)

- **Voices**: mono (faithful) or poly with 2 to 8 voices.
- **Quality**: 2x, 4x or 8x oversampling.
- **Drift**: from 0 (stable) to 3 (hot stage).
- **Unit**: tolerance seed, i.e. which example of the instrument you are playing.
- **Bend**: pitch bend range.
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

- Moog Music, *Minimoog Service Manual* (Model 204D): schematics and board figures (section 9), parts lists, calibration procedure (section 5) and specifications.
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
