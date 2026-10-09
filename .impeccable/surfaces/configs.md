# Local client configs

Mode: Operate. Native Windows x64 / C++20 / Dear ImGui.

The user requested named configs stored directly in the client folder, outside
Temp and AppData. Extend the confirmed charcoal/coral module-widget interface:
Settings > Configs opens the existing independent settings window. Preserve the
approved menu, injector and HUD theme, Inter typography and native controls.

Profiles are readable `configs/<name>.ini` files beside the portable launcher.
`client-settings.ini` also lives beside it; the launcher forwards its directory
to the extracted overlay independently of the runtime/log cache. Existing
automatic preferences migrate once without overwriting local settings.

The first viewport exposes the saved list, name, Save/Overwrite, Load selected
and Refresh. Empty, disabled, success and failure states explain the next action.
Overwrite is explicit in the button label. Loading restores saved module states,
values, binds, HUD layout, friend relations and waypoints. A profile is validated
fully before applying it. Invalid names cannot escape the config directory.
File writes use atomic replacement with a temporary file in that same folder.

Target captures: 1280x800 empty/success/error and 480x360 top/bottom. Small views
use the existing scrollable settings pane. List text and stored-folder text use
native clipping/scrolling with full text available on hover. File enumeration is
cached; no per-frame directory scan or new rendering effect is required.

Verification uses actual native offscreen ImGui/D3D11 WARP fixtures and profile
round-trips, not a live Minecraft attach. The portable build must contain the
current overlay payload. Windows HTML/CSS detection is inapplicable.
