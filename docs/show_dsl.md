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

## Laufzeit-Layer

Die Engine kombiniert vier voneinander getrennte Ebenen. Keine Ebene schreibt direkt
ArtNet; erst die Fixture-Abstraktion übersetzt das Ergebnis nach DMX.

```text
Preset + Mood + Musikdynamik
          |
          +-- LED-Szenen-Layer (Muster mit Farbe 1..4)
          +-- Moving-Layer (Positionen und Dimmerbewegung)
          +-- Farb-Layer (eine gemeinsame Palette für alle Fixtures)
          +-- Gobo-Layer (Muster, Wechselrate und Shake)
                              |
                       Fixture-Abstraktion -> DMX -> ArtNet
```

LED-Szene und Moving-Head-Szene werden unabhängig aus den im Preset erlaubten
Sammlungen gewählt. Dadurch kann dasselbe LED-Muster mit mehreren Bewegungen vorkommen.
Die Auswahl bleibt innerhalb einer 16-Beat-Phrase stabil. Effekte mit
`"energy_min": 0.85` oder höher werden automatisch nur bei einem erkannten Peak gewählt.

Der Farb-Layer ist von beiden Szenen unabhängig und liefert beiden Fixture-Arten dieselbe
sortierte Palette. Seine Wechselrate folgt der Songphase:

- ruhig: 64 Beats
- Groove: 32 Beats
- Aufbau: 16 Beats
- Peak: 8 Beats
- Abbau: 32 Beats

Der Gobo-Layer hält Muster ruhig 16 Beats, im Aufbau 8 Beats und am Peak 1 Beat.
Shake ist zusätzlich an Peak, Mood-Schwelle und `allow_shake` der Moving-Szene gebunden.
In der Oberfläche lassen sich einzelne Gobo-Muster sowie der schnelle Peak-Wechsel und
Shake getrennt freigeben. Mindestens ein Muster bleibt immer aktiv.

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
    "white": 8,
    "red": 24,
    "cyan": 40,
    "amber": 56,
    "blue": 72,
    "yellow": 88,
    "green": 104,
    "magenta": 120
  }
}
```

Preset-Paletten definieren, welche Paletten zu einem Preset passen:

```json
{ "id": "club", "palettes": ["club_blue_amber", "club_teal_pink", "deep_blue"] }
```

Wenn eine Szene `"palette": "preset"` nutzt, wählt die Engine passend zum aktiven Preset eine Palette aus dieser Gruppe. Die Auswahl darf beat-/phrasenabhängig wechseln, damit Shows nicht statisch wirken.

Die Oberfläche zeigt alle Paletten als aktivierbaren Farbpool. „Nächste Farbe“ wechselt
nur diesen Layer und lässt LED-Muster, Moving-Head-Bewegung und Gobo unverändert. Das
Preset `rgb_hard` verwendet absichtlich ausschließlich `red`, `blue` und `green`; dabei
werden nur die abstrakten Farb-Slots zyklisch vertauscht.

## RGB-Szenen

```json
{
  "id": "rgb_chase",
  "name": "Neon Chase",
  "type": "chase",
  "palette": "preset",
  "color_slots": 3,
  "speed": 1.15,
  "intensity": 0.62
}
```

`color_slots` bedeutet nicht feste RGB-Werte, sondern `Farbe 1`, `Farbe 2` usw. aus
der gerade aktiven Palette. Unterstützte RGB-Typen stehen in `shows/rgb_scenes.json`;
zu den grundlegenden Bausteinen gehören:

- `static_glow`: ruhiger Grundlook
- `beat_pulse`: Beat-Hit von der Mitte
- `chase`: laufende Welle
- `comet`: laufender Lichtpunkt mit Tail
- `beat_spark`: kurze Beat-Sparks

## Moving-Head-Szenen

Moving-Head-Szenen beschreiben nur Position, Bewegung, Dimmer und ob ein starker
Gobo-Effekt grundsätzlich erlaubt ist. Farbe und konkretes Gobo liefert der jeweilige
separate Layer.

```json
{
  "id": "mh_cross_sweep",
  "name": "Cross Sweep",
  "type": "sine_pan",
  "points": [[0.50, 0.65]],
  "speed_low": 0.28,
  "speed_high": 0.84,
  "x_amount": 0.36,
  "y_amount": 0.13,
  "dimmer_min": 0.55,
  "dimmer_max": 1.0,
  "energy_min": 0.42,
  "allow_shake": false
}
```

## Gobo Wheels

Gobos werden in Show-JSON über Namen angesprochen, nicht über DMX-Zahlen:

```json
"gobo": {
  "mode": "beat_step",
  "names": ["open", "spiral", "octopus", "cloverleaf"],
  "shake_when_mood_above": 0.62
}
```

Die konkrete Fixture übersetzt diese Namen auf DMX-Werte:

```json
"gobo": {
  "channel": 6,
  "type": "wheel",
  "open": 18,
  "gobos": {
    "spiral": 0,
    "octopus": 10,
    "open": 18,
    "cloverleaf": 26
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
Preset + Mood + Musik -> Layer-Auswahl -> Fixture-Abstraktion -> DMX Frame -> ArtNet
```

## RGB-Fluterzone

Die PAR-Fluter sind eine eigene Zone und stehen physisch in der Reihenfolge
links (100), Mitte (110), rechts (120). Ihre Bewegungsmuster werden in
`shows/rgb_par_scenes.json` beschrieben. Eine Szene definiert:

- den Mustertyp, zum Beispiel `gradient`, `slow_wave` oder `chase`
- den erlaubten Mood-Bereich für die automatische Auswahl
- die Geschwindigkeit in Beats
- minimale und maximale Helligkeit

Im gekoppelten Modus folgen die PARs den LED-Bars der Tanzfläche. Im getrennten
Modus verwendet die Zone ihren eigenen Mood und eine automatische oder manuell
gewählte PAR-Szene. Farben kommen weiterhin aus der gemeinsamen Farbpalette,
damit beide Bereiche auch bei unterschiedlicher Intensität zusammenpassen.
