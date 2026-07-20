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
  rgb_bar_8seg.json
  zkymzl_11ch_moving_head.json
  stairville_1500w_strobe_2ch.json
  stairville_af40_fog_1ch.json

shows/
  default.json
```

The intended engine flow is:

```text
FixtureDefinition + FixturePatch
  -> typed fixture abstraction
  -> semantic commands such as set_color/look_at/pulse
  -> DMX channel writes
```
