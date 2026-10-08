# RenderMenu und ApplyCustomStyle integrieren

Das erste freistehende Beispiel wurde durch das produktive Menue gemaess
Benutzerreferenz ersetzt. Vollstaendige Implementierungen:

- `src/common/compact_style.hpp`: `mcov::style::ApplyCustomStyle()` und lokaler
  `CompactStyle`, der das vorherige Theme und den Font nach dem Rendern wiederherstellt.
- `src/overlay/control_panel.cpp`: `mcov::control_panel::RenderMenu()` mit den
  realen Content-, Catalog- und Activate-Callbacks des vorhandenen Clients.
- `examples/modern_menu.hpp`: kleiner Adapter auf diese produktiven Funktionen.

`Overlay::DrawMenu()` ist bereits angebunden. Fuer weitere Aufrufer bleibt
`mcov::control_panel::Render()` kompatibel. `State` dauerhaft im Aufrufer halten
und das Menue zwischen `NewFrame()` und `Render()` zeichnen. Der Aufrufer setzt
Close/Reset-Actions wie bisher um und besitzt die echten Modulwerte.

`State::selectedSections` behaelt den gewaehlten Abschnitt je Kategorie;
`State::search` filtert die Modulnamen der aktuellen Kategorie. Tabs waehlen den
Abschnitt, `Activate` fuehrt echte Toggle-/Action-Operationen aus. Details werden
im selben Fenster durch `SectionScope` auf den gewaehlten Abschnitt begrenzt.
Die zwei Child-Bereiche Navigation/Content und nativen Tab-/Eingabe-IDs bleiben
Teil des produktiven Renderers. Der Adapter erfindet keine Modulwerte.

Das direkte `ApplyCustomStyle()` veraendert den globalen Style. Beim Client
wird es deshalb durch `CompactStyle` lokal verwendet; ein erneuter globaler
Theme-Aufruf pro Frame ist nicht erforderlich. Fonts/Backend-Initialisierung
bleiben beim vorhandenen Lifecycle. Weitere Layout-, Performance- und
Bedienungsdetails stehen in [compact-ui.md](compact-ui.md).
