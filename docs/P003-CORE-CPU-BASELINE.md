# P003 — Core-CPU baseline and rejected matrix fusion
2026-11-25. Product baseline 11a2bc621db06a4730c5294849ce7d4129c995c4; Engineering 53b10bd4a81fe6c5d62e110b932aef7155172ad6, START-HERE read fully, unchanged reading receipt. No product DSP modification.
## Environment / scope
Linux x86_64 KVM, Intel Xeon Platinum 8573C, GCC 14.2.0 -std=c++17 -O3 -DNDEBUG. Source reconstructed from 1.0.0; all 15 compiled amp-core dependency headers compared LF-normalized to current Git tree (exact blob identities). No JUCE checkout available; git ls-remote GitHub fails DNS. Consequently **not** the P002b AmpSim/host/plugin parity test. No Windows or owner i9 inference.
tests/p003/core_budget.cpp is the exact executed Linux benchmark (blob 43136c0d69c49336b134e0f7cc49ddb2fe45d2b8). Two independent coupled circuits, STOCK, RED, noise off, fixed gain .75, master .46, other controls as in source; distinct precomputed synthetic guitar-like multitone per lane, native internal sample grid (not oversampler output); 24 warm-up groups then 120 unpaced 128-host-frame groups at 48 kHz, 2/4/8 internal factors. Includes energy/finite checks, actual circuit and supply calculations. No JUCE resampling, automation, cabinet, gate, host, ASIO or paced scheduling. CLOCK_THREAD_CPUTIME_ID and steady_clock; measured adjacent CPU clock mean ~0.17–0.24 us, no overhead subtraction. 2666.7 us budget per group. All energy nonzero, no power solver failures or held samples; no claim about all preamp fault counters.
## Initial baseline (microseconds)
factor | mean CPU | p95 CPU | p99 CPU | max CPU | CPU over /120 | wall over /120
---|---:|---:|---:|---:|---:|---:
2 first |1855.6|3076.9|4034.1|4360.4|14|14
2 repeat |1937.4|3529.8|4153.8|4493.2|12|12
4|2625.6|3256.1|3685.2|4363.0|43|43
8|5080.3|7549.2|8869.1|9239.9|120|120

**FAIL for this limited deadline experiment** at 4/8x and variable 2x. Not an owner CPU score, not full-plugin RT QA.
## Instrumented profile
Separate -pg run at 8x RED: CPU mean 7492.0 us vs uninstrumented 5080.3 in initial run; substantial instrumentation/environment distortion. gprof includes DC preparation and excludes/under-represents some library time, so use only as hotspot direction: matVecTAvx 23.72%, TriodeNetwork<6>::solve 19.87%, solvePortStep<10> 10.26%, PowerAmp5150::process 8.97%, DKCircuit::finish 7.69%, solvePortStep<12> 5.77%, PowerAmp5150::evalDevices 5.13%. No product speedup inferred.
## Experimental matrix fusion (REJECTED)
Local-only DKCircuit.h candidate blob 6eb424e (abbreviated): in prepare allocate MOt with ldMO=round_up_4(numS+numO), nz columns; after transpose in rebuild copy state and output rows from Mt/Ot into each column of MOt; for outputs.size()>2 finish copies port currents to z, invokes matVecT once with combined state/output rows, writes output x before copying new states into z. For <=2 output nodes retain original path. Separate evalOutputs unchanged. Same per-row sum order, no circuit or model reduction. Test rail_parity.cpp against original 1.0.0: 142692 records / 856152 doubles, binary identical, SHA256 7cc7c8d0ca7b87bb9d9c758744676ce3d9322c48a6ae879282dfcead4ce5852b. Noise on / all backends / full host are NOT RUN for this candidate.
Paired ABBA measurements (mean/p99/max CPU us; overruns /120):
factor | order | mean | p99 | max | over
---|---|---:|---:|---:|---:
2|A|1504.3|2276.7|2406.5|0
2|B|1791.0|3748.4|4437.3|12
2|B|1956.4|4306.4|5649.4|14
2|A|1585.3|2544.3|2742.3|1
8|A|4772.0|6438.3|7131.1|120
8|B|4569.7|6189.5|6276.8|120
8|B|5407.1|8008.0|9354.9|120
8|A|4718.4|6121.4|6233.6|120

Energies match printed precision within each rate/variant. At 2x candidate both worse, at 8x inconsistent and no deadlines saved. Matrix duplication increases memory/rebuild work. No robust benefit, **do not merge**. Keep original DK implementation. No tolerance relaxation and no lower oversampling to manufacture PASS.
## Next
Measure steady-state RED vs BLUE/CLEAN and noise, separate prepare from hot profile, identify exact algebraic reductions in nonlinear solver/linear products and test candidate via paired full signal checks. Investigate no-heap prepare/transition workload and P002b compile/host parity as soon as exact JUCE 8.0.4 build environment is available. Prior baseline test does not cover the new active AmpSim. No CI triggered; native Windows, full plugin, DI, GUI and DAW NOT RUN.
