#!/usr/bin/env python3
"""The A-440's harmonics, from a440.cir's output (a440.dat): over a whole number of periods.

usage: python3 tools/spice/a440_harmonics.py   (after ngspice -b a440.cir, from tools/spice)
"""
import math
import pathlib

here = pathlib.Path(__file__).resolve().parent
rows = [list(map(float, l.split())) for l in (here / "a440.dat").read_text().splitlines() if l.strip()]
t = [r[0] for r in rows]
v = [r[1] for r in rows]
mean = sum(v) / len(v)
# rising crossings of the mean, interpolated
cross = [t[i] + (mean - v[i]) * (t[i + 1] - t[i]) / (v[i + 1] - v[i])
         for i in range(len(v) - 1) if v[i] < mean <= v[i + 1]]
n = len(cross) - 1
f0 = n / (cross[-1] - cross[0])
lo, hi = cross[0], cross[-1]


def harmonic(k):
    re = im = 0.0
    for i in range(len(t) - 1):
        if t[i] < lo or t[i + 1] > hi:
            continue
        dt = t[i + 1] - t[i]
        x = 0.5 * (v[i] + v[i + 1]) - mean
        ph = 2 * math.pi * k * f0 * 0.5 * (t[i] + t[i + 1])
        re += x * math.cos(ph) * dt
        im += x * math.sin(ph) * dt
    return 2 * math.hypot(re, im) / (hi - lo)


h1 = harmonic(1)
print("fundamental %.2f Hz, %.4f V peak" % (f0, h1))
for k in range(2, 7):
    print("harmonic %d: %.2f %%" % (k, 100 * harmonic(k) / h1))
