# P003 — Structured solver: noise, fault recovery, fallback and rejected grid specialization

2026-11-25. Baseline ca937e9391e1dc6d6528f44c13b222cc1816dbe0. Engineering unchanged 53b10bd4a81fe6c5d62e110b932aef7155172ad6, START-HERE fully read, same assignment. Linux GCC14.2 -std=c++17 -O3 -DNDEBUG. Existing local prototype exact normalized Git blobs RankedPortPrototype bd88a4afc9dd638efdd489556f2932bce2a71949, DKCircuit 721c0fcf4c5a08c6c339186648c9eba574df7242, TriodeNetwork ec7e9b4ef347161cc6f8a579c50735629985e4aa. No product DSP changes.

## Executed evidence
- Noise-on parity: copy tests/p001/rail_parity.cpp, add pre.setNoise(true) immediately after pre.setModeNow(Preamp5150::Red), test source blob 073b2273d84dca33cdb68821fb69df44be8d1262. Compile/run independently with combined pre-prototype core and ranked core. Both exit 0. 142692 records, 6 fields each, all finite. Speaker max absolute delta 3.11335313e-9 V, RMS 1.02651380e-10 V, relative energy residual -227.1496404 dB. Preamp max 7.67713004e-9 V; rail max 3.35546702e-10 and 3.12695647e-10 V. SHA256 before a2d303d44dc8ba12b356e1635c80c9b434b315e252709617a8aeba251b6995de, after f97aded3f53188ba7976ab59316cfb73e0dd2795841d0527264d0212fb1ada21. Same deterministic noise seed, noise enabled after DC settle; not bit-identical, not real guitar DI.
- Original faultRecoveryTests() extracted unchanged from the verified Djenty Donkey 1.0.0 tests/dsp_test.cpp (from function signature through immediately before "// One-sided noise density"). Standalone TU with algorithm/cmath/cstdio/vector, dsp/Amp5150.h and dsp/Filters.h, using namespace vamp; main returns faultRecoveryTests()?0:1. First compilation lacked Filters.h, corrected without changing any test criteria. Executed source normalized Git blob e706d1a2fce748bf623525066133b4470e8f8523. Against ranked prototype 19/19 PASS, process exit 0, text log SHA256 8a53352932b3eed64628bc9a4385d6e5deb4442bebcd01ac3b002522d11e5591 exactly equal to original P001 fault.log. NaN/Inf, forced retry/hold, corrupt state, 2ms persistent faults, 1kV spike and 100V sine. Persistent fault recovery times 64.03/83.07/28.78 ms, not a universal 1ms PASS. Extreme peak 54.3/58.7 V, zero reported Newton failures in those cases.
- New independent fallback test tests/p003/ranked_fallback.cpp, blob 45c39f3878609f74da41b2386f25a87683a8ce5a. Structured first 2x2 diagonal block singular while full matrix nonsingular. Prototype rejects, dense original solve succeeds, original equation independently reconstructed, max residual 1.3877787807814457e-17. Six additional NaN/Inf rejection cases at RHS, derivative and K diagonal; all reject. Exit 0. Command: g++ -std=c++17 -O3 -DNDEBUG -I <disposable-ranked>/Source tests/p003/ranked_fallback.cpp -o ranked-fallback && ./ranked-fallback. This tests fallback at solver-call boundary, not exhaustive ill-conditioned physical cases.

## Additional experiment — nonconducting grid specialization (REJECTED)
If d2==0, the second row imposes q_grid=0. Replace the 2x2 elimination with the scalar plate equation and zero grid solution/slope. Other cases and full residual guard unchanged. Exact experimental normalized header blob 0ce4e953053c4cb523a2a06d356e8c1ed92a2699. Insert before const double X0 in existing solve():
```cpp
   if(d2==0.0){
    const double X0=-(d0*x[k]+d1*x[k+1]);
    const double U0=-(d0*u[k]+d1*u[k+1])-X0*S;
    const double Y0=y[k]-S*v[k];
    const double a=1-(d0*K[k*M+k]+d1*K[(k+1)*M+k])-X0*S*v[k];
    const double f=d0*r[k]+d1*r[k+1]-X0*T;
    if(!std::isfinite(a)||std::abs(a)<=1e-10*std::max(1.,std::abs(a)))return false;
    const double inv=1/a;
    w[k]=f*inv;z[k]=U0*inv;w[k+1]=z[k+1]=0;
    S+=Y0*z[k];T+=Y0*w[k];
    continue;
   }
```
10,000 randomized structured systems accepted, max error 6.6613381477509392e-16, 10,000 damaged structures rejected, exit 0. Rail parity noise off binary SHA256 fcdd4ee7cd59cb5be51d616dd0973c37390f014de407dbcc463332f9c0aeebea identical to previous ranked prototype output, NOT bit-identical to original 1.0.0. No fault suite/noise-on run for this additional variant.

Paired ABBA with A=ranked prototype, B=scalar-grid variant, core_budget_noise.cpp, RED/noise off, 48k/128, pinned CPU 2, 120 groups each. Values mean/p99/max CPU microseconds; deadline overrun count:
|factor|A1|B1|B2|A2|
|---|---|---|---|---|
|2|1431.0/2284.8/2285.9;0|1421.1/2717.7/3413.9;2|1416.5/2460.8/2683.3;1|1475.9/2362.7/2366.6;0|
|8|4590.5/5914.9/6451.4;120|4970.9/8255.7/42750.5;120|4683.2/7629.4/8672.8;120|4636.3/6053.7/6590.3;120|
All finite and nonzero stereo output, same printed energy per factor; 0 power fails/holds. No reliable improvement, worse tails. Reject scalar-grid variant; do not change saved ranked prototype or production solver. No new CPU savings proven in this continuation.

## Next direction
Stop small variations of preamp rank solver. Analyze remaining coupled power amp port structure (rank >1 in current ordering) and exact passive state-space reduction; compare low-rank/structured approaches against full coupling and existing pivot solver. More importantly, obtain full JUCE 8.0.4 compile/host baseline for P002b and realistic DI before claiming any product performance gain. Existing rank prototype remains experimental until conditioning, actual end-to-end deadline benefit and native Windows acceptance are proven. No CI triggered. Full host/Windows/ASIO/DI/DAW NOT RUN. No release.
