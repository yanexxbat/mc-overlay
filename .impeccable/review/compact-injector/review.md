# Compact injector finish review

Disposition: ship. Independent review found no material UI issues.

The injector uses the approved client's CompactStyle: anthracite root at 88% opacity,
coral accents, compact Inter typography, 12px corners and narrow left Start/Diagnose
navigation. The primary action and close footer stay outside scrolling content.

## Evidence

All screenshots in this folder are synthetic fixtures of the actual launcher.
`launcher-native-start.png` and `launcher-native-diagnose.png` are hidden HWND
swapchain readbacks, not desktop captures. They verify rendered alpha and controls;
they do not establish live desktop or Minecraft behavior.

Reviewed: ready/empty at 640x640, busy/error at 640x520, Start and Diagnose at
480x380, Diagnose at 640x640, and native readbacks at both native test sizes.

## Verification

- Production `mcov` and developer `mcov_injector` built successfully.
- `mcov_control_panel_check`: real instance selection, Start, duplicate-start
  prevention, compact sidebar tabs and footer close, title close/minimize,
  scoped style restoration, plus existing client regression checks passed.
- `mcov_launcher_window_check`: hidden first frame, translucent panel alpha,
  transparent margins, visible text, presentation, resize and rounded region passed.
- `portable_client`: current EXE, embedded payload and two-file distribution passed.
- Independent visual review: all nine captures valid; direction and craft passed.

The injector shell no longer initializes or calls LiquidGlass or backdrop capture.
The DWM frame uses negative margins and the D3D11 target clears to transparent.
Existing selection, asynchronous connection, JDK recovery, log and process behavior
remain unchanged. Live attach was outside this presentation review.
