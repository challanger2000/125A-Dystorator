#include "dsp/DKCircuit.h"
#include "dsp/PortSolve.h"
#include "dsp/PowerAmp5150.h"
#include <cfenv>
#include <cstring>
#include <iostream>
#include <random>
static bool linear (std::vector<double> a, int n, double *x)
{
    for (int j = 0; j < n; ++j)
    {
        int p = j;
        for (int r = j + 1; r < n; ++r)
            if (std::abs (a[r * (n + 1) + j]) > std::abs (a[p * (n + 1) + j]))
                p = r;
        if (std::abs (a[p * (n + 1) + j]) < 1e-300)
            return false;
        for (int c = j; c <= n; ++c)
            std::swap (a[p * (n + 1) + c], a[j * (n + 1) + c]);
        for (int r = j + 1; r < n; ++r)
        {
            double q = a[r * (n + 1) + j] / a[j * (n + 1) + j];
            for (int c = j + 1; c <= n; ++c)
                a[r * (n + 1) + c] -= q * a[j * (n + 1) + c];
        }
    }
    for (int r = n - 1; r >= 0; --r)
    {
        double s = a[r * (n + 1) + n];
        for (int c = r + 1; c < n; ++c)
            s -= a[r * (n + 1) + c] * x[c];
        x[r] = s / a[r * (n + 1) + r];
    }
    return true;
}
template <int N> bool ports ()
{
    std::mt19937 rng (712 + N);
    std::uniform_real_distribution<double> u (-.5, .5);
    for (int trial = 0; trial < 500; ++trial)
    {
        double k[N * N], d[N][2]{}, r[N], x[N], y[N];
        for (double &v : k)
            v = .05 * u (rng);
        for (int i = 0; i < N; ++i)
        {
            r[i] = u (rng);
            if (i % 2 == 0)
            {
                d[i][0] = trial % 4 == 0 ? 0 : u (rng);
                d[i][1] = trial % 5 == 0 ? 0 : u (rng);
            }
            else
                d[i][1] = trial % 3 == 0 ? 0 : u (rng);
        }
        std::vector<double> a (N * (N + 1));
        for (int row = 0; row < N; ++row)
        {
            for (int col = 0; col < N; ++col)
            {
                double kd = 0;
                for (int j = 0; j < N; ++j)
                {
                    double deriv = 0;
                    if (j % 2 == 0 && (col == j || col == j + 1))
                        deriv = d[j][col - j];
                    if (j % 2 == 1 && col == j)
                        deriv = d[j][1];
                    kd += k[row * N + j] * deriv;
                }
                a[row * (N + 1) + col] = (row == col ? 1. : 0.) - kd;
            }
            a[row * (N + 1) + N] = r[row];
        }
        if (!linear (a, N, x) || !vamp::solvePortStep<N> (k, d, r, y))
            return false;
        for (int i = 0; i < N; ++i)
            if (!std::isfinite (y[i]) || std::abs (x[i] - y[i]) > 1e-12)
                return false;
    }
    return true;
}
int main ()
{
#if VAMP_SIMD_SSE2
    std::cout << "BACKEND " << (vamp::vampHasAvx () ? "AVX" : "SSE2") << '\n';
#else
    std::cout << "BACKEND SCALAR\n";
#endif
    int fail = 0;
    auto check = [&] (bool ok, const char *msg)
    {
        std::cout << (ok ? "PASS " : "FAIL ") << msg << '\n';
        fail += !ok;
    };
    check (ports<2> () && ports<6> () && ports<8> () && ports<12> (),
           "reduced port steps match independent full Jacobian elimination, 2000 conducting/nonconducting "
           "cases");
    bool exact = true, guards = true;
    std::mt19937 rng (17);
    std::uniform_real_distribution<double> u (-1, 1);
    for (int rows = 1; rows <= 128; ++rows)
        for (int cols : {1, 3, 17, 63})
        {
            int ld = (rows + 3) & ~3;
            std::vector<double> a (rows * cols), t (ld * cols), v (cols), want (rows), got (rows + 2, 87654);
            for (auto &x : a)
                x = u (rng);
            for (auto &x : v)
                x = u (rng);
            for (int r = 0; r < rows; ++r)
                for (int c = 0; c < cols; ++c)
                    t[c * ld + r] = a[r * cols + c];
            for (int r = 0; r < rows; ++r)
                for (int c = 0; c < cols; ++c)
                    want[r] += a[r * cols + c] * v[c];
            vamp::DKCircuit::matVecT (t.data (), ld, rows, cols, v.data (), got.data () + 1);
            guards = guards && got.front () == 87654 && got.back () == 87654;
            exact = exact && std::memcmp (want.data (), got.data () + 1, rows * sizeof (double)) == 0;
        }
    check (exact && guards,
           "SIMD/scalar ordered matrix products bit-exact for 512 shapes, odd tails and guard values");
    double K[4] = {1, 0, 0, 1}, d[2][2] = {{1, 0}, {0, 1}}, r[2] = {1, 1}, dp[2];
    check (!vamp::solvePortStep<2> (K, d, r, dp), "singular reduced solve rejects");
    bool finite = true;
    std::feclearexcept (FE_ALL_EXCEPT);
    for (bool exact : {false, true})
        for (double screen : {0., -1.})
        {
            vamp::KorenPentode tube;
            tube.exact = exact;
            double ip, a, b;
            tube.plate (200, -10, screen, ip, a, b);
            finite = finite && ip == 0 && a == 0 && b == 0 && tube.screen (200, -10, screen) == 0;
        }
    check (finite && !(std::fetestexcept (FE_INVALID | FE_DIVBYZERO)),
           "zero/negative screen yields zero finite currents and derivatives without divide-by-zero");
    return fail ? 1 : 0;
}
