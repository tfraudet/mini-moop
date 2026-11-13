#!/usr/bin/env python3
"""Regenerates the tables tabulated from ngspice in src/circuit/blocks.

usage: python3 tools/spice/gen_tables.py        (from the repository's root or from here)

FilterOutputStage.h: the transfer of board 4's output stage (outstage.cir).
"""
import pathlib
import re
import subprocess

here = pathlib.Path(__file__).resolve().parent
blocks = here.parent.parent / "src" / "circuit" / "blocks"


def run(netlist):
    out = subprocess.run(["ngspice", "-b", netlist], cwd=here, capture_output=True, text=True, check=True)
    return out.stdout + out.stderr


def filter_output_stage():
    log = run("outstage.cir")
    gain = float(re.search(r"^G0 (\S+)", log, re.M).group(1))
    rows = [line.split() for line in (here / "outstage.dat").read_text().splitlines() if line.strip()]
    values = [float(r[1]) for r in rows]
    assert len(values) == 301, len(values)
    zero = values[150]
    cells = ["%.6f" % (v - zero) for v in values]
    lines = ["        " + ", ".join(cells[i:i + 6]) + "," for i in range(0, len(cells), 6)]
    header = blocks / "FilterOutputStage.h"
    text = header.read_text()
    text = re.sub(r"(static constexpr std::array<double, points> table \{\n).*?(\n    \};)",
                  lambda m: m.group(1) + "\n".join(lines) + m.group(2), text, flags=re.S)
    text = re.sub(r"(smallSignalGain = )-?[0-9.]+;", lambda m: "%s%.4f;" % (m.group(1), gain), text)
    header.write_text(text)
    print("FilterOutputStage.h: gain %.4f, clips at %+.3f / %+.3f V" % (gain, values[0] - zero, values[-1] - zero))


if __name__ == "__main__":
    filter_output_stage()
