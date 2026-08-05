# Project and Fixture Layout

The C++ engine keeps fixture definitions and show data separate.

Fixture definitions live in `fixtures/`. They describe what a physical fixture
can do and which relative DMX channels provide each capability.

Show projects live in `shows/`. They describe the active patch, presets and
scene collections. A show references fixture definitions by id; it does not
repeat their DMX capability mapping.

Current layout:

```text
fixtures/
  collection.json
  rgb_bar_8seg.json
  generic_rgb_par_7ch.json
  zkymzl_11ch_moving_head.json
  stairville_1500w_strobe_2ch.json

shows/
  default.json
  rgb_par_scenes.json
```

`fixtures/collection.json` is the default fixture set loaded by the current
engine. It intentionally contains only the fixtures that exist in the current
rig: LED bar, moving head and strobe.

The intended engine flow is:

```text
FixtureDefinition + FixturePatch
  -> typed fixture abstraction
  -> semantic commands such as set_color/look_at/pulse
  -> DMX channel writes
```

The three RGB PARs form a second lighting zone in physical order:

```text
DMX 100 (left) -> DMX 110 (center) -> DMX 120 (right)
```

In linked mode, this zone samples the dance-floor LED looks. In independent
mode, it selects a JSON-defined scene from `shows/rgb_par_scenes.json` using
its own zone mood. The global master and blackout still apply to both zones.
