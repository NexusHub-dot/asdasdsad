HISTORICAL NOTES � supplied with v1.4.8; not current methodology or validation.
See ../FRAME_WINDOWS_AUDIT.md and ../VALIDATION.txt for this revision.

## v1.4.8 stable macro-library identity fix

Pulse now canonicalizes server-backed levels with `m_levelID`, then safely falls back to `m_originalLevel` when Geometry Dash exposes a transient zero-ID non-editor level. It also remembers fingerprint and level-name/creator aliases, can recover a unique server ID from existing Pulse files, and merges strong split-library duplicates back into one canonical slot while preserving replaced takes as backups. Editor copies remain isolated.

## v1.4.8 stable libraries, timing filters, and UI cleanup

This release fixes the "same level, different empty macro library" problem. Downloaded/online/official levels now use their positive Geometry Dash level ID as the persistent Pulse library key instead of the raw in-memory `m_levelString` hash. On first access, Pulse scans legacy per-hash files for that slot, keeps the newest valid take, migrates it to the stable key, and retires duplicate legacy variants instead of silently splitting future recordings. Local/non-positive-ID editor levels remain content-keyed so unrelated local data is not merged.

Frame Windows now separates **replay transitions** from **timings worth measuring**. Every press is replayed and measured. Releases are always replayed for deterministic physics, but only ship, wave, and robot releases receive their own frame window. Cube, ball, UFO, spider, and swing releases are replay-only and are excluded from analysis progress, histograms, circles, precision calculations, and input-aware checkpoint planning. The default local metric now ends at the next meaningful timing for that player rather than the next raw transition. Existing references load, but old widths are invalidated by analysis version 5 and should be re-analyzed so player modes can be learned on the opening baseline.

The normal HUD defaults to a compact transparent NaN-style counter: grouped bins (`9-12`, `7-8`, `5-6`, `4`, `3`, `2`, `1`), a closer purple/blue/green/white/yellow/orange/red palette, and no extra title/percent/footer unless **Minimal counter (NaN-style)** is turned off. Analysis still shows progress. Pulse and Frame Windows popups now use clearer numbered actions and built-in **Help / How to use** guides.

The checkpoint/search behavior is otherwise unchanged: each side stops at the first confirmed failed boundary, failed boundaries receive one confirmation replay, candidate-aware native checkpoints are verified before use, and bad snapshots fall back safely. All six standalone CTest suites pass; Win64/native hook behavior still needs the included GitHub Actions build and in-game testing.

## v1.4.6 deterministic playback auto-repair

Pulse can recover from a reproducible macro playback death without blindly nudging one click and hoping. When normal playback dies and **Auto-repair deterministic playback deaths** is enabled, Pulse first replays the original timing from a clean seed. A non-reproducible death is left untouched. If the same route still fails, Pulse searches nearby 240 Hz frames on the most recent jump transition, then progressively earlier recent transitions if needed.

Every repair candidate is replayed through the **entire remaining macro**, so a frame is not accepted merely because it survives the obstacle that killed the original route. Pulse chooses the center of the widest contiguous passing region and performs one final confirmation replay. The confirmed trial becomes the new recorded trajectory, saving is atomic, and the previous macro is retained as `.bak`.

## v1.4.5 final-validation preservation fix

If every input window has already been measured, a failure in the post-analysis final baseline no longer discards the entire run. Completed windows are saved with a prominent **FINAL VALIDATION WARNING** and are explicitly treated as unverified/potentially inaccurate. Opening-baseline failures and failures during actual measurement remain fatal. This directly fixes the 202-input Cobwebs case where input 201 +5f was conservatively marked unstable and the very last final replay then erased all new measurements.

## v1.4.3 candidate-aware checkpoint acceleration

Frame-window search already stops each direction at its first failed boundary: after the confirmed negative/early failure it switches to +frames, and after the confirmed positive/late failure it immediately advances to the next input. v1.4.3 keeps that behavior and adds regression coverage so the analyzer does not waste trials beyond a failed side. Failed boundaries still get one deterministic confirmation replay; unstable confirmations remain conservative and nonfatal.

Checkpoint restores are now selected against the **exact shifted candidate** instead of always using the worst-case -20f boundary. Positive trials and the common -1f trial can restore much closer to the click. The input-aware plan prioritizes a universal safe anchor (`T - 20 - 12`) for correctness, then spends spare cache slots on closer tiers (`T - 1/-4/-8/-12/-16 - 12`). The default cache limit is now 256 hidden native checkpoints, configurable down to 32 if a very large level uses too much memory.

Dense checkpoints now record overlapping 60-tick validation suffixes correctly. Earlier code only appended validation samples to the newest checkpoint, which could falsely invalidate checkpoints spaced less than 60 ticks apart. Every checkpoint still verifies an unmodified suffix before first use; bad restores fall back to a safe earlier checkpoint or the level start. This changes replay work only, not success/failure semantics.

Standalone/core validation passes all six CTest suites. A fresh Win64 `.geode` still needs the GitHub Actions workflow or another Windows Geode 5.10.1 toolchain; no live GD run is claimed here.

## v1.4.1 checkpoint analyzer continuation

This continuation finishes and hardens the v1.4 native-checkpoint acceleration path. Candidate trials now explicitly select the native checkpoint before `resetLevel`, full-restart fallbacks clear any private analysis checkpoint first, and cleanup clears analyzer checkpoint state before returning to normal gameplay. Fresh installs use a 180-tick (0.75 s) checkpoint cadence, automatically widened on long levels to stay under 128 snapshots. The HUD footer now shows the current input/offset, checkpoint origin, elapsed time, and a rough ETA after a few measured inputs.

The optimization changes **where simulation starts**, not what counts as success: shifted inputs still run through natural GD physics, deaths still fail, checkpoint restores are validated before use, and bad checkpoints fall back to an earlier usable snapshot or the level start. Unstable failure double-checks still continue conservatively with warnings.

Standalone/core validation passes all six suites, including checkpoint selection/held-input/overlay regressions. A live Win64 Geode build cannot be produced in the current Linux sandbox because the Geode Windows cross-toolchain/SDK is not installed, so this source includes an official GitHub Actions Win64 build workflow (`.github/workflows/build-win64.yml`).

## v1.4.0 hidden checkpoint cache

The analyzer can cache hidden native Geometry Dash checkpoints during the opening baseline and restore the nearest safe checkpoint before each shifted input. This avoids replaying a long level from 0% for every candidate. Each checkpoint stores analyzer-side input/pose cursors and held-button state, is validated with an unmodified suffix before first use, and falls back safely if restoration is not deterministic.

## v1.3.13 turbo analyzer

Failed-boundary double-checks and the final baseline are still performed, but they no longer run at one physics update per rendered frame. All analysis phases use batched fixed 240 Hz updates, which is dramatically faster on long macros. New-install defaults are 64 ticks/render frame and an 8 ms time budget. Existing settings are preserved on upgrade, so set **Analysis ticks per rendered frame = 64** and **Analysis time budget = 8 ms** (or 128 / 12 ms for maximum speed) after upgrading. This is a throughput change only; shifted inputs still replay naturally and unstable results still produce warnings.

## v1.3.12 frame-window uncertainty behavior

If a shifted boundary fails once but its slow double-check disagrees, Pulse now continues conservatively instead of aborting the whole analysis. It treats that boundary as failed, marks the analysis uncertain, and warns at completion that results COULD be inaccurate. Global scheduler/TPS/baseline errors remain fatal.

# Pulse Macro 1.3.5

One Windows x64 mod containing the macro library and built-in Frame Windows. Target: GD 2.2081 and Geode 5.10.1.

## Active-player drift checks in 1.3.5

Macro playback now checks player 2 only while native dual mode is active. An inactive player can retain or reset a stale position, which must not stop a single-player replay. Player 1 remains checked, and both players remain checked in dual mode.

Position errors now include the physics step, P1/P2 drift distances for active players, and the effective limit. The allowance is Euclidean distance from the recorded position, not a separate allowance on each axis; older descriptions incorrectly said per-axis. Five units remains the default relaxed limit.

The inspected Cobwebs run loaded 1.3.4 and stopped with a position error, not a timing error. Its tape resets inactive-looking P2 coordinates to zero at step 5991 (24.9625 seconds), near the reported late-run stop. This identifies a plausible false-stop path but does not establish which player caused the actual failure, because the older error omitted that detail. No live replay was performed. The 480 FPS display alone does not establish an FPS/TPS conflict.

## Small playback drift in 1.3.4

**Allow small position drift** is on by default. Macro playback continues when either player's position is within 5 game units in distance of the recording, instead of stopping at the former 1-unit default. If your Position tolerance is larger, that larger value is used. Turn the new option off to use Position tolerance directly.

Inputs keep their recorded order and timing; positions are not snapped or corrected. Larger drift, incompatible physics timing, actual death, and the saved endpoint still stop playback. Frame-window analysis retains its independent strict validation; this setting affects macro playback only. Existing recordings can be reused without re-analysis.

## Wave direction windows in 1.3.3

For **wave**, the default now measures survival until the next press OR release for that player: the next direction change. For other modes it uses the next press. The endpoint is immediately before that next action is delivered, with other inputs unchanged. The final action uses the recording endpoint. Player mode is captured at recording and checked during baseline replay; existing reference files learn mode during that replay.

This matches getting through the click and reaching the next one. The previous full-route test could report an easy early click as 1f because moving it caused a much later death with the untouched suffix. The new mode does not count failures beyond the next-click endpoint against that input. It does not promise the rest of the macro will work from the altered state.

The counter identifies **TO NEXT ACTION**; wave details say **NEXT DIRECTION CHANGE**. Enable **Require survival through the entire macro** in settings only if you want the previous full-route question. Re-analyze after changing scope. Full-run precision scores are hidden for next-click measurements because these local results do not establish whole-run survival.

The specific first-wave regression was a press at tick 268, release at 400, and next press at 436. Moving the press one tick earlier died at 412. Version 1.3.2 incorrectly treated that post-release death as a failure of the 268-to-400 wave segment. Moving one tick later died at 295, which remains inside the segment and must still constrain the result. This fix changes the endpoint; it does not assign an expected 3f or 4f value. Re-analysis determines the width.
The game still simulates fixed 240 Hz steps. Analysis batches many updates per rendered frame, including failed-boundary confirmations and the final baseline. An unstable confirmation no longer aborts the entire run: the boundary is treated conservatively as failed and marked with a warning. Baseline/scheduler/TPS failures remain fatal because they can invalidate every window.

**Restart GD, then F4 → Analyze again.** Existing reference inputs can be reused. Old widths are displayed as unknown until measured using the new checks. Input details and CSV now include the endpoint and failed early/late boundary ticks. Analysis may take longer while confirming failures; P still pauses/resumes.

## Fixes in 1.3.1

Pausing no longer saves or ends either recording. Temporary pause-scene exit callbacks preserve the current take, checkpoints, and frame reference. Resume continues that same recording; the paused time is not recorded. Explicit F8/Save still finishes the take, and actually quitting the level still attempts a save.

Use the normal pause controls or **P** to pause/resume. Recording and analysis update loops stop while paused. During analysis, **F4 → Resume analysis** continues the current search. Saving while already paused leaves the game paused; choose F4 → Analyze when ready instead of being automatically resumed.

Reopening a level and opening F4/Analyze now loads the selected macro slot's saved reference, using the same filename as recording. The old lookup mistakenly looked for a standalone F5 reference. Existing valid schema-2 references from 1.3.0 can load without re-recording; macro replacement/deletion and other-mod version checks still apply.

## Practice and speedhack workflow

1. Open a classic level. Enable practice and set Mega Hack's desired speed before recording.
2. Press **F6**. This restarts from zero and records both the macro and frame reference.
3. Create checkpoints after F6. Dying keeps recording active. Checkpoint restores remove the failed branch from both timelines. A restart without a checkpoint begins a fresh attempt from zero while recording stays on.
4. Press **F8**, use **Pause → Pulse → Save Now**, or complete the level to save. Leaving the level also attempts to save the macro as a precaution.
5. Saving automatically starts frame analysis by default. Disable **Analyze after Pulse saves** in Pulse settings while building a longer route if you prefer manual analysis.
6. Leave practice mode and press **F7** to replay the saved macro from zero. You can return speedhack to 1x first. Keep physics/TPS settings unchanged.

Death alone no longer saves or stops a linked recording. Saving after death but before checkpoint restore can still save that failed branch; save a surviving route for analysis.

A **segment** means the recording ends before completion. Playback pauses at its saved endpoint. **Record / Replace** replaces the selected slot when saved; it does not append to an existing macro. If starting from the pause panel, close it and resume to advance.

## Controls and library

| Control | Action |
| --- | --- |
| P | Pause/resume without ending the take. |
| F6 | Start both recordings; press again to stop and save. |
| F7 | Play selected saved macro; press again to stop. |
| F8 | Stop playback or save recording. |
| Pause → Pulse | Names, 20 slots per level, Record / Replace, Save Now, Play, Delete, Undo Delete, Files, Frame Windows, and Help. |
| F4 / Frame Windows | Analyze, Cancel analysis, Input details, Export CSV, Settings. |
| F3 | Toggle the counter. Analysis progress remains visible. |
| F5 | Optional reference-only recording in normal mode. Use F6 for practice/combined recording. |

The library displays saved duration, input count, full-run/segment status, and save time separately from the active take. Name saves a per-level/slot label. Save stops and writes the active take and edited name. Delete archives the file; Undo Delete restores it only if the slot is empty. Replacement saves keep a `.bak` copy. Names are labels, not filenames. An active take's slot stays fixed. Positive online level IDs use one stable library across transient GD level-string variants; legacy split libraries are migrated per slot on first access.

## Viewing frame windows

Let automatic analysis finish, or use **F4 → Analyze**. The opening baseline runs from zero and creates hidden analyzer-owned native checkpoints about every 0.75 seconds by default. Each shifted-input trial restores the latest safe verified checkpoint before that input, then simulates only the remaining suffix; if a checkpoint cannot be reproduced, Pulse falls back to an earlier usable checkpoint or the level start. The checkpoints are internal (no practice diamonds/sounds), and your original practice checkpoint array is restored when analysis stops. Your saved macro stays intact.

Progress shows validation or the current input/offset. F4 opens cancellation controls. Success shows a notification; failure shows its reason in a dialog. Analysis finishes paused.

Use **F4 → Input details** to browse results immediately without replaying. **3f [-1,+1]** means three valid tick positions: one earlier, original, and one later. At 240 Hz its nominal width is 12.50 ms. Details include player, press/release, original tick, earliest/latest tick, and nominal milliseconds. These are game-time widths, not slowed wall-clock durations.

To see the moving counter and numbered circles, leave practice and press F7. F3 toggles the HUD. **?** means unmeasured; **21+** is a lower bound. CSV includes early/late offsets, tested bounds, nominal milliseconds, and lower-bound flags.

Each result is the contiguous valid interval containing the original timing with all other replay transitions fixed. By default, a trial survives through the next **meaningful timing** for that player. Presses are timings in every classic mode. Ship/wave/robot releases are also timings; cube/ball/UFO/spider/swing releases remain in the replay but are skipped as standalone windows. The last timing uses the reference endpoint. Optional full-macro mode uses the reference endpoint for every timing. Disconnected alternatives and joint multi-input timing matrices are not measured.

The analyzer uses natural game physics with no position corrections. It checks sampled positions and vertical velocities before and after the trial suite. A failed baseline rejects new measurements. Analysis uses test mode and suppresses its own completion/stat side effects.

## Old analyzer failure

The inspected log reported a baseline mismatch at tick 720. Its reference used GD's progress counter: one observed macro had 1,066 physics steps while the reference reported 2,132 ticks.

Version 1.3 counts intercepted physics steps and receives the exact inputs recorded by Pulse. Checkpoints rewind both clocks and inputs. Baseline errors are now displayed directly. This fixes an identified defect; other desync causes remain possible.

**When upgrading from the separate mods (before 1.3.0), record a fresh F6 reference.** Old frame results used a different clock/schema and are not reused. Existing macro files, names, and the old standalone save folder are retained.

## Compatibility

- **Mega Hack speedhack:** new recordings default to fixed 1/240-second game updates. Incoming speed-scaled time controls how often steps run. Simulated playback at changing speed passes tests; actual Mega Hack speedhack in this revision remains unverified live.
- **FPS bypass:** render FPS does not define macro ticks. Keep physics at 240 Hz for analysis. The scheduler and FPS cap are not replaced.
- **Other modifiers:** disable other replay bots, Lock Delta, frame extrapolation, and TPS/physics bypass for this workflow. Modifiers inside another mod's physics calculations may still conflict.
- **CBF:** enable Disable CBF and disable Physics Bypass. Subtick analysis is not implemented. Native CBS/CoS are temporarily disabled for active sessions and restored afterward.
- **Practice:** checkpoints must be created after F6. GD restores native state; Pulse stitches the recording timeline. The final route still has to reproduce naturally from zero. Native checkpoint restore timing remains unverified live.
- **Platformer:** Pulse records directional inputs; frame analysis supports classic levels only. Platformer and dual gameplay remain unverified live.
- **Rated levels:** no rating restriction. Normal input/completion paths are used; GD and other mods decide credit. Pulse does not forge statistics or make automation a human run.
- **Randomness:** recorded seeds are reused, but other random sources or persistent level state can cause drift. Playback checks position/timing without correcting motion.
- **Old macros:** v1 files retain original timing; v2 records its fixed-update mode. Re-record for new timing behavior. StartPos recording is unsupported.

## Installation and files

Install only **alan.pulse_macro.geode** in geode/mods and restart GD. Remove the old standalone **alan.frame_window_lab.geode** package to avoid duplicate hooks.

Both components' settings now live under **Pulse Macro**. Old standalone display settings are not migrated. Macro files are in the alan.pulse_macro save directory under **macros**; new references/CSV are under **frame-windows**. Existing saves are preserved.

## Build and verification

Use Visual Studio 2022 C++ tools, CMake 3.25+, Geode CLI, SDK 5.10.1, and bindings commit **7f6c2a75742856de88dad354e576dcff8a28e881**. Set GEODE_SDK to the SDK checkout. Build from this root; FrameWindows is an internal component.

~~~powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DGEODE_BINDINGS_REPO_PATH="C:/path/to/bindings" -DPULSE_INTEGRATION_TEST=OFF
cmake --build build --config Release --parallel 6
ctest --test-dir build -C Release --output-on-failure
~~~

For tests without Geode, configure with **-DPULSE_CORE_ONLY=ON**.

Six suites pass: replay core, speed/FPS timing, actual-file library operations, frame-window search, checkpoint planning/restoration metadata, and combined practice timeline simulation. The practice test removes 100 failed branches, compares both timelines, and replays the stitched route under changing speed.

The distributed binary has the startup integration harness disabled. **No live UI/game testing was performed for 1.3.5**, as requested. Tests validate reusable algorithms and compilation, not native hook ordering, actual Mega Hack compatibility, popup layout, or rated-level credit. An earlier v1.0.0 local-level live test does not validate these changes.








## 1.3.7 analyzer hotfix

Frame-window validation now tolerates up to 0.1 GD units of harmless pre-shift floating-point drift instead of 0.001. This remains far below one block (30 GD units) and does not apply position correction or change candidate pass/fail physics. New-install analysis defaults are also reduced to 8 simulated ticks / 2 ms per rendered frame to avoid intentionally consuming most of the render-frame budget. Existing saved settings may need to be changed manually.

## 1.3.11 candidate-reset hotfix

A live log showed the analyzer successfully reached input 1, tested `-1f`, and naturally failed at tick 91. The next candidate reset then aborted on the sampled pose at tick 60. That proves the old error was happening between candidate resets, not before candidate testing.

Candidate Early/Late trials now use their own pre-shift reset trajectory for the pose guard. No player position or velocity is corrected. Candidate success is still based on natural simulation after the shifted input, failed boundaries are repeated for consistency, the initial baseline must survive, and the final baseline remains strict. This is narrower than disabling validation globally and specifically avoids treating reset-to-reset hook-stack state as a fake input failure.
