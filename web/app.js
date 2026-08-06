const stateUrl = "/api/state";
const controlUrl = "/api/control";

let lastState = null;
let effectsBuilt = false;
let colorPalettesBuilt = false;
let motionBuilt = false;
let motionScenesBuilt = false;
let goboModesBuilt = false;
let gobosBuilt = false;
let movingHeadColorsBuilt = false;
let parScenesBuilt = false;
let rigBuilt = false;
let pendingSlider = null;
let artnetFormDirty = false;
let discoCalibrationDraft = null;
let discoCalibrationDirty = false;
let discoCalibrationTimer = null;

const preview = document.querySelector("#preview");
const os2lStatus = document.querySelector("#os2lStatus");
const bpmStatus = document.querySelector("#bpmStatus");
const energyStatus = document.querySelector("#energyStatus");
const effectStatus = document.querySelector("#effectStatus");
const layerStatus = document.querySelector("#layerStatus");
const targetStatus = document.querySelector("#targetStatus");
const mood = document.querySelector("#mood");
const moodValue = document.querySelector("#moodValue");
const master = document.querySelector("#master");
const masterValue = document.querySelector("#masterValue");
const ledMaster = document.querySelector("#ledMaster");
const ledMasterValue = document.querySelector("#ledMasterValue");
const motionMaster = document.querySelector("#motionMaster");
const motionMasterValue = document.querySelector("#motionMasterValue");
const ledEnabled = document.querySelector("#ledEnabled");
const ledBeatPulse = document.querySelector("#ledBeatPulse");
const parZoneLinked = document.querySelector("#parZoneLinked");
const parZoneMaster = document.querySelector("#parZoneMaster");
const parZoneMasterValue = document.querySelector("#parZoneMasterValue");
const parZoneMood = document.querySelector("#parZoneMood");
const parZoneMoodValue = document.querySelector("#parZoneMoodValue");
const parZoneScene = document.querySelector("#parZoneScene");
const parZoneStatus = document.querySelector("#parZoneStatus");
const motionEnabled = document.querySelector("#motionEnabled");
const motionBeatPulse = document.querySelector("#motionBeatPulse");
const strobeArmed = document.querySelector("#strobeArmed");
const strobeBeatPulse = document.querySelector("#strobeBeatPulse");
const strobeMaster = document.querySelector("#strobeMaster");
const strobeMasterValue = document.querySelector("#strobeMasterValue");
const strobeSpeed = document.querySelector("#strobeSpeed");
const strobeSpeedValue = document.querySelector("#strobeSpeedValue");
const runButton = document.querySelector("#runButton");
const blackoutButton = document.querySelector("#blackoutButton");
const effectsBox = document.querySelector("#effects");
const colorPalettesBox = document.querySelector("#colorPalettes");
const motionBox = document.querySelector("#motionModes");
const motionScenesBox = document.querySelector("#motionScenes");
const goboEnabled = document.querySelector("#goboEnabled");
const goboMode = document.querySelector("#goboMode");
const goboSelect = document.querySelector("#goboSelect");
const goboHighpointOnly = document.querySelector("#goboHighpointOnly");
const goboFastPeak = document.querySelector("#goboFastPeak");
const goboShakeEnabled = document.querySelector("#goboShakeEnabled");
const goboShakeMood = document.querySelector("#goboShakeMood");
const goboShakeMoodValue = document.querySelector("#goboShakeMoodValue");
const goboPatternsBox = document.querySelector("#goboPatterns");
const colorWheelEnabled = document.querySelector("#colorWheelEnabled");
const colorWheelSelect = document.querySelector("#colorWheelSelect");
const colorWheelUseRaw = document.querySelector("#colorWheelUseRaw");
const colorWheelRaw = document.querySelector("#colorWheelRaw");
const colorWheelRawValue = document.querySelector("#colorWheelRawValue");
const rigBox = document.querySelector("#rig");
const artnetHost = document.querySelector("#artnetHost");
const universe = document.querySelector("#universe");
const startChannel = document.querySelector("#startChannel");
const segmentsInput = document.querySelector("#segmentsInput");
const discoTest = document.querySelector("#discoTest");
const discoTilt = document.querySelector("#discoTilt");
const discoTiltValue = document.querySelector("#discoTiltValue");
const discoHead = document.querySelector("#discoHead");
const discoPan = document.querySelector("#discoPan");
const discoPanValue = document.querySelector("#discoPanValue");
const discoCalibrationStatus = document.querySelector("#discoCalibrationStatus");
const discoCalibrationFile = document.querySelector("#discoCalibrationFile");

async function send(payload) {
  const response = await fetch(controlUrl, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  });
  lastState = await response.json();
  render(lastState);
}

async function loadState() {
  const response = await fetch(stateUrl);
  lastState = await response.json();
  render(lastState);
}

function rgb(color) {
  return `rgb(${color.r}, ${color.g}, ${color.b})`;
}

function colorName(name) {
  const names = {
    white: "Weiß", red: "Rot", cyan: "Cyan", teal: "Türkis", amber: "Amber",
    orange: "Orange", blue: "Blau", yellow: "Gelb", acid: "Acid-Gelb",
    green: "Grün", uv: "UV/Violett", magenta: "Magenta", pink: "Pink"
  };
  return names[name] || name;
}

function render(state) {
  const config = state.config;
  const artnetTarget = config.artnet_host === "127.0.0.1"
    ? `DMX Bridge lokal / U${config.artnet_universe} / CH${config.led_start_channel}`
    : `ArtNet ${config.artnet_host} / U${config.artnet_universe} / CH${config.led_start_channel}`;
  os2lStatus.textContent = state.os2l_connected ? "OS2L live" : "OS2L wartet";
  os2lStatus.className = state.os2l_connected ? "status good" : "status bad";
  bpmStatus.textContent = `${state.bpm.toFixed(1)} BPM`;
  const sectionLabels = { calm: "Ruhig", groove: "Groove", buildup: "Aufbau", peak: "Höhepunkt", release: "Abbau" };
  const strengthHint = state.beat_strength_available ? " · VDJ-Dynamik" : " · Phrasenmodell";
  energyStatus.textContent = `${sectionLabels[state.music_section] || state.music_section} ${Math.round((state.music_energy ?? 0.5) * 100)}%${strengthHint}`;
  effectStatus.textContent = state.active_effect_label;
  const layerState = state.layer_state || {};
  const colors = Array.isArray(layerState.color_slots) ? layerState.color_slots.map(colorName).join(" + ") : "Farben";
  layerStatus.textContent = `${layerState.palette_label || "Palette"}: ${colors} · ${layerState.motion_scene || "Bewegung"}`;
  targetStatus.textContent = artnetTarget;

  mood.value = config.mood;
  moodValue.textContent = config.mood;
  master.value = Math.round(config.master * 100);
  masterValue.textContent = Math.round(config.master * 100);
  ledMaster.value = Math.round((config.led_master ?? 1) * 100);
  ledMasterValue.textContent = Math.round((config.led_master ?? 1) * 100);
  motionMaster.value = Math.round((config.motion_master ?? 1) * 100);
  motionMasterValue.textContent = Math.round((config.motion_master ?? 1) * 100);
  const discoConfig = config.disco_ball || { test_mode: false, tilt: 0.68, pans: [0.333, 0.333, 0.333, 0.333] };
  const movingHeads = (config.fixtures && config.fixtures.moving_heads) || [];
  if (discoHead.options.length !== movingHeads.length) {
    discoHead.replaceChildren(...movingHeads.map((head, index) => {
      const option = document.createElement("option");
      option.value = String(index);
      option.textContent = head.name || `Moving Head ${index + 1}`;
      return option;
    }));
  }
  if (!discoCalibrationDraft || !discoCalibrationDirty) {
    discoCalibrationDraft = {
      test_mode: Boolean(discoConfig.test_mode),
      selected_head: Number(discoConfig.selected_head ?? 0),
      tilt: Number(discoConfig.tilt ?? 0.68),
      pans: [0, 1, 2, 3].map((index) => Number((discoConfig.pans || [])[index] ?? 0.333))
    };
    syncDiscoCalibrationControls();
  }
  discoTest.classList.toggle("active", Boolean(discoCalibrationDraft.test_mode));
  discoTest.textContent = discoCalibrationDraft.test_mode ? "Testfahrt beenden" : "Langsame Testfahrt starten";
  const strobeConfig = (config.fixtures && config.fixtures.strobe) || {};
  ledEnabled.checked = Boolean(config.layers && config.layers.led_bars);
  ledBeatPulse.checked = Boolean(config.led_beat_pulse);
  const parZoneConfig = config.rgb_par_zone || {};
  if (!parScenesBuilt && state.rgb_par_scenes) {
    parScenesBuilt = true;
    parZoneScene.replaceChildren(...Object.entries(state.rgb_par_scenes).map(([key, label]) => {
      const option = document.createElement("option");
      option.value = key;
      option.textContent = label;
      return option;
    }));
  }
  parZoneLinked.checked = Boolean(parZoneConfig.linked);
  parZoneMaster.value = Math.round((config.rgb_par_master ?? 1) * 100);
  parZoneMasterValue.textContent = parZoneMaster.value;
  parZoneMood.value = parZoneConfig.mood ?? 35;
  parZoneMoodValue.textContent = parZoneMood.value;
  parZoneScene.value = parZoneConfig.scene || "auto";
  parZoneMood.disabled = parZoneLinked.checked;
  parZoneScene.disabled = parZoneLinked.checked;
  parZoneStatus.textContent = parZoneLinked.checked
    ? "Eine große Zone · folgt der Tanzfläche vollständig"
    : `Eigene Zone · aktiv: ${(state.rgb_par_scenes && state.rgb_par_scenes[parZoneConfig.resolved_scene]) || parZoneConfig.resolved_scene || "Automatik"}`;
  motionEnabled.checked = Boolean(config.layers && config.layers.motion);
  motionBeatPulse.checked = Boolean(config.motion_beat_pulse);
  strobeArmed.checked = Boolean(strobeConfig.armed);
  strobeBeatPulse.checked = Boolean(strobeConfig.beat_pulse);
  strobeMaster.value = Math.round((strobeConfig.master ?? 1) * 100);
  strobeMasterValue.textContent = Math.round((strobeConfig.master ?? 1) * 100);
  strobeSpeed.value = Math.round((strobeConfig.speed ?? 1) * 100);
  strobeSpeedValue.textContent = Math.round((strobeConfig.speed ?? 1) * 100);
  const goboConfig = config.gobo || {};
  if (!goboModesBuilt && state.gobo_modes) {
    goboModesBuilt = true;
    goboMode.replaceChildren(...Object.entries(state.gobo_modes).map(([key, label]) => {
      const option = document.createElement("option");
      option.value = key;
      option.textContent = label;
      return option;
    }));
  }
  if (!gobosBuilt && state.gobos) {
    gobosBuilt = true;
    goboSelect.replaceChildren(...Object.entries(state.gobos).map(([key, label]) => {
      const option = document.createElement("option");
      option.value = key;
      option.textContent = label;
      return option;
    }));
    goboPatternsBox.replaceChildren(...Object.entries(state.gobos).map(([key, label]) => {
      const row = document.createElement("label");
      row.className = "effect-toggle gobo-pattern-toggle";
      const checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.dataset.goboPattern = key;
      checkbox.addEventListener("change", () => {
        send({ action: "toggle_gobo_pattern", gobo: key, enabled: checkbox.checked });
      });
      const text = document.createElement("span");
      text.textContent = label;
      row.append(checkbox, text);
      return row;
    }));
  }
  goboEnabled.checked = Boolean(goboConfig.enabled);
  goboMode.value = goboConfig.mode || "beat_step";
  goboSelect.value = goboConfig.selected_gobo || "open";
  goboHighpointOnly.checked = Boolean(goboConfig.highpoint_only);
  goboFastPeak.checked = Boolean(goboConfig.fast_peak_enabled);
  goboShakeEnabled.checked = Boolean(goboConfig.shake_enabled);
  goboShakeMood.value = Math.round((goboConfig.shake_mood_threshold ?? 0.62) * 100);
  goboShakeMoodValue.textContent = goboShakeMood.value;
  document.querySelectorAll("[data-gobo-pattern]").forEach((checkbox) => {
    checkbox.checked = (config.enabled_gobos || []).includes(checkbox.dataset.goboPattern);
    checkbox.closest(".gobo-pattern-toggle")?.classList.toggle("active", checkbox.checked);
  });
  const colorWheelConfig = config.color_wheel || {};
  if (!movingHeadColorsBuilt && state.moving_head_colors) {
    movingHeadColorsBuilt = true;
    colorWheelSelect.replaceChildren(...Object.entries(state.moving_head_colors).map(([key, label]) => {
      const option = document.createElement("option");
      option.value = key;
      option.textContent = label;
      return option;
    }));
  }
  colorWheelEnabled.checked = Boolean(colorWheelConfig.enabled);
  colorWheelUseRaw.checked = Boolean(colorWheelConfig.use_raw_value);
  colorWheelSelect.value = colorWheelConfig.selected_color || "white";
  colorWheelRaw.min = colorWheelConfig.test_min ?? 0;
  colorWheelRaw.max = colorWheelConfig.test_max ?? 255;
  colorWheelRaw.step = colorWheelConfig.test_step ?? 1;
  colorWheelRaw.value = colorWheelConfig.raw_value ?? 8;
  colorWheelRawValue.textContent = colorWheelRaw.value;
  runButton.textContent = state.running ? "Running" : "Stopped";
  runButton.classList.toggle("active", state.running);
  blackoutButton.classList.toggle("active", state.blackout);

  // State is refreshed four times per second. Do not overwrite ArtNet values
  // while the user is editing them, otherwise an IP address cannot be typed
  // reliably and Apply may send the previous value.
  if (!artnetFormDirty) {
    artnetHost.value = config.artnet_host;
    universe.value = config.artnet_universe;
    startChannel.value = config.led_start_channel;
    segmentsInput.value = config.segment_count;
  }

  preview.style.gridTemplateColumns = `repeat(${state.preview.length}, minmax(24px, 1fr))`;
  preview.replaceChildren(...state.preview.map((color) => {
    const segment = document.createElement("div");
    segment.className = "segment";
    segment.style.background = rgb(color);
    segment.style.boxShadow = `inset 0 0 18px rgba(255,255,255,.08), 0 0 18px ${rgb(color)}`;
    return segment;
  }));

  document.querySelectorAll("[data-preset]").forEach((button) => {
    button.classList.toggle("active", button.dataset.preset === config.preset);
  });

  if (!colorPalettesBuilt && state.color_palettes) {
    colorPalettesBuilt = true;
    colorPalettesBox.replaceChildren(...Object.entries(state.color_palettes).map(([key, palette]) => {
      const row = document.createElement("label");
      row.className = "effect-toggle palette-toggle";
      const checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.dataset.colorPalette = key;
      checkbox.addEventListener("change", () => {
        send({ action: "toggle_color_palette", palette: key, enabled: checkbox.checked });
      });
      const text = document.createElement("span");
      const paletteName = typeof palette === "string" ? palette : palette.name;
      const paletteColors = typeof palette === "string" ? [] : (palette.colors || []);
      text.textContent = `${paletteName} · ${paletteColors.map(colorName).join(" / ")}`;
      row.append(checkbox, text);
      return row;
    }));
  }

  document.querySelectorAll("[data-color-palette]").forEach((checkbox) => {
    checkbox.checked = (config.enabled_color_palettes || []).includes(checkbox.dataset.colorPalette);
    checkbox.closest(".palette-toggle")?.classList.toggle("active", checkbox.checked);
  });

  if (!motionBuilt && state.motion_modes) {
    motionBuilt = true;
    motionBox.replaceChildren(...Object.entries(state.motion_modes).map(([key, label]) => {
      const button = document.createElement("button");
      button.type = "button";
      button.dataset.motionMode = key;
      button.textContent = label;
      button.addEventListener("click", () => send({ action: "set_motion_mode", motion_mode: key }));
      return button;
    }));
  }

  document.querySelectorAll("[data-motion-mode]").forEach((button) => {
    button.classList.toggle("active", button.dataset.motionMode === config.motion_mode);
  });

  if (!motionScenesBuilt && state.motion_scenes) {
    motionScenesBuilt = true;
    motionScenesBox.replaceChildren(...Object.entries(state.motion_scenes).map(([key, label]) => {
      const row = document.createElement("label");
      row.className = "effect-toggle motion-toggle";
      const checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.dataset.motionScene = key;
      checkbox.addEventListener("change", () => {
        send({ action: "toggle_motion_scene", scene: key, enabled: checkbox.checked });
      });
      const text = document.createElement("span");
      text.textContent = label;
      row.append(checkbox, text);
      return row;
    }));
  }

  document.querySelectorAll("[data-motion-scene]").forEach((checkbox) => {
    checkbox.checked = (config.enabled_motion_scenes || []).includes(checkbox.dataset.motionScene);
    checkbox.closest(".motion-toggle")?.classList.toggle("active", checkbox.checked);
  });

  if (!effectsBuilt) {
    effectsBuilt = true;
    effectsBox.replaceChildren(...Object.entries(state.effects).map(([key, label]) => {
      const row = document.createElement("label");
      row.className = "effect-toggle";
      const checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.dataset.effect = key;
      checkbox.addEventListener("change", () => {
        send({ action: "toggle_effect", effect: key, enabled: checkbox.checked });
      });
      const text = document.createElement("span");
      text.textContent = label;
      row.append(checkbox, text);
      return row;
    }));
  }

  document.querySelectorAll("[data-effect]").forEach((checkbox) => {
    checkbox.checked = config.enabled_effects.includes(checkbox.dataset.effect);
    checkbox.closest(".effect-toggle")?.classList.toggle("active", checkbox.checked);
  });

  document.querySelectorAll("[data-layer]").forEach((checkbox) => {
    checkbox.checked = Boolean(config.layers && config.layers[checkbox.dataset.layer]);
  });

  document.querySelectorAll("[data-arm]").forEach((checkbox) => {
    const fixture = checkbox.dataset.arm;
    checkbox.checked = Boolean(config.fixtures && config.fixtures[fixture] && config.fixtures[fixture].armed);
  });

  if (!rigBuilt) {
    rigBuilt = true;
    buildRig(config);
  } else {
    syncRig(config);
  }
}

function fixtureCard(title, fixture, index, start) {
  const card = document.createElement("div");
  card.className = "fixture-card";
  const name = document.createElement("strong");
  name.textContent = title;
  const label = document.createElement("label");
  const span = document.createElement("span");
  span.textContent = "DMX Start";
  const input = document.createElement("input");
  input.type = "number";
  input.min = "1";
  input.max = "512";
  input.value = start;
  input.dataset.fixture = fixture;
  input.dataset.index = String(index);
  input.addEventListener("change", () => {
    send({
      action: "set_fixture_address",
      fixture,
      index,
      start: Number(input.value)
    });
  });
  label.append(span, input);
  card.append(name, label);
  return card;
}

function buildRig(config) {
  const fixtures = config.fixtures || {};
  const cards = [];
  (fixtures.led_bars || []).forEach((bar, index) => {
    cards.push(fixtureCard(bar.name || `LED Bar ${index + 1}`, "led_bars", index, bar.start));
  });
  (fixtures.rgb_pars || []).forEach((par, index) => {
    cards.push(fixtureCard(par.name || `RGB PAR ${index + 1}`, "rgb_pars", index, par.start));
  });
  (fixtures.moving_heads || []).forEach((head, index) => {
    cards.push(fixtureCard(head.name || `MH ${index + 1}`, "moving_heads", index, head.start));
  });
  if (fixtures.strobe) {
    cards.push(fixtureCard(fixtures.strobe.name || "Strobe", "strobe", 0, fixtures.strobe.start));
  }
  if (fixtures.fog) {
    cards.push(fixtureCard(fixtures.fog.name || "Nebel", "fog", 0, fixtures.fog.start));
  }
  rigBox.replaceChildren(...cards);
}

function syncRig(config) {
  const fixtures = config.fixtures || {};
  document.querySelectorAll("[data-fixture]").forEach((input) => {
    const fixture = input.dataset.fixture;
    const index = Number(input.dataset.index || 0);
    let start = null;
    if (fixture === "led_bars") {
      start = fixtures.led_bars && fixtures.led_bars[index] && fixtures.led_bars[index].start;
    } else if (fixture === "rgb_pars") {
      start = fixtures.rgb_pars && fixtures.rgb_pars[index] && fixtures.rgb_pars[index].start;
    } else if (fixture === "moving_heads") {
      start = fixtures.moving_heads && fixtures.moving_heads[index] && fixtures.moving_heads[index].start;
    } else if (fixtures[fixture]) {
      start = fixtures[fixture].start;
    }
    if (start !== null && document.activeElement !== input) {
      input.value = start;
    }
  });
}

function queueSlider(payload) {
  clearTimeout(pendingSlider);
  pendingSlider = setTimeout(() => send(payload), 80);
}

mood.addEventListener("input", () => {
  moodValue.textContent = mood.value;
  queueSlider({ action: "set_mood", mood: Number(mood.value), custom: true });
});

master.addEventListener("input", () => {
  masterValue.textContent = master.value;
  queueSlider({ action: "set_master", master: Number(master.value) / 100 });
});

ledMaster.addEventListener("input", () => {
  ledMasterValue.textContent = ledMaster.value;
  queueSlider({ action: "set_output_master", target: "led_master", value: Number(ledMaster.value) / 100 });
});

motionMaster.addEventListener("input", () => {
  motionMasterValue.textContent = motionMaster.value;
  queueSlider({ action: "set_output_master", target: "motion_master", value: Number(motionMaster.value) / 100 });
});

parZoneMaster.addEventListener("input", () => {
  parZoneMasterValue.textContent = parZoneMaster.value;
  queueSlider({ action: "set_output_master", target: "rgb_par_master", value: Number(parZoneMaster.value) / 100 });
});

ledEnabled.addEventListener("change", () => {
  send({ action: "set_layer", layer: "led_bars", enabled: ledEnabled.checked });
});

ledBeatPulse.addEventListener("change", () => {
  send({ action: "set_beat_pulse", target: "led", enabled: ledBeatPulse.checked });
});

function sendParZone() {
  send({
    action: "set_rgb_par_zone",
    linked: parZoneLinked.checked,
    mood: Number(parZoneMood.value),
    scene: parZoneScene.value || "auto"
  });
}

parZoneLinked.addEventListener("change", sendParZone);
parZoneScene.addEventListener("change", sendParZone);
parZoneMood.addEventListener("input", () => {
  parZoneMoodValue.textContent = parZoneMood.value;
  queueSlider({
    action: "set_rgb_par_zone",
    linked: parZoneLinked.checked,
    mood: Number(parZoneMood.value),
    scene: parZoneScene.value || "auto"
  });
});

motionEnabled.addEventListener("change", () => {
  send({ action: "set_layer", layer: "motion", enabled: motionEnabled.checked });
});

motionBeatPulse.addEventListener("change", () => {
  send({ action: "set_beat_pulse", target: "motion", enabled: motionBeatPulse.checked });
});

strobeArmed.addEventListener("change", () => {
  send({ action: "set_fixture_armed", fixture: "strobe", armed: strobeArmed.checked });
});

strobeBeatPulse.addEventListener("change", () => {
  send({ action: "set_strobe_beat_pulse", enabled: strobeBeatPulse.checked });
});

strobeMaster.addEventListener("input", () => {
  strobeMasterValue.textContent = strobeMaster.value;
  queueSlider({ action: "set_strobe_master", value: Number(strobeMaster.value) / 100 });
});

strobeSpeed.addEventListener("input", () => {
  strobeSpeedValue.textContent = strobeSpeed.value;
  queueSlider({ action: "set_strobe_speed", value: Number(strobeSpeed.value) / 100 });
});

function sendGoboControl() {
  send({
    action: "set_gobo_control",
    enabled: goboEnabled.checked,
    mode: goboMode.value,
    selected_gobo: goboSelect.value,
    highpoint_only: goboHighpointOnly.checked,
    shake_enabled: goboShakeEnabled.checked,
    shake_mood_threshold: Number(goboShakeMood.value) / 100,
    fast_peak_enabled: goboFastPeak.checked
  });
}

goboEnabled.addEventListener("change", sendGoboControl);
goboMode.addEventListener("change", sendGoboControl);
goboSelect.addEventListener("change", sendGoboControl);
goboHighpointOnly.addEventListener("change", sendGoboControl);
goboFastPeak.addEventListener("change", sendGoboControl);
goboShakeEnabled.addEventListener("change", sendGoboControl);
goboShakeMood.addEventListener("input", () => {
  goboShakeMoodValue.textContent = goboShakeMood.value;
  queueSlider({
    action: "set_gobo_control",
    enabled: goboEnabled.checked,
    mode: goboMode.value,
    selected_gobo: goboSelect.value,
    highpoint_only: goboHighpointOnly.checked,
    shake_enabled: goboShakeEnabled.checked,
    shake_mood_threshold: Number(goboShakeMood.value) / 100,
    fast_peak_enabled: goboFastPeak.checked
  });
});

function sendColorWheel() {
  send({
    action: "set_color_wheel",
    enabled: colorWheelEnabled.checked,
    use_raw_value: colorWheelUseRaw.checked,
    selected_color: colorWheelSelect.value,
    raw_value: Number(colorWheelRaw.value)
  });
}

colorWheelEnabled.addEventListener("change", sendColorWheel);
colorWheelSelect.addEventListener("change", sendColorWheel);
colorWheelUseRaw.addEventListener("change", sendColorWheel);
colorWheelRaw.addEventListener("input", () => {
  colorWheelRawValue.textContent = colorWheelRaw.value;
  queueSlider({
    action: "set_color_wheel",
    enabled: colorWheelEnabled.checked,
    use_raw_value: colorWheelUseRaw.checked,
    selected_color: colorWheelSelect.value,
    raw_value: Number(colorWheelRaw.value)
  });
});

runButton.addEventListener("click", () => {
  send({ action: "set_running", running: !(lastState && lastState.running) });
});

document.querySelectorAll("[data-trigger]").forEach((button) => {
  button.addEventListener("click", () => send({ action: "trigger", name: button.dataset.trigger }));
});

document.querySelectorAll("[data-hold-trigger]").forEach((button) => {
  const name = button.dataset.holdTrigger;
  const release = () => {
    button.classList.remove("active");
    send({ action: "set_hold_trigger", name, held: false });
  };
  button.addEventListener("pointerdown", (event) => {
    event.preventDefault();
    button.setPointerCapture(event.pointerId);
    button.classList.add("active");
    send({ action: "set_hold_trigger", name, held: true });
  });
  button.addEventListener("pointerup", release);
  button.addEventListener("pointercancel", release);
  button.addEventListener("lostpointercapture", () => {
    if (button.classList.contains("active")) release();
  });
});

document.querySelectorAll("[data-layer]").forEach((checkbox) => {
  checkbox.addEventListener("change", () => {
    send({ action: "set_layer", layer: checkbox.dataset.layer, enabled: checkbox.checked });
  });
});

document.querySelectorAll("[data-arm]").forEach((checkbox) => {
  checkbox.addEventListener("change", () => {
    send({ action: "set_fixture_armed", fixture: checkbox.dataset.arm, armed: checkbox.checked });
  });
});

document.querySelectorAll("[data-preset]").forEach((button) => {
  button.addEventListener("click", () => send({ action: "apply_preset", preset: button.dataset.preset }));
});

function syncDiscoCalibrationControls() {
  if (!discoCalibrationDraft) return;
  discoHead.value = String(discoCalibrationDraft.selected_head);
  const headIndex = Math.max(0, Math.min(3, discoCalibrationDraft.selected_head));
  discoTilt.value = Math.round(discoCalibrationDraft.tilt * 1000);
  discoTiltValue.textContent = discoTilt.value;
  discoPan.value = Math.round(discoCalibrationDraft.pans[headIndex] * 1000);
  discoPanValue.textContent = discoPan.value;
}

function discoCalibrationPayload(adjustAllTilt = false) {
  return {
    action: "set_disco_ball_calibration",
    test_mode: Boolean(discoCalibrationDraft.test_mode),
    selected_head: discoCalibrationDraft.selected_head,
    adjust_all_tilt: adjustAllTilt,
    tilt: discoCalibrationDraft.tilt,
    pan_1: discoCalibrationDraft.pans[0],
    pan_2: discoCalibrationDraft.pans[1],
    pan_3: discoCalibrationDraft.pans[2],
    pan_4: discoCalibrationDraft.pans[3]
  };
}

async function applyDiscoCalibration(markSaved = false, adjustAllTilt = false) {
  try {
    await send(discoCalibrationPayload(adjustAllTilt));
    if (markSaved) {
      discoCalibrationDirty = false;
      discoCalibrationStatus.textContent = "Positionen übernommen";
    } else {
      discoCalibrationStatus.textContent = discoCalibrationDraft.test_mode
        ? "Live-Test aktiv · Änderungen werden langsam angefahren"
        : "Test beendet";
    }
  } catch (error) {
    discoCalibrationStatus.textContent = "Übernahme fehlgeschlagen";
    console.error("Disco ball calibration could not be applied", error);
  }
}

function queueDiscoCalibration(adjustAllTilt) {
  clearTimeout(discoCalibrationTimer);
  if (discoCalibrationDraft.test_mode) {
    discoCalibrationTimer = setTimeout(() => applyDiscoCalibration(false, adjustAllTilt), 90);
  }
}

discoTest.addEventListener("click", () => {
  discoCalibrationDraft.test_mode = !discoCalibrationDraft.test_mode;
  discoCalibrationDirty = true;
  syncDiscoCalibrationControls();
  applyDiscoCalibration(false, true);
});

discoTilt.addEventListener("input", () => {
  discoCalibrationDraft.tilt = Number(discoTilt.value) / 1000;
  discoTiltValue.textContent = discoTilt.value;
  discoCalibrationDirty = true;
  queueDiscoCalibration(true);
});

discoHead.addEventListener("change", () => {
  discoCalibrationDraft.selected_head = Math.max(0, Math.min(3, Number(discoHead.value || 0)));
  discoCalibrationDirty = true;
  syncDiscoCalibrationControls();
  if (discoCalibrationDraft.test_mode) applyDiscoCalibration(false, false);
});

discoPan.addEventListener("input", () => {
  const headIndex = Math.max(0, Math.min(3, Number(discoHead.value || 0)));
  discoCalibrationDraft.pans[headIndex] = Number(discoPan.value) / 1000;
  discoPanValue.textContent = discoPan.value;
  discoCalibrationDirty = true;
  queueDiscoCalibration(false);
});

document.querySelector("#saveDiscoCalibration").addEventListener("click", () => applyDiscoCalibration(true));

document.querySelector("#exportDiscoCalibration").addEventListener("click", () => {
  const calibration = {
    schema: "lightengine.disco_ball_calibration.v1",
    tilt: discoCalibrationDraft.tilt,
    pans: discoCalibrationDraft.pans
  };
  const blob = new Blob([`${JSON.stringify(calibration, null, 2)}\n`], { type: "application/json" });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = "disco-ball-calibration.json";
  link.click();
  URL.revokeObjectURL(link.href);
  discoCalibrationStatus.textContent = "Kalibrierung als JSON exportiert";
});

document.querySelector("#importDiscoCalibration").addEventListener("click", () => discoCalibrationFile.click());

discoCalibrationFile.addEventListener("change", async () => {
  const file = discoCalibrationFile.files && discoCalibrationFile.files[0];
  if (!file) return;
  try {
    const imported = JSON.parse(await file.text());
    if (imported.schema !== "lightengine.disco_ball_calibration.v1" ||
        !Array.isArray(imported.pans) || imported.pans.length !== 4 ||
        !Number.isFinite(Number(imported.tilt)) || imported.pans.some((value) => !Number.isFinite(Number(value)))) {
      throw new Error("Ungültiges Kalibrierungsformat");
    }
    discoCalibrationDraft.tilt = Math.max(0, Math.min(1, Number(imported.tilt)));
    discoCalibrationDraft.pans = imported.pans.map((value) => Math.max(0, Math.min(1, Number(value))));
    discoCalibrationDirty = true;
    syncDiscoCalibrationControls();
    await applyDiscoCalibration(true, true);
    discoCalibrationStatus.textContent = "JSON geladen und Positionen übernommen";
  } catch (error) {
    discoCalibrationStatus.textContent = `JSON konnte nicht geladen werden: ${error.message}`;
  } finally {
    discoCalibrationFile.value = "";
  }
});

const artnetInputs = [artnetHost, universe, startChannel, segmentsInput];
artnetInputs.forEach((input) => {
  input.addEventListener("input", () => {
    artnetFormDirty = true;
  });
});

document.querySelector("#applyArtnet").addEventListener("click", async () => {
  const payload = {
    action: "set_artnet",
    artnet_host: artnetHost.value.trim(),
    artnet_universe: Number(universe.value),
    led_start_channel: Number(startChannel.value),
    segment_count: Number(segmentsInput.value)
  };

  try {
    await send(payload);
    artnetFormDirty = false;
    render(lastState);
  } catch (error) {
    console.error("ArtNet settings could not be applied", error);
  }
});

loadState();
setInterval(loadState, 250);
