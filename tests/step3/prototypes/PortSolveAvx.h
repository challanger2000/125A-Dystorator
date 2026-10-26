// REJECTED whole-solve AVX performance prototype: full-path timing regressed.
#pragma once
#include "dsp/Simd.h"
#include <cmath>
#if VAMP_SIMD_SSE2
namespace vamp
{
// Same port system, pivoting, arithmetic order, tolerances and back substitution
// as the scalar implementation in PortSolve.h. Only independent columns of an
// elimination row are SIMD updated. Keep the whole solve in one AVX function:
// splitting elimination into a separate call regressed the full plugin benchmark.
template <int M, int n>
VAMP_AVX_FN bool solvePortStepNAvx (const double* K, const double (&d)[M][2], const double (&r)[M], const int* act, double (&dp)[M])
{
    // rows of (I - D K) and D r over the active ports
    double a[n][n + 1];
    for (int x = 0; x < n; ++x)
    {
        const int k = act[x];
        const double* Kk = K + k * M;
        if (k % 2 == 0)
        {
            const double d0 = d[k][0], d1 = d[k][1];
            const double* Kg = Kk + M;
            for (int y = 0; y < n; ++y) a[x][y] = -(d0 * Kk[act[y]] + d1 * Kg[act[y]]);
            a[x][n] = d0 * r[k] + d1 * r[k + 1];
        }
        else
        {
            const double d1 = d[k][1];
            for (int y = 0; y < n; ++y) a[x][y] = -d1 * Kk[act[y]];
            a[x][n] = d1 * r[k];
        }
        a[x][x] += 1.0;
    }

    double q[n];
    double invPivot[n];
    for (int col = 0; col < n; ++col)
    {
        int piv = col;
        for (int x = col + 1; x < n; ++x)
            if (std::abs (a[x][col]) > std::abs (a[piv][col])) piv = x;
        if (std::abs (a[piv][col]) < 1e-300) return false;
        if (piv != col)
            for (int j = col; j <= n; ++j) { const double t = a[col][j]; a[col][j] = a[piv][j]; a[piv][j] = t; }
        invPivot[col] = 1.0 / a[col][col];
        for (int x = col + 1; x < n; ++x)
        {
            const double f = a[x][col] * invPivot[col];
            if (f == 0.0) continue;
            int j = col + 1;
            const auto multiplier = _mm256_set1_pd (f);
            for (; j + 3 <= n; j += 4)
                _mm256_storeu_pd (a[x] + j, _mm256_sub_pd (_mm256_loadu_pd (a[x] + j),
                    _mm256_mul_pd (multiplier, _mm256_loadu_pd (a[col] + j))));
            if (j + 1 <= n)
            {
                _mm_storeu_pd (a[x] + j, _mm_sub_pd (_mm_loadu_pd (a[x] + j),
                    _mm_mul_pd (_mm_set1_pd (f), _mm_loadu_pd (a[col] + j))));
                j += 2;
            }
            for (; j <= n; ++j) a[x][j] -= f * a[col][j];
        }
    }
    for (int x = n - 1; x >= 0; --x)
    {
        double s = a[x][n];
        for (int j = x + 1; j < n; ++j) s -= a[x][j] * q[j];
        q[x] = s * invPivot[x];
    }

    for (int k = 0; k < M; ++k)
    {
        const double* Kk = K + k * M;
        double s = r[k];
        for (int y = 0; y < n; ++y) s += Kk[act[y]] * q[y];
        dp[k] = s;
    }
    return true;
}
} // namespace vamp
#endif
