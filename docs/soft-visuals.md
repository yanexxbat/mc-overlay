# Soft world effects

The October 2026 refinement leaves player ESP, its defaults and its renderer
unchanged. It affects the target marker and decorative world effects:

- Target-marker corners use circular, tangent arcs. New configurations use a
  larger corner radius and slimmer stroke; saved settings remain respected.
  Rear edges remain available, with quieter opacity to reduce the wireframe look.
- Marker glow spreads gently around a less glaring inner highlight.
- World particles retain more of their selected colour, with softer white cores
  and lower halo opacity. Stars have rounded arms instead of pointed diamonds.
- Trail and ring segment ends receive soft, depth-tagged caps. Shared textured
  strokes use a broader, quieter halo for trails, rings and related world effects.

No module activation, tracking, camera projection, persistence or player-ESP
settings are changed. Existing glow and colour controls remain effective.

Validation: build the canonical `build` directory and run the existing
`world_visuals`, `aura_rendering`, `depth_rendering`, `esp_rendering`,
`smp_features`, `rockstar_features`, `hole_esp` and `portable_client` checks.
These are synthetic checks; an in-game visual review is still needed.
