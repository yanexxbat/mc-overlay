# Launcher motion

The complete implementation lives in `anim.h`, `launcher_ui.hpp/.cpp`,
`injector_main.cpp` and `../common/flat_app.hpp/.cpp`. No new dependency is needed.
The existing project uses C++20; the animation helpers themselves use C++17 APIs.

## Tuning

All motion times are seconds in `anim.h`:

- `Hover` / `Press`: 0.15; `Focus`: 0.20; `Transition`: 0.24.
- `Intro`: 2.4 total, including 0.3 entrance and 0.3 exit.
- `IntroEnabled`: set false to omit the intro while retaining other animations.
- `Success` / `Error`: 0.30; `Copied`: 1.4 before a 0.24 fade back.
- `Slide` / `Shake`: displacement in pixels.
- `Accent`, `AccentHover`, `AccentPressed`, `Danger`, `Disabled`: animation colors.
  The default accent aliases the existing workspace palette: **#B3DB8F**.
  Override the constants here for this launcher; change `workspace_style.hpp`
  only when the shared application palette should also change.
- `easeOutCubic` and `easeInOutCubic` define the easing curves.

The Animations checkbox disables the intro, movement, fades, checkbox drawing
animation and looping progress motion. Feedback remains readable as static states.
Its preference is saved in `launcher-animations.txt` under the existing Storage()
directory, and loaded before the first visible frame. A disabled intro never
replays when animations are enabled again in the same session. Any key or mouse
button dismisses the intro; that input cannot activate the controls underneath.

## Status hooks and ownership

`VerificationStatus` is declared in `launcher_ui.hpp`. `AppState::verification`
in `injector_main.cpp` is atomic. The wiring is already complete:

1. Immediately before starting the existing async Login call, publish Verifying.
2. After Login returns, publish Success or Error using `result.success`.
3. On an exception, publish Error and rethrow to retain existing error handling.
4. The UI reads the status with acquire ordering, polls the future without
   waiting, and consumes the original result on the UI thread.

The worker never writes View, ImGui state, or strings displayed by the UI.
The existing authBusy/authError/licensed fields supply presentation edges.
A successful verification shows a short drawn check before revealing the instance
selector. No license result, validation, expiry rule, network request, HWID
calculation, or injection operation is changed.

Each View owns its widget-ID tween map and timeline. Timelines advance using
ImGui DeltaTime. Log snapshots are parsed only when changed; retained tail lines
keep their birth times. Stable button IDs retain hover/press state across labels.
No sleep, busy-wait, extra worker, texture blur or heavy dependency was added.

## Window lifecycle

FlatApp exposes optional onCloseRequested and SetOpacity presentation hooks.
The launcher fades its existing layered HWND in/out with the same motion tokens.
Close and Alt+F4 use the same exit; a minimized window closes immediately because
its render loop is suspended. Cancellation is requested at the beginning of exit.
The existing AppState destructor still joins outstanding jobs before destruction.

## Validation

Built mcov_injector, mcov_launcher_window_check and mcov_control_panel_check.
The existing native test covers 640x640 and 480x380, rounded clipping and controls.
Additional local checks in `.motion-check` cover 30/60/144 Hz tween progression,
reduced-motion snapping, intro timing/keyboard skip, verification result edges,
and log append continuity. Native render captures cover intro, sign-in, progress,
error, success and Logs. Live authentication/injection is not exercised by UI tests.
