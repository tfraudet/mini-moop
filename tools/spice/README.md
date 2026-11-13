# Netlists and benches transcribed from the Moog service manual

Everything here is read directly from the scanned figures of the *Minimoog Service Manual*
(`doc/Minimoog_Service_Manual.pdf`, not tracked) and from its text (sections 2 and 5, OCR at
archive.org `moog_Moog_Minimoog_Service_Manual`). No value comes from another project.

How the figures were read: `pdftoppm -r 72` gives the scan at its native resolution (the pages are
images at 72 ppi); tiles of 1100 x 800 and 2x to 4x enlargements of the dense areas. Confidence:
**C** clear on the scan, **P** probable (read with some doubt, or deduced), **D** deduced from the
circuit's arithmetic, not read.

Target configuration: the figures as drawn. Fig. 9-3 is the old oscillator board (serial numbers
below 10175) with the 94-001 kit applied (R69/R105/R141 15K, RC networks at R78/R106/R128, R181
51K, R170 15K 1 %). Fig. 9-16 is the later front panel wiring, with the octave buffer board 6.

## Front panel (Fig. 9-16, page 18 of the scan)

Wire colours: RED [BRN] = +10 V, WHT [RED] = ground, BLK [ORN] = -10 V.

| What | Reading | Conf. |
|---|---|---|
| Octave string | R25, R26, R27, R28 1K (matched set), R29 4.75K 1 %, in series on SW3; the six taps go to SW4 and SW5 in parallel; each switch's wiper to its buffer on board 6 | C |
| R59 | in series at the R25 end of the string ("deleted above 10175"); value illegible, 8.2K deduced: 5 V / (4K + 4.75K + R59 + R169 68.1 + R168 ~12) must give 0.2935 mA, one octave a 1K into the 15K RNG inputs | D |
| LO | the tap beyond R29: 4.75 octaves below 32' (6.75 below 8') | C |
| TUNE R1 | 5K linear: pin 3 to +10 V, pin 1 through R22 to ground, wiper to board 1 pin 5A (R21 / R88 / R120 560K) | C |
| R22 | drawn value reads "5.1K" (parts table: 3.3K). 5.1K gives 5.4 semitones of travel, 3.3K gives 6.6; SM 5.35 asks for 4 to 6 | P |
| FREQUENCY R4 (osc 2), R5 (osc 3) | 5K linear: pin 1 to +10 V, pin 3 through R35 / R41 3.3K to ground (the ground of SW6 / SW7 pin 10), wiper to board 1 pin 12A / 9A | P |
| OSC. 3 CONTROL SW2 | pole 1: R5's wiper (pin 2) to board 1 pin 10A (pin 1) when OFF; pole 2: board 1 pin 16A (5) to 15A (6) when ON | C |
| Pulse width bias, SW6 pole B | -10 V, R32 7.5K, R33 1K, R34 1.5K, ground (pin 10): taps at 0, -1.5 and -2.5 V. Osc 2: R38, R39, R40; osc 3: R42, R43, R44 | P |
| Shark tooth, SW6 pole A | R30 47K from the sawtooth (CO1B-9), R31 10K from the triangle (CO1B-21). Osc 2: R36, R37 | C |
| Mixer | VOLUME pots R6, R7, R8, R10 (pin 1 ground), wiper through R45, R47, R49 33K (noise R48 11K) to the switch's common; ON: to the bus, OFF: to ground. Ext: R46 33K | C |
| CUTOFF R11 | 5K linear between +10 V (pin 3) and -10 V (pin 1), wiper through R55 200K to the control node (board 4 pin 5) | C |
| External filter control J2 | through R51 100K to the control node | P |
| KEYBOARD CONTROL | keyboard CV (CO2A-5) through R53 300K (SW16) and R54 150K (SW17) to the control node | C |
| FILTER MODULATION SW15 | modulation line through R52 33K to the control node; OFF: R52's input grounded | C |
| AMOUNT OF CONTOUR R17 | 5K linear from the filter contour (CO2B-11) to ground, wiper to board 4 pin 16 (R74 47K) | C |
| EXT. LOUDNESS J3 | shorting jack: +10 V through R56 33K to board 4 pin 10 | C |
| MODULATION MIX R3 | 25K, R23 / R24 24K from oscillator 3's waveform switch and the noise switch (R50 24K at SW14) | C |

## Board 1, oscillators (Fig. 9-3, page 5)

Oscillator 1 (2 and 3 the same with their own designators).

| What | Reading | Conf. |
|---|---|---|
| Summer IC1 741 | inverting node held at -5 V (pin 3 through R10 1K to the -5 V line); feedback R20 1K T.C. | C |
| Inputs | BEND R12 150K 1 %, TUNE R21 560K, KEYBOARD R27 51.1K 1 %, MOD R32 51.1K 1 %, EXT R38 51.1K 1 %, RNG R43 15K 1 % (R27 / R80 / R129 matched, R43 / R52 / R179 matched) | C |
| Range trim | R11 1K wirewound (rheostat) in series with R37 11.8K 1 % from the node to -10 V | C |
| Scale trim | output through R9 33, R8 100 wirewound, R19 1K to -5 V; R8's wiper to the base of the pair's servoed transistor (IC2 pin 2) | C |
| Expo pair IC2 CA3046 | pins 1/2/3: collector to R50 39K from ground and to IC3 (+), held at -4 V; pins 5/4/3: base to -5 V through R7 100, collector to the timing capacitor. Tail: pins 14/12/13, driven by IC3 through R69 15K, emitter R79 1K to -10 V | C |
| High-frequency compensation | R42 160K from the tail's emitter (pin 13) to the summing node: fixed, no trimmer | C |
| Core | C1 .01, ramp 0 to -4 V; x2 amplifier, +4 to -4 V at Q3's collector | C |
| Sawtooth output | Q3's collector through R34 4300, R33 4700 to ground: +/-2.09 V, 2.25K source | C |
| Triangle output | Q2's collector +3.3 to -0.5 V, follower Q1, R28 750 / R23 8200 to -10 V: "+1.7 TO -1.7V", 0.69K source | C |
| Rectangle | Schmitt Q11 2N4058 / Q12 2N3392, sawtooth through R1 43K and the width bias through R40 43K; output R22 4700 / R13 6200, "(3.5V)" | C |
| Oscillator 3 | inputs R110, R120, R129, R136, R144 and the range trim R109 / R108 on a bus (16A) that SW2 joins to the node (15A); RNG R179 always connected | C |
| Oscillator 3 FREQUENCY | IC8 741 inverter: R181 51K from 9A, R180 15K from 10A, feedback R170 15K 1 %, offset R162 3K from +10 V, output through R143 51.1K 1 % to the node | C |
| Reverse sawtooth | Q37 2N3392 from sawtooth 3 through R171 33K, R175 7500, R176 13K: "(+1.7 TO -1.7V) LOADED" | P |
| -5 V reference | R177 / R173 10K 1 %, IC9 741, Q38 2N3392; octave trim R168 25 wirewound with R169 68.1 1 % from the string's end to ground | C |

## Board 4, filter and VCA (Fig. 9-11, page 13)

| What | Reading | Conf. |
|---|---|---|
| Ladder bias string | +10 V, R4 220, R16 150, R32 150, R41 150, R52 150, node B, R67 200 to ground, C24 220 uF across R67 | C |
| Ladder | Q2/Q3, Q10/Q11, Q19/Q20, Q23/Q24 TIS97; C3, C7, C11, C16 .068; bottom pair Q29/Q30 TIS97 (matched) | C |
| Input | pin 4 through C27 10 uF to Q29's base, R54 470 from that base to node B | C |
| Tail | Q29/Q30 emitters through R60 680 to Q28 TIS92, emitter grounded | C |
| Cutoff converter | Q26 TIS93 follower: base at the control node, emitter to Q28's base and through R46 68K to the wiper of R39 10K RANGE (+10 V to ground), collector through R45 330 to -10 V | C |
| Control node | pin 5 (front panel resistors), R74 47K from pin 16 (AMOUNT OF CONTOUR), R49 500 SCALE in series with R70 1.8K to ground, C18 100 pF | C |
| Output stage | C5 / C1 .22 from the two sides of C3 to Q8 / Q6 (R31 / R38 47K to ground); Q8's emitter R24 220K, Q6's R20 100K to -10 V; pair Q7 / Q5, tail R26 1K to -10 V; Q7's and Q8's collectors through R3 1K and R7 180 to R6 47 from +10 V, C9 100 uF | C |
| EMPHASIS path | junction of R3 and R7 through C10 10 uF to pin 7; R14 (panel) back on pin 6; R73 1K REGEN CAL is a potentiometer between pin 6 and R76 330 (to node B), its wiper on Q30's base | C |
| VCA supply | +10 V, R10 120, R11 150, R13 120, R22 120, node D, R36 510 to ground, C15 220 uF at D | C |
| First VCA | filter output through R2 82K and C6 .33 to Q16's base, R34 820 to D; Q15's base at D; loads R19 / R23 100 and R14 25 (1ST VCA BAL, wiper to the R13 / R22 node); tail Q18 through R35 1K; R59 68K from pin 9, R37 6.8K to ground, R43 270K to -10 V | C |
| Second VCA | Q14 / Q13 on the first pair's collectors; loads R18 / R21 330 and R12 100 (2ND VCA BAL, wiper to the R11 / R13 node); R33 470 to Q21; R51 33K from pin 10, R42 3.3K to ground, R44 150K to -10 V | C |
| Output | Q12 / Q17 2N4058 on the second pair's collectors, R8 / R28 8.2 to Q1 2N4058 (R9 62 from +10 V, base on the R10 / R11 node, C4 220 uF); R30 / R29 680 to ground; CR1 / CR2 1N458A; C2 10 uF, R75 1K to pin 21, R77 1K to pin 22 | C |
| A-440 | Q22 TIS97, Q9 2N4058, follower Q4 2N3392; R40 22K (parts table: 2.2K) and C8 .1 into Q14's collector (Q12's base): after both VCAs | C |

## Board 2, contours and keyboard (Fig. 9-7, page 9)

| What | Reading | Conf. |
|---|---|---|
| Attack | Q5 TIS93 from the +9.3 V rail through R7 100 (loudness: Q16, R42 100) to the ATTACK pot and C5 10 uF | C |
| Peak detector | the follower's output through R33 5.6K (loudness: R24 6.8K) against R29 16.9K 1 % to -10 V, **CR3 1N34A** (germanium) into Q4's base; R14 33K holds that base low from Q1's side | C |
| Flip-flop | Q1 / Q4 2N3392, R6 560, R5 3.3K, R4 10K, R3 10K; set through C1 .01 and R26 10K | C |
| Follower | Q22 2N3392 / Q21 TIS93 (filter); R48 10K, R50 560 | C |
| Keyboard | 43 resistors of 10 ohm 1 %, "8.33 ma" from Q9 TIS93 / Q11 TIS92 (R1 590 1 %, R20 / R22 1K 1 %) | C |
| Track and hold | pair Q23 / Q14 2N3392 (tail R52 43K to -10 V, R58 6.2K), C9 .33 on the pitch input, R53 4.7M to +10 V; Q24 2N4058 with R59 3.9K to +10 V and R54 1.5K to -10 V on its collector; R61 330 to pin 8 (GLIDE pot and switch), back on pin 7; Q13 2N4303 "(GLIDE UP)" switched by the trigger through CR5 1N4004; C6 1 uF; follower Q10 2N4303 "(GLIDE DOWN)", R18 3.9K to -10 V; R30 10K back to Q14 | C |
| Glide drives | Q24 saturated: about +9.8 V; Q24 off: R59 / R54, -4.44 V | D |

## Board 3, noise (Fig. 9-8, page 10; R. A. Moog drawing 1431)

The board's lower rail is -10 V (pins 17 / 18): the emitters and the RC branches return to it, not
to ground. Drawing 1431 marks Q3's and Q6's collectors at +3.8 V and the three outputs at -4 dBm.

| What | Reading | Conf. |
|---|---|---|
| Source | Q15 2N3392 (selected), R67 470K, R47 75K, C25 10 uF; C12 5.6 uF, R29 4.7M | C |
| Amplifier | Q12 2N3392, R17 100K, NOISE LEVEL R26 2.5K in its emitter; follower Q4, R9 470, R19 3.3K; white output through C18 .15 to pin 5 | C |
| Pink network | R16 10K; C3 .12 with R8 3.3K; **C2 .033 with R13 220**; C1 2.5 uF into Q3 | C |
| Pink amplifier | Q3 2N3392, R3 3.3K, R1 120 to -10 V, bias R7 / R6 560K decoupled by C4 2.5 uF; output through C20 2.5 uF to pin 6 | C |
| Red | R12 10K into C7 .15, C8 2.5 uF into Q6 2N3392: a common emitter stage, R24 10K load from +10 V, R15 3.9K emitter, base biased from the collector through R28 / R23 2.2M decoupled by C10 .1, C22 .15 across the load; output through C28 10 uF to pin 2, R59 100K. Drawing 1431 has the same stage (Q905, R917, R918, C910) | C |

## Device models (`models.lib`)

Only the gain bins and the type come from data sheets; the other parameters are those of a generic
small-signal silicon planar part. `sensitivity.cir` shows what depends on them: over the TIS97's
bin (hFE 250 to 700) the filter's output stage gains 63 to 71 (68.0 at the typical 440) and the VCA
1.92 to 1.99; a saturation current three times smaller or larger moves them by 0.5 and 1.1 %.
The units draw the first, the second is negligible.

## Benches

Run with `ngspice -b <file>.cir`.

| Bench | What it gives | Used by |
|---|---|---|
| `vcf_op.cir` | circuit 6: operating point, passband gain 56.7, output stage gain 68.0, the emphasis tap's 0.152 | `LadderFilter.h`, `outstage.cir` |
| `outstage.cir` | the output stage's transfer, +6.39 / -3.70 V | `gen_tables.py` -> `FilterOutputStage.h` |
| `vca_op.cir` | circuit 7: operating point, gain against the contour (1.97 at 5 V), -9.0 dB for +2 V on EXT. LOUDNESS (manual: -10 +/- 3 dB) | `Vca.h`, `tests/test_vca.cpp` |
| `vca_large.cir` | Q18's current against the contour, the output's clipping (+3.05 / -2.84 V) | `Vca.h`, `tests/test_vca.cpp` |
| `sensitivity.cir` | the above against the TIS97's gain and saturation current | the units' tolerance draws |
| `contour.cir` | circuit 8, both contour generators on the trigger section, for SUSTAIN 0 to 1: rest, peak and held levels at the output and on the timing capacitor (filter contour: peak 4.43 V, cap 4.96 V; loudness: 5.22 V, cap 6.37 V; the capacitor rests at 0.645 V) | `ContourGen.h`: the junction voltages, the sustain follower's Vbe and gain; `tests/test_contour.cpp`: rest, peaks and the six held levels within 0.04 V |
| `keyboard.cir` | circuit 1, track and hold: the amplifier's rails (+9.87 / -4.44 V, the "glide drives" of board 2), the follower Q10 (out = 0.927 C6 + 0.88 V), GLIDE 5M's time an octave (0.58 s up, 1.07 s down), the hold's drift (about 2 cents a minute with the generic FET cards) | `Glide.h` (the rails and the follower); `tests/test_service_manual.cpp` (2.19): the glide's times at 5M within 3 % |
| `noise.cir` | circuit 3 on its -10 V rail: Q3's collector +2.3 V and Q6's +4.1 V (drawing 1431: +3.8 V); Q3's gain 16.9 and input resistance 26.9K at 1 kHz; red against pink, +4.8 dB at 20 Hz to -19.5 dB at 500 Hz. Writes `noise_ac.dat`, the pink and red transfers from white | `NoiseSource.h` (Q3's gain and input resistance, the red stage's 1.81); `tests/test_service_manual.cpp`: the red against the pink within 1 dB from 20 to 500 Hz |
| `noise_levels.py` | from `noise_ac.dat`: the white noise's bandwidth for which the three outputs come closest to one level, as drawing 1431 marks them: 37 kHz, pink then 1.1 dB above white and red 0.7 dB below | `NoiseSource::whiteBandwidth` |
| `shapers.cir` | board 1's waveshapers: the triangle (+1.53 V at the top of the sawtooth, -1.97 V at its lowest, 1.55 V at the end: 3.49 V peak to peak about -0.21 V), the sawtooth output +/-2.09 V, the rectangle's width for the three bias taps (0 / -1.5 / -2.5 V: high for 50.1 / 31.7 / 19.9 % of the cycle, swinging 0 to -4.3 V unloaded, against the drawing's "(3.5V)": not reconciled). Writes `triangle.dat`, the triangle's transfer, on each run | `VcoCore.h`: the triangle's peak and rounding fitted to `triangle.dat`, the comparator thresholds; `tests/test_vco.cpp`: the triangle within 40 mV, the widths |
| `modulation.cir` | circuit 15 (`board3-modmix.lib`) with MODULATION MIX at oscillator 3, the line loaded by the three MOD inputs and R52, for the wheel at 1.2K, 600 and 200 ohm: gain -0.829, standing offset +0.041 V, the output's floor -1.69 / -1.34 / -1.06 V and its bend into it. Writes `modmix_<wheel>.dat` | `ModulationMix.h` (gain, offset, floor, knee); `tests/test_noise_a440.cpp`: within 0.02 V |

The libraries `board1-shapers.lib`, `board2-contour.lib`, `board2-keyboard.lib`, `board3-noise.lib`, `board4-vca.lib` and `board4-vcf.lib` hold the circuits as subcircuits, one per figure; `models.lib` holds the devices.

`gen_tables.py` regenerates the tabulated headers in `src/circuit/blocks/` (today `FilterOutputStage.h`, from `outstage.cir` and `outstage.dat`). `triangle.dat` is not read by the code: `VcoCore`'s rounded fold was fitted to it once, and `test_vco.cpp` holds nine of its points.
