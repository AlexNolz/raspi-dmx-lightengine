const stateUrl = "/api/state";
const controlUrl = "/api/control";

let lastState = null;
let effectsBuilt = false;
let motionBuilt = false;
let motionScenesBuilt = false;
let goboModesBuilt = false;
let gobosBuilt = false;
let movingHeadColorsBuilt = false;
let rigBuilt = false;
let pendingSlider = null;

const preview = document.querySelector("#preview");
const os2lStatus = document.querySelector("#os2lStatus");
const bpmStatus = document.querySelector("#bpmStatus");
const effectStatus = document.querySelector("#effectStatus");
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
const motionEnabled = document.querySelector("#motionEnabled");
const strobeArmed = document.querySelector("#strobeArmed");
const strobeBeatPulse = document.querySelector("#strobeBeatPulse");
const strobeMaster = document.querySelector("#strobeMaster");
const strobeMasterValue = document.querySelector("#strobeMasterValue");
const strobeSpeed = document.querySelector("#strobeSpeed");
const strobeSpeedValue = document.querySelector("#strobeSpeedValue");
const runButton = document.querySelector("#runButton");
const blackoutButton = document.querySelector("#blackoutButton");
const effectsBox = document.querySelector("#effects");
const motionBox = document.querySelector("#motionModes");
const motionScenesBox = document.querySelector("#motionScenes");
const goboEnabled = document.querySelector("#goboEnabled");
const goboMode = document.querySelector("#goboMode");
const goboSelect = document.querySelector("#goboSelect");
const goboHighpointOnly = document.querySelector("#goboHighpointOnly");
const goboShakeEnabled = document.querySelector("#goboShakeEnabled");
const goboShakeMood = document.querySelector("#goboShakeMood");
const goboShakeMoodValue = document.querySelector("#goboShakeMoodValue");
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

function render(state) {
  const config = state.config;
  const artnetTarget = config.artnet_host === "127.0.0.1"
    ? `DMX Bridge lokal / U${config.artnet_universe} / CH${config.led_start_channel}`
    : `ArtNet ${config.artnet_host} / U${config.artnet_universe} / CH${config.led_start_channel}`;
  os2lStatus.textContent = state.os2l_connected ? "OS2L live" : "OS2L wartet";
  os2lStatus.className = state.os2l_connected ? "status good" : "status bad";
  bpmStatus.textContent = `${state.bpm.toFixed(1)} BPM`;
  effectStatus.textContent = state.active_effect_label;
  targetStatus.textContent = artnetTarget;

  mood.value = config.mood;
  moodValue.textContent = config.mood;
  master.value = Math.round(config.master * 100);
  masterValue.textContent = Math.round(config.master * 100);
  ledMaster.value = Math.round((config.led_master ?? 1) * 100);
  ledMasterValue.textContent = Math.round((config.led_master ?? 1) * 100);
  motionMaster.value = Math.round((config.motion_master ?? 1) * 100);
  motionMasterValue.textContent = Math.round((config.motion_master ?? 1) * 100);
  const strobeConfig = (config.fixtures && config.fixtures.strobe) || {};
  ledEnabled.checked = Boolean(config.layers && config.layers.led_bars);
  motionEnabled.checked = Boolean(config.layers && config.layers.motion);
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
  }
  goboEnabled.checked = Boolean(goboConfig.enabled);
  goboMode.value = goboConfig.mode || "beat_step";
  goboSelect.value = goboConfig.selected_gobo || "open";
  goboHighpointOnly.checked = Boolean(goboConfig.highpoint_only);
  goboShakeEnabled.checked = Boolean(goboConfig.shake_enabled);
  goboShakeMood.value = Math.round((goboConfig.shake_mood_threshold ?? 0.62) * 100);
  goboShakeMoodValue.textContent = goboShakeMood.value;
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
  colorWheelRaw.value = colorWheelConfig.raw_value ?? 3;
  colorWheelRawValue.textContent = colorWheelRaw.value;
  runButton.textContent = state.running ? "Running" : "Stopped";
  runButton.classList.toggle("active", state.running);
  blackoutButton.classList.toggle("active", state.blackout);

  artnetHost.value = config.artnet_host;
  universe.value = config.artnet_universe;
  startChannel.value = config.led_start_channel;
  segmentsInput.value = config.segment_count;

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

ledEnabled.addEventListener("change", () => {
  send({ action: "set_layer", layer: "led_bars", enabled: ledEnabled.checked });
});

motionEnabled.addEventListener("change", () => {
  send({ action: "set_layer", layer: "motion", enabled: motionEnabled.checked });
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
    shake_mood_threshold: Number(goboShakeMood.value) / 100
  });
}

goboEnabled.addEventListener("change", sendGoboControl);
goboMode.addEventListener("change", sendGoboControl);
goboSelect.addEventListener("change", sendGoboControl);
goboHighpointOnly.addEventListener("change", sendGoboControl);
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
    shake_mood_threshold: Number(goboShakeMood.value) / 100
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

document.querySelector("#applyArtnet").addEventListener("click", () => {
  send({
    action: "set_artnet",
    artnet_host: artnetHost.value.trim(),
    artnet_universe: Number(universe.value),
    led_start_channel: Number(startChannel.value),
    segment_count: Number(segmentsInput.value)
  });
});

loadState();
setInterval(loadState, 250);
