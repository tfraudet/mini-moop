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
// Fixed-size dense LU factorisation with partial pivoting. No allocation.

#include <array>
#include <cmath>
#include <utility>

namespace mm
{

template <int N>
struct Matrix
{
    std::array<double, N * N> a {};
    double& operator() (int r, int c) noexcept { return a[size_t (r * N + c)]; }
    double operator() (int r, int c) const noexcept { return a[size_t (r * N + c)]; }
    void clear() noexcept { a.fill (0.0); }
};

template <int N>
using Vector = std::array<double, size_t (N)>;

/** Solve A x = b in place (A is destroyed, b becomes x). Returns false if singular. */
template <int N>
inline bool luSolveInPlace (Matrix<N>& A, std::array<double, size_t (N)>& b) noexcept
{
    for (int k = 0; k < N; ++k)
    {
        int piv = k;
        double best = std::abs (A (k, k));
        for (int r = k + 1; r < N; ++r)
        {
            const double v = std::abs (A (r, k));
            if (v > best) { best = v; piv = r; }
        }
        if (best < 1.0e-300)
            return false;
        if (piv != k)
        {
            for (int c = 0; c < N; ++c)
                std::swap (A (k, c), A (piv, c));
            std::swap (b[size_t (k)], b[size_t (piv)]);
        }
        const double inv = 1.0 / A (k, k);
        for (int r = k + 1; r < N; ++r)
        {
            const double f = A (r, k) * inv;
            if (f == 0.0)
                continue;
            A (r, k) = 0.0;
            for (int c = k + 1; c < N; ++c)
                A (r, c) -= f * A (k, c);
            b[size_t (r)] -= f * b[size_t (k)];
        }
    }
    for (int k = N - 1; k >= 0; --k)
    {
        double s = b[size_t (k)];
        for (int c = k + 1; c < N; ++c)
            s -= A (k, c) * b[size_t (c)];
        b[size_t (k)] = s / A (k, k);
    }
    return true;
}

} // namespace mm
