# Pulse Macro v1.4.9

240 Hz macro recording/playback, practice recording, a 20-slot macro library, playback auto-repair, and built-in Frame Windows for GD 2.2081 / Geode 5.10.1 on Win64.

Frame Windows now validates immutable baseline states, simulates same-tick/crossing events, restores queued and held checkpoint input state, and keeps uncertainty warnings after reload. The default local endpoint executes the next meaningful input and a configurable settling suffix. Inclusive width counts valid discrete tick positions, including the original timing.

Every release remains in replay. Ship/wave/robot releases are separate timing targets; cube/ball/UFO/spider/swing releases are replay-only. Input mode and active dual state are learned at each input boundary.

Developer settings provide forced full-start replay, selected strict validation, and automatic accelerated/full-restart comparisons. Export includes per-input windows, per-trial evidence, histogram totals and comparison differences. Completed references require a native completion signal; final validation failure preserves measurements with a warning.

F6 records, F8 saves, F7 plays, F4 opens Frame Windows, F3 toggles the HUD, P pauses/resumes. Existing macro playback, storage, UI and auto-repair remain available.

Compiled and standalone-tested; not live-tested in Geometry Dash. See the source package's FRAME_WINDOWS_AUDIT.md for endpoint policy, evidence and remaining limits.
