#!/usr/bin/env python3
"""The noise outputs' levels against the white noise's bandwidth.

R. A. Moog drawing 1431 marks the three outputs of board 3, white, pink and red, at the same
level: -4 dBm each. The pink and red levels follow from the white noise's spectrum through the
drawn networks (noise.cir writes their transfers to noise_ac.dat); the white noise's bandwidth,
the junction and Q12's, is not on the drawing. This finds the bandwidth for which the three
levels come closest to one another (least squares about their mean).

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


def spread(bw):
    """Variance of the three levels (white at 0 dB) about their mean."""
    p, r = levels(bw)
    m = (0.0 + p + r) / 3.0
    return (m * m + (p - m) ** 2 + (r - m) ** 2) / 3.0


lo, hi = 1e3, 1e6
for _ in range(80):  # golden-section search on log(bandwidth)
    a, b = math.exp(math.log(lo) + 0.382 * (math.log(hi) - math.log(lo))), math.exp(math.log(lo) + 0.618 * (math.log(hi) - math.log(lo)))
    (lo, hi) = (lo, b) if spread(a) < spread(b) else (a, hi)
p, r = levels(lo)
print("the three levels closest to one another (drawing 1431: -4 dBm each) for a %.0f Hz bandwidth:"
      " pink %+.1f dB, red %+.1f dB against white" % (lo, p, r))
