# mcov

## Platform

Native Windows desktop overlay attached to the Minecraft client window.

## Stack

C++20, Dear ImGui, Win32 and D3D11. A separate JNI/JVMTI agent publishes coherent
world snapshots and executes existing client-tick combat actions through IPC v14.

## User and purpose

The user configures Minecraft visual/combat modules, observes game information,
and operates features through an external menu and keyboard/mouse bindings.
Success means working features with clear controls, a modular HUD, and preserved
camera/entity projection stability.

## Confirmed constraints

- Follow the supplied classic dark/purple menu screenshot; retain actual mcov features.
- Keep the simplified Kill Aura workflow and always independent camera behavior.
- Renderers consume snapshots; UI/HUD changes add no render-thread JNI.
- Keep one canonical build directory and existing version/Forge compatibility.
- Ship one portable mcov.exe containing the client payload; the launcher matches
  the classic client UI and handles selection, connection and overlay startup.
- Module keybinds start unbound, as explicitly chosen by the user. Menu INSERT,
  HUD editor F6 and Quit END are defaults; all can be rebound.
- Support keyboard/mouse bindings, toggle/hold operation, movable/scalable HUD elements.

## Persistence

HUD positions, per-element scale/visibility and keybinds are saved locally.
Other visual/combat settings and friend relations retain their existing session behavior.

- ThunderHack-Recode-inspired visuals are requested, with particles and trails prioritized.
