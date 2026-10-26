#include "prototypes/PortElimination.h"
#include <cstring>
#include <iostream>
#include <random>

template <int N> bool reference (double (&a)[N][N + 1], double (&inverse)[N])
{
    for (int col = 0; col < N; ++col)
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
                double t = a[col][j];
                a[col][j] = a[piv][j];
                a[piv][j] = t;
            }
        inverse[col] = 1. / a[col][col];
        for (int x = col + 1; x < N; ++x)
        {
            const double f = a[x][col] * inverse[col];
            if (f == 0.)
                continue;
            for (int j = col + 1; j <= N; ++j)
                a[x][j] -= f * a[col][j];
        }
    }
    return true;
}
template <int N> bool exercise ()
{
    std::mt19937 rng (22591 + N);
    std::uniform_real_distribution<double> value (-5., 5.);
    for (int trial = 0; trial < 1000; ++trial)
    {
        struct Matrix
        {
            std::uint64_t before = 0x125a76543210;
            double a[N][N + 1]{};
            std::uint64_t after = 0xabcdef125a;
        } expected, actual;
        for (int r = 0; r < N; ++r)
            for (int c = 0; c <= N; ++c)
                expected.a[r][c] = value (rng);
        for (int r = 0; r < N; ++r)
            expected.a[r][r] += N;
        // Forced zero coefficients/whole singular column and nontrivial pivots.
        if (trial % 13 == 0)
            for (int r = 0; r < N; ++r)
                expected.a[r][0] = 0.;
        if (trial % 7 == 0)
            for (int c = 0; c <= N; ++c)
                std::swap (expected.a[0][c], expected.a[N - 1][c]);
        actual = expected;
        double invReference[N]{}, invActual[N]{};
        const bool okReference = reference (expected.a, invReference);
#if VAMP_SIMD_SSE2
        const bool okActual = vamp::vampHasAvx () ? vamp::eliminatePortsAvx (actual.a, invActual)
                                                  : vamp::eliminatePorts (actual.a, invActual);
#else
        const bool okActual = vamp::eliminatePorts (actual.a, invActual);
#endif
        if (okActual != okReference || std::memcmp (actual.a, expected.a, sizeof (actual.a)) ||
            std::memcmp (invActual, invReference, sizeof (invActual)) || actual.before != expected.before ||
            actual.after != expected.after)
        {
            std::cerr << "FAIL elimination N=" << N << " trial=" << trial << '\n';
            return false;
        }
    }
    if constexpr (N < 12)
        return exercise<N + 1> ();
    return true;
}
int main ()
{
    bool ok = exercise<1> ();
    std::cout << (ok ? "PASS" : "FAIL")
              << " 12000 exact original-order elimination matrices, pivot/singularity/tails/guards"
              << std::endl;
    return ok ? 0 : 1;
}
