// REJECTED split-elimination performance prototype: bit-exact but slower end-to-end.
#pragma once
#include "dsp/Simd.h"
#include <cmath>

namespace vamp
{
// The same partially pivoted Gaussian elimination as PortSolve originally used.
// Columns of a row are independent updates: vectorize them without changing
// pivot choices, division, row order, or multiply-then-subtract arithmetic.
template <int N> inline bool portPivot (double (&a)[N][N + 1], int col, double &inverse)
{
    int piv = col;
    for (int x = col + 1; x < N; ++x)
        if (std::abs (a[x][col]) > std::abs (a[piv][col]))
            piv = x;
    if (std::abs (a[piv][col]) < 1e-300)
        return false;
    if (piv != col)
        for (int j = col; j <= N; ++j)
        {
            const double t = a[col][j];
            a[col][j] = a[piv][j];
            a[piv][j] = t;
        }
    inverse = 1.0 / a[col][col];
    return true;
}

template <int N> inline bool eliminatePorts (double (&a)[N][N + 1], double (&inverse)[N])
{
    for (int col = 0; col < N; ++col)
    {
        if (!portPivot (a, col, inverse[col]))
            return false;
        for (int x = col + 1; x < N; ++x)
        {
            const double f = a[x][col] * inverse[col];
            if (f == 0.0)
                continue;
            int j = col + 1;
#if VAMP_SIMD_SSE2
            const auto multiplier = _mm_set1_pd (f);
            for (; j + 1 <= N; j += 2)
                _mm_storeu_pd (a[x] + j, _mm_sub_pd (_mm_loadu_pd (a[x] + j),
                                                     _mm_mul_pd (multiplier, _mm_loadu_pd (a[col] + j))));
#endif
            for (; j <= N; ++j)
                a[x][j] -= f * a[col][j];
        }
    }
    return true;
}

#if VAMP_SIMD_SSE2
// A single target-isolated call per solve, rather than one AVX call per row.
// AVX (no FMA or AVX2 requirement); the 2-/1-column tails retain the same ops.
template <int N> VAMP_AVX_FN bool eliminatePortsAvx (double (&a)[N][N + 1], double (&inverse)[N])
{
    for (int col = 0; col < N; ++col)
    {
        if (!portPivot (a, col, inverse[col]))
            return false;
        for (int x = col + 1; x < N; ++x)
        {
            const double f = a[x][col] * inverse[col];
            if (f == 0.0)
                continue;
            int j = col + 1;
            const auto multiplier = _mm256_set1_pd (f);
            for (; j + 3 <= N; j += 4)
                _mm256_storeu_pd (a[x] + j,
                                  _mm256_sub_pd (_mm256_loadu_pd (a[x] + j),
                                                 _mm256_mul_pd (multiplier, _mm256_loadu_pd (a[col] + j))));
            if (j + 1 <= N)
            {
                _mm_storeu_pd (a[x] + j,
                               _mm_sub_pd (_mm_loadu_pd (a[x] + j),
                                           _mm_mul_pd (_mm_set1_pd (f), _mm_loadu_pd (a[col] + j))));
                j += 2;
            }
            for (; j <= N; ++j)
                a[x][j] -= f * a[col][j];
        }
    }
    return true;
}
#endif
} // namespace vamp
