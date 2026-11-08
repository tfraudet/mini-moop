// Mini Moop - circuit-simulation synthesizer
// Copyright (C) 2026 Thierry Fraudet
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once
// Runs ngspice in batch mode on a netlist and returns the transient waveforms.
// Used to validate the C++ circuit models against a reference SPICE engine.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace mmtest
{

struct Waveforms
{
    std::vector<double> t;
    std::vector<std::vector<double>> v; // one vector per probe

    /** Linear interpolation of probe `k` at time `time`. */
    double at (size_t k, double time) const
    {
        if (time <= t.front()) return v[k].front();
        if (time >= t.back()) return v[k].back();
        size_t lo = 0, hi = t.size() - 1;
        while (hi - lo > 1)
        {
            const size_t mid = (lo + hi) / 2;
            (t[mid] <= time ? lo : hi) = mid;
        }
        const double f = (time - t[lo]) / (t[hi] - t[lo]);
        return v[k][lo] + f * (v[k][hi] - v[k][lo]);
    }
};

inline bool ngspiceAvailable()
{
    return std::system ("command -v ngspice > /dev/null 2>&1") == 0;
}

/** `netlistBody` holds components and models; `probes` are SPICE vector
    expressions such as "v(out)". Runs `.tran step stop`. */
inline std::optional<Waveforms> runNgspiceTran (const std::string& name,
                                                const std::string& netlistBody,
                                                const std::vector<std::string>& probes,
                                                double step, double stop)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "minimoop_spice";
    fs::create_directories (dir);
    const fs::path cir = dir / (name + ".cir");
    const fs::path out = dir / (name + ".dat");
    fs::remove (out);

    {
        std::ofstream f (cir);
        f << "* " << name << "\n" << netlistBody << "\n";
        f << ".options method=trap reltol=1e-5 abstol=1e-13 vntol=1e-8\n";
        f << ".control\nset wr_singlescale\nset wr_vecnames\noption numdgt=12\n";
        f << "tran " << step << " " << stop << " 0 " << step << "\n";
        f << "wrdata " << out.string();
        for (const auto& p : probes) f << " " << p;
        f << "\n.endc\n.end\n";
    }

    const std::string cmd = "ngspice -b " + cir.string() + " > " + (dir / (name + ".log")).string() + " 2>&1";
    if (std::system (cmd.c_str()) != 0 || ! fs::exists (out))
        return std::nullopt;

    Waveforms w;
    w.v.resize (probes.size());
    std::ifstream f (out);
    std::string line;
    std::getline (f, line); // header (wr_vecnames)
    while (std::getline (f, line))
    {
        std::istringstream ss (line);
        double t;
        if (! (ss >> t)) continue;
        w.t.push_back (t);
        for (auto& col : w.v)
        {
            double val = 0.0;
            ss >> val;
            col.push_back (val);
        }
    }
    if (w.t.size() < 2)
        return std::nullopt;
    return w;
}

/** RMS of (a - b) relative to RMS of b. */
inline double relativeRmsError (const std::vector<double>& a, const std::vector<double>& b)
{
    double e = 0.0, r = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        e += (a[i] - b[i]) * (a[i] - b[i]);
        r += b[i] * b[i];
    }
    return std::sqrt (e / std::fmax (r, 1e-30));
}

} // namespace mmtest
