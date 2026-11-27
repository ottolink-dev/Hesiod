# Application memory evidence

`memory-fix-comparison.csv` contains all 32 paired cells; the JSON adds per-run fixture/artifact checks and provenance. Each fixed series and the separate baseline calibration have plain config, provenance, result and policy-audit files, plus a deterministic `run-evidence.tar.gz` containing per-run fixtures/logs/measurements, runner snapshots and build context.

`manifest.json` hashes the 39 original generated evidence files. The parent `SHA256SUMS` also covers this explanatory README. No executables, copied application data, exports or caches are archived here; their hashes and original local paths remain in provenance/results.

The scripts retain the original workspace layout (`output/fixes` beneath the repository root). To rerun them, restore that layout and the referenced baseline series, or adapt their `ROOT`/input paths. The committed `scripts/profiling/memory_study.py` provides the general run/report CLI. Source/binary association for the old executable is explicit in the calibration report; the current orchestration HEAD is not its build revision.

`archive-verification.json` records manifest, gzip CRC and complete tar-member checks. The initial orchestration exit 1 came from the deliberately strict data allowlist flagging the generated version label; final comparator/calibration exits are 0 after that UI-only difference was reviewed. Both initial and final logs remain retained.
