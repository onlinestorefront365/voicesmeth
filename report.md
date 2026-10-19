# Voicesmith – Repo Review & Tooling Report

Scope: static read-through of all 109 tracked files (Kotlin UI/service layer, C++ audio engine, Gradle/CMake, CI, Makefile). Nothing was built or run: the sandbox has no Android SDK/NDK, so every finding below comes from reading the code, not from a failing build or test.

Severity: **High** = crash/UB/wrong behaviour reachable in normal use · **Medium** = real defect with limited trigger · **Low** = hygiene.

---

## 0. Fix status (update)

All findings below were addressed in a follow-up commit except where noted. Verified: host-side unit tests (new, `tests/native`, ASan+UBSan) pass; the C++ files `AudioPipeline`, `AudioStream`, `AudioSource`, `TestAudioPlugin`, `AudioPluginFactory` pass a clang `-fsyntax-only -Wall -Werror` check against the real Oboe/eventpp/stftPitchShift headers. **Not verified:** the Gradle/Kotlin build, lint, and anything on a device (no Android SDK in the sandbox).

- **Fixed:** H1–H3, M1–M6, M8, L1–L2, L3 (channel importance only), L4–L7, L9–L12, all CI findings except SHA pinning.
- **Not changed:** M7 (Oboe error-callback close; the audio callback uses its `stream` argument rather than `state.stream`, so the race is narrower than first stated), L3 notification wording (author's text), L8 (explicit coroutines dependency; not verifiable offline), L10 `-Werror` (kept), L12 `KEYDNA` name (kept).
- **Caveats:** H1's old behaviour was UB but not reproducible under ASan (reads stayed inside the deque's chunk); the new ring buffer is covered by exact-offset tests instead. CI actions are pinned to major version tags (`@v4`, `@v2`), not commit SHAs, because SHAs could not be verified offline. `-Ofast` became `-O3` (no `-ffast-math`); check performance on a device.
- **Also found while fixing:** `ChainEffect` had a signed/unsigned comparison GCC rejects (fixed); `Noise` used a fixed seed so both stereo channels were identical noise (now seeded from `random_device`); `Oscillator` amplitude drifted over time (now renormalised); `Header.h` lacked `<cstring>`/`<cstdint>`.
- **New:** a signal source selector (VOICE / NOISE / SINE / SWEEP) in the UI, backed by `SelectorEffect`. Noise/sine/sweep run at amplitude 0.25 as a hearing-safety default. Unknown plugin parameters now raise an error instead of being ignored. `tests/native` + `make test` + a `native-tests` CI job, and Dependabot for Actions and Gradle.

---

## 1. Summary

The codebase is small, tidy and consistent. The audio design (Oboe → lock-free SPSC queues → worker thread → effect chain → sink) is sound. The problems are concentrated in three places:

1. **Delay effect reads out of bounds at the slider's maximum (1000 ms).**
2. **Failed start leaves the UI in the "started" state.**
3. **Lifecycle/threading edge cases** (non-atomic flag, lost wake-up, stuck pipeline after failed `open()`).

The project has **no tests, no static analysis, no formatter and no dependency automation**, and its CI uses unpinned `@master` actions while handling signing secrets. That is where added tooling pays off most.

---

## 2. Errors found in the existing code

### High

| # | Where | Problem |
|---|-------|---------|
| H1 | `fx/DelayEffect.cpp` `apply()`, `fx/DelayEffect.h` | `buffer` is sized `params.delay.max * samplerate` (N samples) and `pop_back`/`push_front` keep it at N. At a delay of 1000 ms (UI `max = 1000`) the code computes `delay = clamp(1.0, 0, 1.0) * samplerate = N` and reads `buffer[N]`, **one past the end** of a `std::deque` (no bounds check → UB, garbage or crash). Fix: size the buffer `maxdelay + 1`, or clamp the index to `size()-1`. |
| H2 | `service/AudioServiceActivity.kt` `onStartStopAudioService()` | On start, `it.start()` swallows exceptions and routes them to `onPluginError` → error callback → `onAudioServiceFailed()`. Control then returns and the code **unconditionally calls `onAudioServiceStarted()`**, which sets `state = true`, turns on game mode and vibrates "on". Result: after a failed start the button shows **STOP** although nothing is running. Fix: call `onAudioServiceStarted()` only if `it.isStarted` after `start()`. |
| H3 | `plug/TestAudioPlugin.cpp` `start()` | `state.pipeline = pipe` is assigned **before** `open()`. If `open()` throws (stream open failure, sample-rate/block-size mismatch), `pipeline` stays non-null and every later `start()` returns early at `if (state.pipeline != nullptr)`. The plugin is stuck until `stop()`/destroy. Fix: assign after `open()` succeeds, or reset on exception. |

### Medium

| # | Where | Problem |
|---|-------|---------|
| M1 | `io/AudioPipeline.h` / `.cpp` | `state.loop` is a plain `bool` written by the control thread and read by the worker thread → data race (UB). Use `std::atomic<bool>`. |
| M2 | `io/AudioPipeline.cpp` `onloop()` | `wait_for(lock, 1s)` has **no predicate** and `start()` sets `loop = true` / `notify_all()` without holding the mutex. If the notify lands before the wait, the worker sleeps the full second (start-up latency) or, if `loop` is still false when checked, **exits immediately** and the pipeline never runs. Use a predicate (`wait_for(lock, 1s, [&]{ return state.loop; })`) and set the flag under the mutex. |
| M3 | `fx/VadEffect.cpp` ctor | If `WebRtcVad_Init` fails, `vad = nullptr` is set and then `WebRtcVad_set_mode(vad.get(), …)` is called unconditionally → null dereference. Currently only reachable if `VadEffect` is enabled (it is commented out in `TestAudioPlugin.cpp`). |
| M4 | `etc/Preferences.kt` | Every setter uses `.commit()` (synchronous disk write). The sliders call it on every value change on the **UI thread** → jank and ANR risk on slow storage. Use `.apply()`. |
| M5 | `service/AudioServiceExtensions.kt`, `AudioService.kt` | `START_STICKY` re-creates the service after a process kill, which calls `startForeground(MICROPHONE)` with no UI visible. On Android 14+ a microphone foreground service cannot be started from the background → `ForegroundServiceStartNotAllowedException`. The service also holds no state worth restarting. Use `START_NOT_STICKY`. |
| M6 | `io/AudioStream.cpp` `open()` | `blocksize.get = getFramesPerDataCallback() * channels`. When the device reports `Unspecified (0)` this yields a block size of **0**, creating zero-length FIFO blocks (`fifosize` divides by it as well). The app always passes the system's frames-per-buffer, so it is not hit today, but `AudioFeatures.blocksize` returns `0` when `AudioManager` is unavailable, and nothing validates the value. Add a check after open. |
| M7 | `io/AudioStream.cpp` `onError()` / `close()` | `close()` is run from inside Oboe's error callback, and `state.stream` (a `shared_ptr`) is reset while the audio thread may still be reading it in `onAudioReady` → race on the pointer. Defer the close to a separate thread/control path. |
| M8 | `AudioPipeline::start()` | The worker thread is started before `sink->start()`/`source->start()`; if either throws, the already-spawned thread is left running with `loop == false` and `~AudioPipeline` → `close()` → `stop()` must clean it up. Reorder or wrap in try/catch. |

### Low

| # | Where | Problem |
|---|-------|---------|
| L1 | `voicesmith/build.gradle.kts` | Comment says minSdk should be 27 for Oboe/AAudio (issue #33), but `sdk = intArrayOf(33, 34, 36)` sets **minSdk 33**. Either the comment or the value is stale. Also `targetSdk 34` is below Google Play's current floor (irrelevant for F-Droid, matters if you ever publish). |
| L2 | `AndroidManifest.xml` | No `android:allowBackup`, `dataExtractionRules`, or `configChanges`. Lint flags the first two. Without `configChanges`, rotation recreates `MainActivity`, which tears down the bound service and stops audio. |
| L3 | `service/AudioServiceExtensions.kt` | Notification channel uses `IMPORTANCE_DEFAULT` (audible). A persistent status notification should be `IMPORTANCE_LOW`. Notification text says "*Hopefully* the Voicesmith app is still running…" – likely meant as a joke, but it is user-facing. |
| L4 | `service/AudioServiceActivity.kt` | `enableAudioService()` sets `service = null` in `finally`, which is meaningless there. If the activity is destroyed after `bindService` but before `onServiceConnected`, `service` is null so `disableAudioService()` skips `unbindService` → leaked `ServiceConnection`. |
| L5 | `ui/IntParameterScreen.kt` | `steps = (max - min - 1) / inc` is only correct when `inc` divides the range (`(max-min)/inc - 1` is the right formula); `inc` is otherwise unused for value snapping. Positive values are shown as `+500ms` for the delay parameter, where a sign is odd. |
| L6 | `fx/DelayEffect.cpp` | Per-sample `deque::pop_back` + `push_front` is slow and allocation-prone on the real-time thread; a ring buffer is the usual choice. Both DSP effects also lock a `std::mutex` in `apply()` (audio-path priority inversion). |
| L7 | `plug/AudioPluginFactory.cpp` | `make_plugin` returns `TestAudioPlugin*`, stored through `reinterpret_cast<jna_pointer>` and read back as `AudioPlugin*`. It works only because the base sits at offset 0; store as `AudioPlugin*` explicitly. |
| L8 | `plug/AudioPlugin.kt` | `kotlinx.coroutines` is imported but not declared in `dependencies {}` (arrives transitively via Compose). Declare it. `set()` ignores unknown parameter names silently on the native side. |
| L9 | `etc/JNA.h` | `jna_result::nok` copies `min(max_error_length, size)` bytes with no NUL terminator and no check that `error` is non-null/`max_error_length >= 0`. Works because Kotlin reads by length. |
| L10 | `CMakeLists.txt` | `-Ofast` enables `-ffast-math` (NaN/Inf assumptions) – risky for DSP code that can see denormals/NaNs; `-Werror` makes any NDK/compiler upgrade a build break; `GLOB_RECURSE` has no `CONFIGURE_DEPENDS`; `cmake_minimum_required(3.1...3.19)` is below what `cxx_std_20` needs (≥3.12 to be safe, newer to be meaningful). |
| L11 | `Main.kt` | Enumerates audio devices by reflection and logs at every process start, in release builds too. |
| L12 | `Makefile` | `key-base64` is missing from `.PHONY` and `help`; `base64 -i` is macOS-only; SDK path is hard-wired to `~/Library/Android/sdk`; `HOST` default hard-codes `192.168.178.*`; `KEYDNA` embeds a personal name. |

---

## 3. CI / supply-chain issues

| Severity | Finding |
|----------|---------|
| **High** | Both workflows use `actions/*@master` and `softprops/action-gh-release@master`. `apk.yml` has the **release signing keystore and passwords** in scope; a compromised or force-pushed `master` of any action can exfiltrate them. Pin to a version tag, ideally a commit SHA. |
| Medium | No `permissions:` block – the default `GITHUB_TOKEN` gets broader scope than needed. Set `permissions: contents: read` (and `contents: write` only on the release-upload job). |
| Medium | `apk.yml` runs on every push to `main`, and `build.yml` on every push *and* PR, with no `concurrency` group, so duplicate runs pile up. |
| Low | No Gradle or CPM cache (`setup-gradle` / `actions/cache` for `.cpm` and the Gradle home) → every run re-downloads all native dependencies. |
| Low | `distribution: oracle` for Java; Temurin is the more conventional choice and avoids Oracle licence-terms changes. |
| Low | `build.yml` runs `assembleRelease` then `lintRelease` in separate steps (fine), but runs nothing else – no unit tests, no native tests, no format check. |
| Low | CPM pins most dependencies by commit hash (good); `fmt` and `readerwriterqueue` are pinned by version/tag only and `cpm.cmake` downloads `CPM.cmake` with **no hash check** (`file(DOWNLOAD … EXPECTED_HASH)`). |

---

## 4. Tools worth adding

Ordered by value for this repo. Each is low-effort and doesn't change runtime behaviour.

### Do first
1. **Dependabot / Renovate** (`.github/dependabot.yml`) for `gradle` and `github-actions`. Also pins action versions automatically once H-level CI findings are fixed.
2. **Gradle Version Catalog is already used – add the `gradle-versions` or Renovate check** so `libs.versions.toml` drifts are reported.
3. **Android Lint config** (`lint.xml` + `lint { warningsAsErrors; abortOnError }` in `build.gradle.kts`). Lint already runs in CI but is unconfigured, so it only warns.
4. **ktlint or Spotless** with the existing `.editorconfig` for Kotlin, plus **clang-format** (a `.clang-format` matching the current 2-space style) for C++.
5. **Native unit tests** – the DSP and queue code (`DelayEffect`, `FIFO`, `AudioBlock`, `ChainEffect`, `Debouncer`, `PitchTimbreShiftEffect`) has no Android dependencies except `Log.h`. A host-side CMake target with GoogleTest/Catch2 (run in CI on `ubuntu-latest`) would have caught H1 immediately.
6. **Sanitizers on the host-side test target**: `-fsanitize=address,undefined` (and `thread` for `AudioPipeline`/`FIFO`). Covers H1, M1, M2.

### Next
7. **clang-tidy / cppcheck** in CI, scoped to `voicesmith/src/main/cpp`.
8. **JVM unit tests** for pure logic (`Preferences` defaults, `IntParameterScreen` step maths, `JnaResultByReference`) and a **Compose UI test** for the start/stop button state (would have caught H2).
9. **detekt** for Kotlin static analysis.
10. **CodeQL** (Java/Kotlin + C/C++) scheduled workflow.
11. **`gradle/actions/wrapper-validation`** to verify `gradle-wrapper.jar` – the jar is committed.
12. **Pre-commit config** (`.pre-commit-config.yaml`) running the formatters above, plus a secret scanner such as gitleaks (the repo handles a signing keystore).

### Nice to have
13. `CONTRIBUTING.md` and a `.github/pull_request_template.md`; a `CHANGELOG.md` (F-Droid users see release notes via `fastlane/metadata`).
14. **F-Droid metadata** (`fastlane/metadata/android/en-US/…`) kept in-repo, if not already maintained upstream.
15. Makefile targets: `test`, `lint`, `format`, `install` (`adb install`), and a portable `props` (read `ANDROID_HOME`).
16. A **benchmark/offline harness** (a small host executable that pushes a WAV through `StereoChainEffect<DelayEffect, PitchTimbreShiftEffect>`) – useful for tuning the shifter without a phone and for regression-testing audio output.
17. **Baseline Profiles / R8**: `isMinifyEnabled = false` ships unshrunk code; enabling it is optional but should be a conscious choice (JNA needs keep rules if enabled).

---

## 5. Recommended fix order

1. H1 (delay OOB) – one-line fix, user-reachable at the slider's end stop.
2. H2 (UI state after failed start) and H3 (stuck pipeline after failed `open()`).
3. M1 + M2 (atomic flag, wait predicate) – both are small and fix start-up flakiness.
4. CI: pin actions, add `permissions:`, add Dependabot.
5. M4 (`apply()` instead of `commit()`), M5 (`START_NOT_STICKY`), L3 (`IMPORTANCE_LOW`).
6. Add the host-side native test target with sanitizers; write regression tests for H1/M1/M2.

---

## 6. Assumptions and limits

- No build, lint or test was run; findings are from code reading. H1, H2 and H3 are traced end to end from the UI/API entry point to the faulting line; M5 depends on Android 14+ foreground-service background-start rules and was not reproduced on a device.
- "Tools to add" is interpreted as developer/CI tooling, not new in-app features. If you meant new audio effects or app features (the repo already carries unused `Noise`, `Sine`, `Sweep`, `Vad` and `Bypass` effects ready to expose), say so and I'll scope that separately.
- No source files were modified; this report is the only change.
