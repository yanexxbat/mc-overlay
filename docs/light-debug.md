# Light ESP
Enable **Blocks > Light ESP**; right-click for its own keybind and settings.
It operates without Chunkfinder. Restart Minecraft after installing this build
(IPC v39), then reattach using build/client/Keim.exe.

The scanner reads existing loaded block-light arrays below Y 0, including light
already present when enabled. Only values at least Minimum light and strictly
greater than sky light qualify. Unknown sky sections are skipped. Radius is
1–16 chunks (default 8); minimum light is 1–15 (default 5).
Denoise removes weak boundary samples without a stronger local neighbour.
This is a bounded approximation of the reference's connected-region denoising.
Up to four sections are processed per client tick with a 2 ms scheduling budget;
each started section completes. Completed sweeps pause for one second.

Projected light cells are merged into a continuous translucent 2D field, with no internal block outlines. A feathered union mask avoids overlapping fill seams. Screen-space projection approximates coverage; overlapping areas at different depths merge visually.
Higher light levels approach white. Opacity, edge softness and draw distance are
independent settings, with fades and Reduce motion support.
Results are limited to 500 light samples so Chunkfinder evidence retains capacity.
Only section boundary samples are shown; this is not identification of light
sources or guaranteed base detection. Server-provided missing/modified lighting
cannot be reconstructed. Live DonutSMP behaviour has not been verified.
