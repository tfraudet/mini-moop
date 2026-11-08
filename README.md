# Mini Moop

A VST3, AU and Standalone synthesizer for personal use. It simulates, at component level, the circuits of a classic 1970s monophonic "Model D"-type synthesizer, and adds a polyphonic mode.

Built with JUCE 9.0.5. Licensed under the GNU AGPLv3 (see [License](#license)).

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
src/                     JUCE plugin: parameters, processor, factory programs
src/ui                   Vector panel (no bitmaps), resizable
tests/                   Catch2 tests, including direct comparisons with ngspice
```

The engine (`minimoop_dsp`) is plain C++20 with no JUCE dependency. After `prepare()` it allocates no memory; a test checks this.

### "Hybrid component-level" simulation

Each block's equations are derived from the physical models of its components. The block is then solved implicitly, either with the trapezoidal rule and Newton-Raphson or by exact integration of RC segments. Blocks exchange voltages with each other, as on the real circuit boards.

| Block | What is simulated |
|---|---|
| `ExpoConverter` | Transistor pair: Vt = kT/q at each transistor's temperature, Is temperature law (SPICE XTI/EG), +3500 ppm/K tempco resistor, Is mismatch, emitter resistance, V/oct, offset and HF trimmers |
| `VcoCore` | Expo current integrated on a 0.01 µF polystyrene capacitor, comparator and FET reset with dead time (high notes go flat, compensated by the HF trim), shapers for triangle, shark-tooth, reverse ramp and 3 rectangles with output-stage slew. polyBLEP correction at the exact instant of each discontinuity |
| `LadderFilter` | 4 differential-pair stages (Ebers-Moll equations), 68 nF ±10 % capacitors, base current losses (β), pair offsets, zero-delay Emphasis feedback (solved implicitly), transistor noise, output op-amp |
| `Mixer` | Pots loaded by the summing resistors (Millman's theorem). The "pushed mixer" saturation happens in the ladder's input pair (tanh) |
| `ContourGen` | Timing capacitor, 5 MΩ log pots, end-of-attack comparator, Decay switch, leakage |
| `Vca` | Differential pair + V→I converter, control feedthrough ("thump"), 80 dB range |
| `NoiseSource` | Junction noise (Gaussian + 1/f + popcorn), −3 dB/oct RC network for pink noise |
| `Glide` | RC on 1 µF, audio-taper pot |
| `DriftModel` | Board temperature (Ornstein-Uhlenbeck), local hot spots, temperature difference within each expo pair. Tuning drift **results** from the physics; it is not an added LFO |

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

## Where the values come from: sourced or assumed

**Sourced** (Model D service manual, OCR of the parts lists, specifications):
- ±10 V supply rails;
- sawtooth core with FET reset and 0.01 µF polystyrene capacitor;
- 1 kΩ tempco resistor and a reference current of about 5 µA;
- ladder with 4 × 68 nF ±10 %;
- self-oscillation at Emphasis 7–8;
- filter contour over 0 to 4 octaves, KBD 1/3 and 2/3;
- attack from 1 ms to 10 s and decay from 4 ms to 35 s;
- glide on 1 µF;
- 50 kΩ audio-taper mod wheel and modulation injection over 0 to 1¼ octaves;
- linear VCA over 80 dB;
- pink noise at −3 dB/oct;
- lowest-note priority, single trigger;
- rectangle duty cycles of about 50 / 30 / 15 %;
- reverse ramp on oscillator 3.

**Assumed or estimated** (no source found; these are the parameters to refine first if you have the scanned schematics in front of you):
- mixer values (47 k / 4.7 k) and ladder input gain (set for mild saturation with one oscillator at 10);
- Emphasis network and filter output amp values;
- contour timing capacitor (2.2 µF) and minimum resistances;
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

Rocker switches are two-state choice parameters (Off/On) rather than boolean parameters, so they survive a host state save and restore unchanged (required to pass pluginval).

## License

Mini Moop is licensed under the **GNU Affero General Public License v3.0**. The full text is in [`LICENSE`](LICENSE).

It is built on the JUCE framework, which is dual-licensed under the AGPLv3 and the commercial [JUCE 9 licence](https://juce.com/legal/juce-9-licence/). This project uses JUCE under the AGPLv3.

Third-party components keep their own licences:
- JUCE 9.0.5: AGPLv3 / commercial JUCE licence
- Catch2 3.16.1 (tests only): Boost Software License 1.0
