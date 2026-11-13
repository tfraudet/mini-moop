#!/usr/bin/env python3
"""The noise outputs' levels against the white noise's bandwidth.

R. A. Moog drawing 1431 marks the three outputs of board 3, white, pink and red, at the same
level: -4 dBm each. The pink and red levels follow from the white noise's spectrum through the
drawn networks (noise.cir writes their transfers to noise_ac.dat); the white noise's bandwidth,
the junction and Q12's, is not on the drawing. This finds the bandwidth for which pink comes
out at white's level, and reports red's level with it.

usage: python3 tools/spice/noise_levels.py   (after ngspice -b noise.cir, from tools/spice)
"""
import math
import pathlib

here = pathlib.Path(__file__).resolve().parent
rows = [list(map(float, l.split())) for l in (here / "noise_ac.dat").read_text().splitlines() if l.strip()]
f = [r[0] for r in rows]
pink = [r[1] for r in rows]
red = [r[3] for r in rows]


def levels(bw):
    """dB of pink and red against white, white flat up to a one-pole bandwidth `bw` (Hz)."""
    sw = sp = sr = 0.0
    for i in range(1, len(f)):
        df = f[i] - f[i - 1]
        fm = math.sqrt(f[i] * f[i - 1])
        s = 1.0 / (1.0 + (fm / bw) ** 2)
        g = lambda v: 0.5 * (v[i] ** 2 + v[i - 1] ** 2)
        sw += s * df
        sp += s * g(pink) * df
        sr += s * g(red) * df
    return 10 * math.log10(sp / sw), 10 * math.log10(sr / sw)


for bw in (5e3, 10e3, 20e3, 40e3, 80e3):
    p, r = levels(bw)
    print("white bandwidth %6.0f Hz: pink %+5.1f dB, red %+5.1f dB against white" % (bw, p, r))
lo, hi = 1e3, 1e6
for _ in range(60):
    mid = math.sqrt(lo * hi)
    (lo, hi) = (mid, hi) if levels(mid)[0] > 0 else (lo, mid)
p, r = levels(lo)
print("pink at white's level (drawing 1431) for a %.0f Hz bandwidth; red then %+.1f dB" % (lo, r))
