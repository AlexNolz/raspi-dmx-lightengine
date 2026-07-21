# Show JSON DSL

Ziel: Lichtshows sollen in JSON gebaut werden können, ohne C++ neu zu kompilieren.

Die Grenze ist bewusst klar:

- C++ implementiert sichere Bausteine wie `beat_pulse`, `chase`, `position_chase`.
- JSON kombiniert diese Bausteine mit Parametern, Gruppen, Paletten und Presets.
- JSON wird keine freie Programmiersprache. Keine Schleifen, kein beliebiger Code.

## Dateien

- `shows/default.json`: Show-Projekt, Patch, Presets, aktive Szenen-Sammlungen.
- `shows/color_palettes.json`: zusammenpassende Farbpaletten und Preset-Palettengruppen.
- `shows/rgb_scenes.json`: RGB-/LED-Bar-Szenen.
- `shows/moving_head_scenes.json`: Moving-Head-Szenen.

## Farbpaletten

Eine Palette ist eine feste Sammlung zusammenpassender Farben:

```json
{
  "id": "club_blue_amber",
  "name": "Club Blue / Cyan / Amber",
  "colors": ["blue", "cyan", "amber", "pink"]
}
```

Paletten verwenden absichtlich Farbnamen, keine harten RGB-Werte. RGB-Bars lösen diese
Namen auf RGB auf. Moving Heads lösen dieselben Namen über ihr Fixture-Farbrad auf.

Beispiel ZKYMZL:

```json
"color": {
  "channel": 5,
  "type": "wheel",
  "colors": {
    "white": 3,
    "red": 11,
    "cyan": 18,
    "amber": 25,
    "blue": 70,
    "yellow": 80,
    "green": 90,
    "magenta": 100
  }
}
```

Preset-Paletten definieren, welche Paletten zu einem Preset passen:

```json
{ "id": "club", "palettes": ["club_blue_amber", "club_teal_pink", "deep_blue"] }
```

Wenn eine Szene `"palette": "preset"` nutzt, wählt die Engine passend zum aktiven Preset eine Palette aus dieser Gruppe. Die Auswahl darf beat-/phrasenabhängig wechseln, damit Shows nicht statisch wirken.

## RGB-Szenen

```json
{
  "id": "rgb_chase",
  "name": "Neon Chase",
  "type": "chase",
  "palette": "preset",
  "speed": 1.15,
  "intensity": 0.62
}
```

Unterstützte RGB-Typen:

- `static_glow`: ruhiger Grundlook
- `beat_pulse`: Beat-Hit von der Mitte
- `chase`: laufende Welle
- `comet`: laufender Lichtpunkt mit Tail
- `beat_spark`: kurze Beat-Sparks

## Moving-Head-Szenen

Moving-Head-Szenen sollen dieselbe Idee nutzen: JSON beschreibt Zielpositionen, Dimmer, Farbe, Gobo und Strobe-Verhalten.

```json
{
  "id": "mh_cross_sweep",
  "name": "Cross Sweep",
  "type": "position_chase",
  "fixtures": "moving_heads",
  "palette": "preset",
  "positions": [
    { "pan": 70, "tilt": 95, "label": "left_front" },
    { "pan": 186, "tilt": 95, "label": "right_front" }
  ],
  "motion": { "mode": "sine", "speed": 0.5, "spread": 0.25 },
  "dimmer": { "mode": "constant", "value": 0.65 },
  "color": { "mode": "palette_step", "every_beats": 8 },
  "gobo": { "mode": "static", "name": "open" },
  "strobe": { "mode": "off" }
}
```

Geplante Moving-Head-Typen:

- `position_pulse`: feste Position, Dimmer pulst auf Beat
- `position_chase`: Fixtures laufen phasenversetzt durch Positionen
- `beat_hits`: kurze Beat-Hits mit Farbe/Gobo-Wechsel
- `fan`: Pan/Tilt-Fächer über mehrere Fixtures
- `circle`: Kreis-/Acht-Bewegung, später mit Kalibrierung

## Gobo Wheels

Gobos werden in Show-JSON über Namen angesprochen, nicht über DMX-Zahlen:

```json
"gobo": {
  "mode": "beat_step",
  "names": ["open", "gobo_1", "gobo_2", "gobo_3"],
  "shake_when_mood_above": 0.62
}
```

Die konkrete Fixture übersetzt diese Namen auf DMX-Werte:

```json
"gobo": {
  "channel": 6,
  "type": "wheel",
  "open": 0,
  "gobos": {
    "open": 0,
    "gobo_1": 10,
    "gobo_2": 18
  },
  "effects": {
    "shake_offset": 64,
    "shake_min": 64,
    "shake_max": 127
  }
}
```

Damit kann eine Show dieselben Gobo-Namen verwenden, auch wenn ein anderes Moving-Head-Modell andere DMX-Werte braucht.

## Presets

Presets wählen keine DMX-Kanäle direkt. Sie aktivieren Szenen:

```json
{
  "id": "rave",
  "effects": ["rgb_beat_pulse", "rgb_chase", "rgb_comet", "rgb_spark"],
  "motion_scenes": ["mh_cross_sweep", "mh_rave_hits"]
}
```

Damit bleibt die Abstraktion sauber:

```text
Preset -> Szenen -> Fixture-Abstraktion -> DMX Frame -> ArtNet
```
