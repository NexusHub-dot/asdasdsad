# Pulse Macro 1.4.9

Windows x64 · Geometry Dash 2.2081 · Geode 5.10.1

Pulse records and plays 240 Hz macros, keeps the existing 20-slot macro library, practice recording, UI and playback auto-repair, and includes Frame Windows.

This revision focuses on frame-window methodology: immutable baseline validation, natural event crossings, execution of the next meaningful input plus a settling suffix, fuller checkpoint input restoration, persistent warnings, detailed exports, and automatic checkpoint-versus-full-restart comparisons. Existing reference timelines can be reused; analysis version 6 invalidates older measured widths. Stronger validation may reject a previously accepted but non-reproducible reference.

Read [the complete audit](FRAME_WINDOWS_AUDIT.md) for the algorithms, evidence, endpoint tradeoffs and remaining accuracy limits. See [validation](VALIDATION.txt) for actual tests/build results and [Windows build instructions](BUILD_WINDOWS.md) to rebuild. Historical release notes are in `docs/` and do not describe current behavior.

## Controls

- F6: record Pulse and the linked frame reference.
- F8: stop and save.
- F7: play the selected macro.
- F4: Frame Windows controls, results, export and settings.
- F3: show/hide the frame counter.
- F5: optional standalone frame-reference recording.
- P: pause/resume.

Practice checkpoints must belong to the current recording. Normal playback starts at level start. Auto-repair behavior and its backup workflow are preserved.

## Measurement

Shift exactly one event in whole 240 Hz ticks and replay naturally. Every release remains in replay. Ship/wave/robot releases are independent targets; cube/ball/UFO/spider/swing releases are replay-only. Dormant P2 is excluded as a target. Mode is determined at the original input boundary.

The width is the count of contiguous passing discrete offsets, including zero: `-3..+2` means **6f**, with a timestamp span of 5 ticks. The default endpoint executes the next meaningful input group and four more steps. This is a finite local-survival measurement; it does not promise eventual completion. Full-macro mode is also available.

Developer settings:

- **Force full restart analysis**
- **Compare checkpoints versus full restart** (automatically runs both and exports all differences)
- **Strict validation** (additional selected candidate checks)
- **Next action settling steps** (0–240; persisted with results)

Do not treat matching histogram totals as proof of accuracy. The included binary is compiled but has **not been run in Geometry Dash**. No apps were controlled and no mod was installed during this work.

## Standalone tests

```powershell
cmake -S . -B build-core -DPULSE_CORE_ONLY=ON -DBUILD_TESTING=ON
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

Seven C++ suites run without Geode; an eighth offline comparison suite runs when Python 3 is available. Tests cover all 24 requested scenarios and randomized event overlay checks. They exercise reusable algorithms and synthetic physics, not the native game.

To compare exported files offline:

```powershell
python tools/compare_windows.py accelerated.csv full-restart.csv --output diff.csv
```
