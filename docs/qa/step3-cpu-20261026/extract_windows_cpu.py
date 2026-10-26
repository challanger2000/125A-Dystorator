"""Derive reviewable tables from the Windows host's printed case aggregates.

Usage: python extract_windows_cpu.py docs/qa/step3-windows-<source SHA>
No interpolation/pooling of percentiles and no exclusion of failing cases.
"""
import csv
import hashlib
import json
import math
import pathlib
import sys

directory = pathlib.Path(sys.argv[1])
keys = ("fs", "lanes", "quality", "frames", "mode")
measurements = ("mean_us", "p95_us", "p99_us", "max_us", "deadline_us",
                "overruns", "schedule_misses", "left_energy", "right_energy",
                "component_state", "status")
versions = {}
hashes = {}
expected = {(str(fs), str(lanes), str(quality), str(frames), mode)
            for fs in (44100, 48000) for lanes in (1, 2) for quality in (0, 1, 2)
            for frames in (128, 256) for mode in ("offline", "paced")}
for version in ("before", "after"):
    raw = directory / f"cpu-{version}.txt"
    hashes[raw.name] = hashlib.sha256(raw.read_bytes()).hexdigest()
    rows = []
    for line in raw.read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("VST3_CPU "):
            row = dict(word.split("=", 1) for word in line.split()[1:])
            assert row["version"] == version
            assert all(name in row for name in keys + measurements)
            for name in measurements[:-2]:
                assert math.isfinite(float(row[name])), (version, name, row)
            assert row["status"] in ("PASS", "FAIL") and row["component_state"] in ("PASS", "FAIL")
            assert 0 <= int(row["overruns"]) <= 160 and 0 <= int(row["schedule_misses"]) <= 160
            assert 0 <= float(row["p95_us"]) <= float(row["p99_us"]) <= float(row["max_us"])
            assert abs(float(row["deadline_us"]) - 1e6 * int(row["frames"]) / int(row["fs"])) < .02
            rows.append(row)
    assert len(rows) == 48, f"Incomplete {version} matrix: {len(rows)}/48; no full comparison"
    indexed = {tuple(row[name] for name in keys): row for row in rows}
    assert len(indexed) == 48, "Duplicate case identifiers"
    assert indexed.keys() == expected, "Matrix does not cover requested rates/modes/qualities/blocks"
    versions[version] = indexed
    with (directory / f"cpu-{version}.csv").open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=("version",) + keys + measurements, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

assert versions["before"].keys() == versions["after"].keys()
comparison = []
for key, before in versions["before"].items():
    after = versions["after"][key]
    row = {name: before[name] for name in keys}
    for name in measurements:
        row["before_" + name] = before[name]
        row["after_" + name] = after[name]
    row["mean_saving_percent"] = 100 * (1 - float(after["mean_us"]) / float(before["mean_us"]))
    comparison.append(row)
with (directory / "cpu-comparison.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=tuple(comparison[0]), lineterminator="\n")
    writer.writeheader()
    writer.writerows(comparison)

summary = {
    "sourceCommit": json.loads((directory / "metadata.json").read_text(encoding="utf-8-sig"))["sourceCommit"],
    "derivedFromSHA256": hashes,
    "method": "One pair on one shared runner; same flags and fixture. Percentiles remain per case; no pooled quantiles. Mean saving averages relative case means, not a confidence interval.",
    "hardwareQualification": "Windows CI, NOT owner i9-11900K or real ASIO/DAW",
}
for version, indexed in versions.items():
    rows = list(indexed.values())
    summary[version] = {
        "cases": len(rows),
        "callbacksMeasured": len(rows) * 160,
        "pacedCallbacksMeasured": sum(row["mode"] == "paced" for row in rows) * 160,
        "casesPASS": sum(row["status"] == "PASS" for row in rows),
        "casesFAIL": sum(row["status"] == "FAIL" for row in rows),
        "callbackOverruns": sum(int(row["overruns"]) for row in rows),
        "pacedScheduleMisses": sum(int(row["schedule_misses"]) for row in rows),
        "componentStateAllPASS": all(row["component_state"] == "PASS" for row in rows),
        "bothChannelsFiniteAndAudible": all(float(row["left_energy"]) > 1e-8 and float(row["right_energy"]) > 1e-8 for row in rows),
    }
summary["qualityMeanRelativeSavingPercent"] = {
    quality: sum(row["mean_saving_percent"] for row in comparison if row["quality"] == quality)
             / sum(row["quality"] == quality for row in comparison)
    for quality in ("2", "1", "0")
}
summary["printedChannelEnergyEqualAllCases"] = all(
    row[f"before_{channel}_energy"] == row[f"after_{channel}_energy"]
    for row in comparison for channel in ("left", "right")
)
(directory / "cpu-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps(summary, indent=2))
