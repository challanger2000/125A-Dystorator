# P003 — Channel/noise costs and three additional CPU experiments
2026-11-25; exact product baseline 94cb5560b904569a35c12e1077fd6df1dfe8a239, Engineering unchanged at 53b10bd4a81fe6c5d62e110b932aef7155172ad6, START-HERE read fully. No product DSP changes. Same core-only Linux Xeon / GCC 14.2.0 environment as P003-CORE-CPU-BASELINE.md. LF-normalized local core header hashes rechecked; AmpSim/JUCE, actual host and ASIO are not in the benchmark. All three channel settings: STOCK, two distinct input streams, 48k/128, 120 measured unpaced groups, fixed controls, power failure/held counts zero, finite nonzero output. No real DI.
## Channel costs (noise off; original core, one run per cell)
| factor | channel | mean thread CPU us | p99 us | max us | CPU overruns/120 |
|---|---|---:|---:|---:|---:|
|2|CLEAN|780.8|1216.4|1383.2|0|
|2|BLUE|1394.5|2439.6|2595.6|0|
|2|RED|1630.2|2986.9|3285.0|2|
|4|CLEAN|1476.0|3017.7|3327.4|3|
|4|BLUE|2285.1|2908.6|4650.8|8|
|4|RED|2700.7|4137.9|4418.6|53|
|8|CLEAN|2829.8|6042.0|9603.7|57|
|8|BLUE|4347.9|6573.4|7200.5|120|
|8|RED|5162.4|8578.2|9367.2|120|
Budget 2666.7 us. CLEAN is less expensive but cannot substitute for the desired RED circuit.
## Noise costs
New exact executed benchmark tests/p003/core_budget_noise.cpp, blob ec4c7d17f7ef2990f8d080f57c45043456936eb6. Build as original core_budget.cpp, optional last argument 0/1 selects preamp noise after settling, RED fixed. ABBA order off/on/on/off, CPU affinity 3. Values mean/p99/max us, overruns:
|factor|noise|mean|p99|max|over|
|---|---|---:|---:|---:|---:|
|2|off|1615.0|2542.5|4417.2|1|
|2|on|1844.6|2759.2|2771.9|2|
|2|on|1933.4|3410.2|6852.3|5|
|2|off|1665.6|2832.4|6307.8|2|
|8|off|5082.8|7867.4|10272.8|120|
|8|on|6223.9|11215.8|11846.8|120|
|8|on|6189.0|9439.0|9966.7|120|
|8|off|5596.8|15584.9|19716.0|120|
Approx. +15%/+16% group mean with noise on (2x/8x) in these noisy environment samples. Keep optional noise off by default; not a new optimization. Some 8x tests overlapped compilation or measurement on another logical CPU; no precision claim for relative timing. All samples finite, signal energy reproducible to printed precision within each setting.
## Three additional experiments
A=existing core; B=candidate. All three candidates compiled with GCC14.2 -O3 -DNDEBUG and completed the existing rail_parity.cpp run, each binary PCM+rails exactly identical (142692 records / 856152 doubles, SHA256 7cc7c8d0ca7b87bb9d9c758744676ce3d9322c48a6ae879282dfcead4ce5852b). These are noise-off normal-signal channel/automation probes, not full fault/host/DI tests.
1. Row parallel Newton residual: in TriodeNetwork.h and PowerAmp5150.h replace the per-row Ki scalar dot product in the Newton loop with DKCircuit::matVec(K.data(), M, M, i, ki), preserving the same summation order and all residual tolerances. Actual candidate normalized blobs 1dac6aa91485fd418a8cc1ec891f92f68aad2d04 and 903071df0be61cdf3a57be8a38682bb50fb0cbcf. 
2. AVX2 compiler target: unchanged sources, add -march=haswell -ffp-contract=off for the entire headless benchmark. This is a CPU-specific experiment, not a portable shipping build or Windows/MSVC result.
3. Column-parallel Newton residual: allocate padded transposed Kt once in DKCircuit::prepare, transpose K to Kt in rebuild, use matVecT through multiplyK(i,ki) in both Newton loops. Original K retained for solvePortStep. Candidate normalized blobs DKCircuit 51e68fb052ee9e3f0942ef858f867bdd59db891c, TriodeNetwork 33eab88dd7aa07c858cfccbe48450781bc918bae, PowerAmp 48ce1c4d1c1247a783f9df3973358cdf724e953c. 
ABBA, RED, noise off, affinity 2, each cell is mean thread CPU us / p99 / CPU deadline overruns:
| candidate | factor | A1 | B1 | B2 | A2 |
|---|---:|---|---|---|---|
| row parallel |2|1590.5/2396.6/1|1523.2/2428.4/1|2129.2/11209/12|1938.0/3517.3/20|
| row parallel |4|2822.4/4018/72|2780.3/3878.1/69|2728.4/3913.9/59|2691.9/3795.4/54|
| row parallel |8|5489.4/11176.8/120|5088.0/7230.9/120|6314.0/12190.7/120|5867.0/12304.5/120|
| AVX2 compiler |2|1736.3/2988.7/6|1657.6/2443.7/1|1670.6/2601.2/1|1668.1/3612.4/3|
| AVX2 compiler |8|5036.1/7712.3/120|5624.6/12406.9/120|5100.4/6131.9/120|5340.4/10894.9/120|
| column parallel |2|1595.1/2415.4/0|1688.4/4128.8/5|2917.7/6528.1/55|1635.6/2952.9/4|
| column parallel |8|5164.1/8267.4/120|4990.1/6700.6/120|5089.5/8389.7/120|5046.7/7371.2/120|
Large variance; no repeatable useful gain across factors. All 8x runs exceed CPU deadline. Candidate row parallel and column parallel rejected; AVX2 global compiler target not adopted. No loosening of Newton tolerances or change in oversampling. Original circuit remains the reference and active product code is unchanged. 
## Next engineering direction
The four matrix-only experiments so far provide no reliable material gain. Do not repeat those variations indefinitely. Investigate exact topology reduction and stereo-lane vectorization of the actual coupled solver; compare against existing rejected step3 prototypes and original 1.0.0. Separately determine whether sharing an immutable prepared idle snapshot across the three lanes per rate could reduce preparation time without sharing mutable DSP state; this would not be a steady-state CPU win. Full JUCE 8.0.4 source/host environment still unavailable locally: build and run current P002b regressions before accepting any product optimization. Windows native, ASIO, full VST3, DI, final GUI and DAW acceptance NOT RUN; no release. No CI triggered.
