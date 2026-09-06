# Frame Windows audit — Pulse Macro 1.4.9

Source reviewed: the supplied `PulseMacro-source-v1.4.8.zip`. The archive's old validation notes are historical evidence, not instructions or proof that this revision was tested in GD. This update targets GD 2.2081 / Geode 5.10.1 / Win64. No application control, mod installation, or live Geometry Dash testing was performed.

## Findings that can explain disagreement

1. **Event ordering imposed artificial failures.** Version 1.4.8's analyzer `canShift()` rejected meeting or crossing any neighboring same-player transition, including a replay-only cube release. Such a failure did not come from GD physics. Version 1.4.9 permits these candidates and gives ties the original source order. Auto-repair's separate ordering restrictions remain unchanged.
2. **The local endpoint stopped before the next input.** Step `T` has not executed when the simulation clock first reaches `T`. A candidate could reach that boundary in a trajectory that fails as soon as the next control is applied. The new endpoint executes that input group and a settling suffix.
3. **Validation rewrote its own evidence.** Opening and Early/Late trials assigned `expected = actual` before comparison. That effectively accepted whatever reset state those trials produced; the final check then compared against mutated data. Recorded samples now remain immutable. Candidates also compare their entire observed pre-shift trajectory against dense samples of the successful opening replay.
4. **Completed references could pass without completing.** The old runtime ignored its existing `needsCompletion()` helper. A completed reference now requires the completion callback or native completion/end-sequence state. Merely reaching a tick count passes only a surviving-prefix reference or a local endpoint before the level end. Death takes precedence. An undelivered shifted target cannot pass through an early completion callback.
5. **Checkpoint input state was incomplete.** Reset restored analyzer jump booleans but cleared queued commands and only assigned one native button-map entry. Checkpoints now retain the native queued commands and both players' complete holding maps, plus analyzer cursors/latches and both GD seeds. Native queues are no longer erased on every playback update.
6. **Warnings could disappear on reload.** Final-validation warnings were written to `last-analysis.json`, not the reusable reference. Warning flags/messages now survive reference save/load, CSV export, and the minimal HUD. Precision is hidden for unverified measurements. Repeated final-error callbacks cannot turn an already preserved final warning back into data loss.

These are methodological defects, not evidence that a particular Cobwebs bin should change by a specified amount. No Cobwebs macro/reference or executable gameplay was used to fit results. The supplied histogram alone cannot isolate individual failures or establish identical endpoint, input-selection, level-version, or physics conditions.

## Exact frame-window definition

Inputs are indexed by their immutable order in the reference. One analysis trial substitutes the tick of exactly one event. All other events, including every excluded release, retain their original ticks and relative order. Inputs are processed before their numbered step's physics; after step `T`, the clock is `T+1`.

After a fatal opening baseline check establishes that the original route passes, search `-1,-2,...` until the first failed boundary, then independently `+1,+2,...` until its first failed boundary. Do not test farther beyond a failure. Each failed simulated boundary receives one full-start confirmation. A conflicting outcome or failure tick retains the failure and marks uncertainty. Ordinary successful candidates are not repeated unless Strict validation is enabled.

For passing offsets **-3,-2,-1,0,+1,+2**:

- `early = 3`, `late = 2`.
- `width() = early + late + 1 = 6` valid discrete timing positions.
- `spanTicks() = early + late = 5` ticks between the earliest and latest positions.
- One tick is `1000/240 = 4.166666… ms`.
- Nominal six-bin duration is 25 ms. The earliest-to-latest timestamp span is 20.833333… ms. Neither calculation measures a continuous/subtick window.

Search is capped at **20 offsets on each side**. Both sides are still searched even when the first side already proves the width exceeds the displayed 20f range. A side still passing at the cap is *censored*: the exported interval is a tested lower bound, and its unknown failed boundary is blank. `overflow` identifies the histogram's `>20` category, independently of whether both actual boundaries were found. Circles/details show the tested count with `+` only for censoring; exact wider counts remain exact. JSON, CSV, histogram and precision use the same inclusive count.

Bounds outside `[0,endTick)` are explicit reference-domain limits with no fabricated death tick. An original timing need not receive an extra per-target zero-offset replay: the successful unmodified opening run already includes every original event and reaches every selected local endpoint.

## Success endpoint and its tradeoffs

Default local policy: find the next **strictly later meaningful input tick on either active player**. Same-tick P1/P2 events form one group. Excluded releases are executed but cannot anchor the endpoint. Sharing the anchor across players reflects the shared world/dual physics rather than assuming independent worlds.

Execute that group and **4 additional whole physics steps**. The exclusive endpoint is:

```
min(reference.endTick, max(nextMeaningfulTick, shiftedTargetTick) + 1 + settlingTicks)
```

The final meaningful input uses the reference endpoint. A candidate moved past the original anchor extends the endpoint so the shifted event itself executes and receives the settling suffix. Each attempt exports its actual endpoint; the per-input summary exports the zero-offset endpoint. The last prefix may truncate the requested suffix, which limits what that measurement establishes.

`Next action settling steps` is configurable from 0 to 240 and persisted. Zero still executes the anchor input's step. Four is an explicit initial policy choice, not an empirically proven universal GD horizon. It adds 16.666667 ms **after** the anchor step. It was not selected to match NaN or Cobwebs.

The synthetic tests compare three definitions:

| Synthetic case | Reach/execute next input only | Execute next input + 4 steps | Full reference |
|---|---:|---:|---:|
| An altered trajectory dies two steps after the next input | Incorrectly accepts the tested shift | Rejects the shift | Rejects the shift |
| Nearby obstacle passes, but unchanged suffix dies at tick 400, far beyond the local anchor at 100 | Accepts the local shift | Accepts the local shift | Narrows the early window |

Demanding the original pose after a changed input was deliberately not adopted: an alternate, naturally surviving trajectory may have a different position or velocity. Pose validation is confined to the unchanged prefix and developer reproducibility checks; it never steers physics.

A finite suffix cannot prove that the next input is usable indefinitely. It may miss delayed consequences beyond the horizon; a longer horizon may include consequences the user wants to attribute to later controls. The local metric is therefore **natural survival under a documented finite endpoint**, not proof of eventual route completion. Full-macro mode remains available. A real-level horizon study (for example 0/2/4/8/16 steps on the identical reference) is still required before calling any horizon the most representative gameplay definition.

## Presses, releases and event order

Every recorded transition remains in replay. Active-player presses are targets. Ship, wave and robot releases are targets when relevant releases are enabled. Cube, ball, UFO, spider and swing releases are replay-only. Swing follows the requested default; this audit does not claim to have reverse-engineered the entire proprietary release implementation. Unknown legacy modes are learned during the opening replay.

Mode and active-dual status are sampled at each original input's delivery boundary, before that tick's physics. A portal processed during step `T` affects subsequent input boundaries, including step `T+1`; same-tick events retain actual delivery order. Release classification uses the release's own mode, not the mode at its preceding press. Inactive P2 events remain in replay but are excluded as targets/endpoints and from player-state validation. Native dual activation makes P2 relevant again.

Ordering is lexicographic `(effective tick, original event index)`. The candidate overlay performs a stable merge against the immutable input vector, without copying or sorting it per trial. A press tied with its later release precedes that release; a release tied with a later press precedes that press. Crossing another event changes only the moved event's position in this order. Natural duplicate/latch behavior is left to GD; crossing is never automatically made successful. New linked references retain the actual source macro event index, including gaps for non-jump events. Legacy source indexes identify the frame-reference event sequence.

## Determinism and checkpoint audit

| State/path | Handling in this revision | Remaining limit |
|---|---|---|
| Level start | Full-start reset, no private current checkpoint, consistent input mode and released buttons; compare subsequent start poses to opening start pose | Native reset and arbitrary third-party global state cannot be proven complete by bindings |
| RNG | Capture/restore independent `m_randomSeed` and `m_replayRandSeed`; compare both in new recorded and dense canonical poses | Old references lack separate replay seed; other mods' RNG is not captured |
| Queued and held input | Preserve native checkpoint queue, both complete holding maps, analyzer input cursor and held latches; no synthetic checkpoint press | External code can maintain private input queues |
| clickBetweenSteps / clickOnSteps | Normalize analyzer session to whole-step input, restore user values at exit, reject changes while measuring | Other mods may bypass these fields |
| P1/P2, dual and modes | Position/Y velocity plus mode, dual, gravity direction, mini scale, speed, reverse motion, on-ground/slope, dash, buffer/orb/pad/portal and control-lock flags | Public fields do not describe every native player bit |
| Dormant P2 | Exclude its pose/flags and attempted death callbacks while dual is inactive; preserve its replay events | Dual portals must still be validated in GD |
| Rotation | No visual rotation comparison or correction | No claim that every modded collision mode treats rotation as cosmetic |
| Object/trigger/EffectManager state | Native `CheckpointObject` via reset/load, not a handwritten player-only snapshot | Bindings expose saved object, active/special-object, sequence and EffectManager structures; native copy/restore implementation is not fully public |
| Moving objects, slopes, collisions, teleports | Retain native snapshots; verify immediate restored boundary and 60-step unmodified suffix; compare all pre-shift canonical poses thereafter | A latent object difference can first matter after this guard; A/B/Strict testing is needed |
| Camera-independent physics | Natural engine updates; no position/velocity writes during trials | Third-party camera hooks or render-scheduled triggers may affect physics |
| Practice/test/checkpoints | Isolate native user checkpoint array and current pointer; align selected private checkpoint and array before reset; restore original array/pointer/settings on exit | Native practice restoration can omit mod-owned state |
| Scheduler/update order | Fixed 1/240-second updates on main thread; reject subticks, wrong step delta, multiple physics steps per update, missed input ticks and stalled replay | Native/hook ordering and render-scheduled work require live testing |
| CBF/TPS/speedhack | Existing CBF compatibility checks retained; reject detected physics bypass/subticks/TPS changes; batch fixed updates instead of scaling physics delta | Mod version fingerprint does not identify every private setting or multiplier |
| Pause/resume | Existing pause lifecycle retained; analysis does not step while paused | User settings or other mods can change while paused; ensuing detectable mismatches remain guarded |
| Death/completion | Latch attempted real death as failure; suppress side-effect callbacks during analysis; inspect native terminal signals | Callback suppression by other hooks may produce a warning/fatal baseline rather than a usable result |

Checkpoint `C` must satisfy `C <= min(originalTick, shiftedTick) - 12`; even a zero safety argument is clamped to at least one tick. Cursors point to the first unconsumed input and pose. Each selected checkpoint gets an immediate boundary comparison and an unmodified suffix validation before candidate use. Questionable points become unusable; selection retries an earlier point or level start **without submitting a candidate outcome to Search**. A rejected restore never receives a physics update.

Cached candidate failures are confirmed from level start. A conflicting confirmation conservatively narrows the window, invalidates the implicated checkpoint and marks the result unverified; it is never silently presented as an exact boundary. Previously accepted candidates are not retroactively proved correct by a later fallback. This is why the complete A/B comparison exists.

Opening mismatch is fatal and preserves the previous saved/in-memory reference. Candidate-only pre-shift mismatch from level start produces a conservative uncertain boundary and one confirmation. Global scheduler incompatibility remains fatal during measurement. Final validation after all targets are measured saves the measurements with a persistent warning. Cancel/fatal incomplete analysis restores the previous in-memory reference instead of leaving partially learned metadata mixed with old widths.

Validation tolerances: existing legacy position allowance **0.1 GD units per coordinate** retained; Y velocity, reverse speed, scale, speed and gravity scalars use **1e-6**; discrete flags/modes/dual/seeds compare exactly. Tests cover small noise, meaningful mismatch and nonfinite values. These are conservative guard policies, not tolerances calibrated across live GD hook stacks. They do not alter any physics state. References recorded with richer metadata get stronger opening checks; legacy sparse poses cannot retrospectively supply that metadata.

## Developer checks and exports

- **Force full restart analysis:** no hidden checkpoints created or used.
- **Compare checkpoints versus full restart:** first run accelerated, second run from level start against the same in-memory input/reference/settings; export both and diff every event/window. Overrides Force full restart for pass one.
- **Strict validation:** additionally replay passing `-1/+1` candidates from level start and compare outcome, failure tick and terminal pose. Ordinary failed boundaries already get one full-start confirmation. Disagreements cannot widen the measured window.

Files are saved under the mod's `frame-windows` folder:

| File | Contents |
|---|---|
| `<key>.fwl.json` | Reference, analysis version 6, inclusive widths, censoring, warnings, modes, endpoint policy, trial records and metrics |
| `<key>.csv` | Every source event; analyzed index, source index, tick, native percentage when known, action/player/mode, passing offsets, failed boundaries/ticks/reasons, checkpoint summary, restarts/fallbacks, uncertainty, updates and endpoint |
| `<key>.trials.csv` | Each actual trial/verification/confirmation, including checkpoint index/tick, true full-start indicator, actual candidate endpoint, updates and outcome/reason |
| `<key>.histogram.csv` | Exact 1–20 bins, grouped bins, >20, unknown, presses/releases and total targets/raw events |
| `<key>.accelerated.*`, `<key>.full-restart.*` | Separately retained A/B evidence using the same schemas |
| `<key>.restart-diff.csv` | Every A/B field disagreement, not just aggregate histogram differences |

Summary checkpoint fields say `multiple` when several checkpoints contributed. Use the trial file for exact attempt-by-attempt origins and candidate-dependent endpoints. A failed boundary offset and a death tick are distinct columns; blank death ticks do not invent a collision at a timeline bound. Input indexes are one-based analyzed ordinals; source-event indexes are zero-based. JSON's trial `sourceIndex` addresses the frame-reference vector; each corresponding input stores its original macro index.

A/B differences are labeled an equivalence bug; full-restart results are retained with warnings. Matching widths with validation warnings do not receive a success claim. If pass two cannot finish, pass one's already measured results are retained as unverified, with the comparison failure explained. Neither result is installed or sent anywhere by this work.

`python tools/compare_windows.py accelerated.csv full-restart.csv --output diff.csv` provides an offline comparison. Its exit code is nonzero for differences or unverified input. Other counters need a deliberate schema/convention adapter; the tool does not guess NaN event mappings.

## Public NaN comparison

NaN GD's own [NaNDL FAQ and formula](https://nandl.pages.dev/) describe windows as the number of available ticks, and convert that count to nominal time using `N/f`. This agrees with Pulse's inclusive discrete-position convention. There is no documented justification here for a universal one-frame adjustment. The page also states the symmetric Gaussian/independence assumptions behind its BASE precision model.

No public implementation or sufficiently specific endpoint, release, ordering or checkpoint specification was located in this search. The publicly described convention does not prove how an individual video was measured. No proprietary code was copied. Relevant native state declarations were inspected in [Geode's GD 2.2081 bindings](https://github.com/geode-sdk/bindings/blob/7f6c2a75742856de88dad354e576dcff8a28e881/bindings/2.2081/GeometryDash.bro); those declarations are evidence of available state, not proof of engine checkpoint equivalence.

## Validation and performance

See `VALIDATION.txt` for final build/test evidence. Tests cover all 24 requested scenarios, plus randomized overlay equivalence, pose/seed guards, duplicate final-error callbacks, CSV escaping, histogram/precision consistency, and offline diff failures. Synthetic checkpoint and fallback runs match full-start results exactly.

Synthetic fixture updates: full start **1,268,415**, accelerated **59,660**, unusable-checkpoint fallback **61,495**. The accelerated path used about **95.3% fewer** fixture updates. This is a comparison of restore strategies in a toy deterministic world; it is neither a v1.4.8-versus-v1.4.9 GD benchmark nor evidence of a 21× in-game speedup. The before/after standalone suites contain different workloads, so their wall times are not a meaningful analyzer benchmark.

Runtime tracks physics steps separately from update calls, checkpoint restores, full starts, fallbacks, candidates, pass/fail counts, confirmations and elapsed wall time. Per-trial steps exclude all work before the restore boundary. Search no longer rejects crossings without simulation; it also searches both capped sides and adds a settling suffix. Those correctness changes, dense validation and full-start boundary confirmations may increase runtime versus 1.4.8. Ignored targets, immutable overlay merging and candidate-safe checkpoints reduce work. Exports aggregate in one pass rather than scanning every trial for every input.

Memory includes one dense canonical `Pose` per reference tick, bounded native checkpoint count (maximum 256), and detailed trial records. Long references therefore cost more memory. Compact JSON is limited to 256 MiB; oversized output is rejected before replacing an existing reference. No background-thread GD physics is used.

## Remaining accuracy limits

The native binary compiles, but engine restore fidelity, moving/trigger object behavior, actual dual/portal edge cases, arbitrary mods, CBF interactions, callback timing and tolerance suitability have **not** been verified in live GD. The synthetic harness cannot establish those facts. Neither matching synthetic windows nor matching Cobwebs histogram totals would establish universal accuracy. The cause of each supplied Cobwebs discrepancy remains unconfirmed until the same reference is run through the new trial exports and A/B comparison.
