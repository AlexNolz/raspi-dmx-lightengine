from __future__ import annotations

import argparse
import colorsys
import copy
import json
import math
import random
import socket
import struct
import threading
import time
from dataclasses import dataclass, field
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


APP_DIR = Path(__file__).resolve().parent
WEB_DIR = APP_DIR / "web"
CONFIG_PATH = APP_DIR / "light_engine_config.json"

PURE_COLOR_BANK: list[tuple[int, int, int]] = [
    (255, 0, 0),
    (0, 255, 0),
    (0, 55, 255),
    (0, 220, 255),
    (255, 0, 220),
    (255, 190, 0),
]

PRESET_COLOR_SETS: dict[str, list[tuple[int, int, int]]] = {
    "lounge": [
        (0, 80, 255),
        (0, 210, 255),
        (150, 0, 255),
        (255, 70, 0),
    ],
    "club": [
        (255, 0, 0),
        (0, 255, 0),
        (0, 55, 255),
        (0, 220, 255),
        (255, 0, 220),
        (255, 190, 0),
    ],
    "rave": [
        (255, 0, 0),
        (0, 255, 0),
        (0, 55, 255),
        (255, 0, 220),
        (0, 220, 255),
        (255, 190, 0),
    ],
    "game_show": [
        (0, 70, 255),
        (255, 0, 0),
        (255, 190, 0),
        (0, 255, 70),
    ],
    "rgb_hard": [
        (255, 0, 0),
        (0, 255, 0),
        (0, 0, 255),
        (255, 0, 0),
        (0, 255, 0),
        (0, 0, 255),
    ],
}

DEFAULT_CONFIG: dict[str, Any] = {
    "web_host": "127.0.0.1",
    "web_port": 8088,
    "os2l_host": "127.0.0.1",
    "os2l_port": 9996,
    "artnet_host": "192.168.137.255",
    "artnet_port": 6454,
    "artnet_universe": 0,
    "led_start_channel": 3,
    "segment_count": 16,
    "fps": 40,
    "master": 1.0,
    "led_master": 1.0,
    "motion_master": 1.0,
    "mood": 58,
    "preset": "club",
    "motion_mode": "auto",
    "enabled_motion_scenes": [
        "center_pulse",
        "point_chase",
        "line_sweep",
        "depth_sweep",
        "cross_pairs",
        "split_strobe",
        "corner_swap",
        "gobo_chase",
        "side_pingpong",
        "x_cross",
        "color_fan",
        "pair_orbit",
        "pair_random",
    ],
    "layers": {
        "led_bars": True,
        "motion": True,
        "strobe": False,
        "fog": False,
    },
    "fixtures": {
        "led_bars": [
            {"name": "LED Bar 1", "start": 3, "segments": 8, "enabled": True},
            {"name": "LED Bar 2", "start": 27, "segments": 8, "enabled": True},
        ],
        "strobe": {
            "name": "Stairville 1500W Strobe",
            "start": 1,
            "channels": 2,
            "enabled": True,
            "armed": False,
            "beat_pulse": False,
            "master": 1.0,
            "speed": 1.0,
        },
        "fog": {
            "name": "Stairville AF-40 DMX Fog",
            "start": 95,
            "channels": 1,
            "enabled": True,
            "armed": False,
        },
        "moving_heads": [
            {"name": "MH 1", "profile": "zkymzl11", "start": 51, "channels": 11, "enabled": True, "invert_pan": False, "invert_tilt": False, "pan_min": 0.14, "pan_max": 0.54, "pan_center": 0.333, "pan_width": 0.30, "tilt_min": 0.56, "tilt_max": 0.82},
            {"name": "MH 2", "profile": "zkymzl11", "start": 62, "channels": 11, "enabled": True, "invert_pan": False, "invert_tilt": False, "pan_min": 0.14, "pan_max": 0.54, "pan_center": 0.333, "pan_width": 0.30, "tilt_min": 0.56, "tilt_max": 0.82},
            {"name": "MH 3", "profile": "zkymzl11", "start": 73, "channels": 11, "enabled": True, "invert_pan": False, "invert_tilt": False, "pan_min": 0.14, "pan_max": 0.54, "pan_center": 0.333, "pan_width": 0.30, "tilt_min": 0.56, "tilt_max": 0.82},
            {"name": "MH 4", "profile": "zkymzl11", "start": 84, "channels": 11, "enabled": True, "invert_pan": False, "invert_tilt": False, "pan_min": 0.14, "pan_max": 0.54, "pan_center": 0.333, "pan_width": 0.30, "tilt_min": 0.56, "tilt_max": 0.82},
        ],
    },
    "enabled_effects": [
        "pulse",
        "ball",
        "pair_swap",
        "rainbow",
        "scanner",
        "sparkle",
        "split",
        "blocks",
        "comet",
        "theater",
        "zipper",
        "orbit",
        "gate",
        "binary",
        "fill",
    ],
}

EFFECTS: dict[str, str] = {
    "pulse": "Beat Pulse",
    "ball": "Game Ball",
    "pair_swap": "Pair Swap",
    "rainbow": "Rainbow Chase",
    "scanner": "Scanner",
    "sparkle": "Sparkle",
    "split": "Split Crowd",
    "blocks": "Color Blocks",
    "strobe": "Color Strobe",
    "breathe": "Soft Breathe",
    "comet": "RGB Comet",
    "theater": "Theater Dots",
    "zipper": "Zipper",
    "orbit": "Dual Orbit",
    "traffic": "Traffic Blocks",
    "gate": "Beat Gate",
    "binary": "Binary Pixels",
    "fill": "Fill Chase",
    "siren": "Red Blue Siren",
}

PRESETS: dict[str, dict[str, Any]] = {
    "lounge": {
        "mood": 24,
        "effects": ["breathe", "pulse", "ball", "pair_swap", "comet", "fill"],
        "motion_scenes": ["center", "center_pulse", "depth_sweep", "pair_orbit"],
    },
    "club": {
        "mood": 58,
        "effects": [
            "pulse",
            "ball",
            "pair_swap",
            "rainbow",
            "scanner",
            "sparkle",
            "split",
            "blocks",
            "comet",
            "theater",
            "zipper",
            "orbit",
            "binary",
            "fill",
        ],
        "motion_scenes": ["center_pulse", "point_chase", "line_sweep", "depth_sweep", "cross_pairs", "corner_swap", "color_fan", "pair_orbit"],
    },
    "rave": {
        "mood": 88,
        "effects": [
            "pulse",
            "ball",
            "rainbow",
            "scanner",
            "sparkle",
            "split",
            "blocks",
            "strobe",
            "comet",
            "theater",
            "zipper",
            "orbit",
            "traffic",
            "gate",
            "binary",
            "siren",
        ],
        "motion_scenes": ["split_strobe", "corner_swap", "gobo_chase", "side_pingpong", "x_cross", "color_fan", "pair_orbit", "all_random", "pair_random", "duo_random"],
    },
    "game_show": {
        "mood": 68,
        "effects": [
            "ball",
            "scanner",
            "blocks",
            "pair_swap",
            "split",
            "zipper",
            "theater",
            "traffic",
            "fill",
            "siren",
        ],
        "motion_scenes": ["center", "front", "left", "right", "split_strobe", "corner_swap", "gobo_chase", "side_pingpong", "x_cross"],
    },
    "rgb_hard": {
        "mood": 78,
        "effects": [
            "pulse",
            "scanner",
            "blocks",
            "comet",
            "theater",
            "gate",
            "binary",
            "fill",
        ],
        "motion_scenes": ["split_strobe", "gobo_chase", "side_pingpong", "x_cross", "color_fan", "pair_random"],
    },
    "custom": {
        "mood": None,
        "effects": None,
        "motion_scenes": None,
    },
}

MOTION_MODES: dict[str, str] = {
    "auto": "Auto",
    "center": "Mitte",
    "front": "Vorne",
    "back": "Hinten",
    "left": "Links",
    "right": "Rechts",
    "point_chase": "Punkt Chase",
    "line_sweep": "Links/Rechts Sweep",
    "depth_sweep": "Vorne/Hinten Sweep",
    "cross_pairs": "2 Links / 2 Rechts",
    "split_strobe": "Links/Rechts Strobe",
    "corner_swap": "Ecken Wechsel",
    "center_pulse": "Mitte Pulse",
    "gobo_chase": "Gobo Chase",
    "side_pingpong": "Side Ping Pong",
    "x_cross": "X Cross",
    "color_fan": "Color Fan",
    "pair_orbit": "Pair Orbit",
    "all_random": "Alle Random",
    "pair_random": "Paare Random",
    "duo_random": "2 Gemeinsam + 2 Random",
}

MOTION_POINTS: dict[str, tuple[float, float]] = {
    "center": (0.50, 0.54),
    "front": (0.50, 0.90),
    "back": (0.50, 0.18),
    "left": (0.18, 0.55),
    "right": (0.82, 0.55),
    "front_left": (0.22, 0.86),
    "front_right": (0.78, 0.86),
    "back_left": (0.22, 0.24),
    "back_right": (0.78, 0.24),
}

FLOOR_FORWARD_OFFSET = 0.11

RANDOM_MOTION_POINTS = [
    "center",
    "front",
    "back",
    "left",
    "right",
    "front_left",
    "front_right",
    "back_left",
    "back_right",
]


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * clamp(t, 0.0, 1.0)


def mix_rgb(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return (
        int(lerp(a[0], b[0], t)),
        int(lerp(a[1], b[1], t)),
        int(lerp(a[2], b[2], t)),
    )


def scale_rgb(color: tuple[int, int, int], amount: float) -> tuple[int, int, int]:
    amount = clamp(amount, 0.0, 1.5)
    return (
        int(clamp(color[0] * amount, 0, 255)),
        int(clamp(color[1] * amount, 0, 255)),
        int(clamp(color[2] * amount, 0, 255)),
    )


def add_rgb(a: tuple[int, int, int], b: tuple[int, int, int]) -> tuple[int, int, int]:
    return (
        min(255, a[0] + b[0]),
        min(255, a[1] + b[1]),
        min(255, a[2] + b[2]),
    )


def hsv_rgb(h: float, s: float, v: float) -> tuple[int, int, int]:
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, clamp(s, 0, 1), clamp(v, 0, 1))
    return (int(r * 255), int(g * 255), int(b * 255))


def clean_rgb(color: tuple[int, int, int]) -> tuple[int, int, int]:
    channels = [int(clamp(channel, 0, 255)) for channel in color]
    peak = max(channels)
    if peak <= 0:
        return (0, 0, 0)

    order = sorted(range(3), key=lambda index: channels[index], reverse=True)
    clean = [0, 0, 0]
    clean[order[0]] = peak

    second = channels[order[1]]
    if second >= peak * 0.22:
        clean[order[1]] = min(second, int(peak * 0.82))

    return (clean[0], clean[1], clean[2])


def color_mix(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return clean_rgb(mix_rgb(a, b, t))


def color_add(a: tuple[int, int, int], b: tuple[int, int, int]) -> tuple[int, int, int]:
    return clean_rgb(add_rgb(a, b))


TMH17_COLOR_WHEEL: list[tuple[tuple[int, int, int], int]] = [
    ((255, 255, 255), 0),
    ((255, 0, 0), 14),
    ((0, 255, 0), 24),
    ((0, 0, 255), 34),
    ((255, 220, 0), 44),
    ((0, 190, 255), 54),
    ((255, 120, 0), 64),
    ((255, 0, 220), 74),
]

ZKYMZL_COLOR_WHEEL: list[tuple[tuple[int, int, int], int]] = [
    ((255, 255, 255), 3),
    ((255, 0, 0), 11),
    ((0, 220, 255), 18),
    ((255, 120, 0), 25),
    ((0, 0, 255), 70),
    ((255, 220, 0), 80),
    ((0, 255, 0), 90),
    ((180, 0, 255), 100),
    ((255, 0, 220), 100),
]


def tmh17_color_wheel_value(color: tuple[int, int, int]) -> int:
    return closest_color_wheel_value(color, TMH17_COLOR_WHEEL)


def zkymzl_color_wheel_value(color: tuple[int, int, int]) -> int:
    return closest_color_wheel_value(color, ZKYMZL_COLOR_WHEEL)


def closest_color_wheel_value(
    color: tuple[int, int, int],
    wheel: list[tuple[tuple[int, int, int], int]],
) -> int:
    if max(color) < 12:
        return 0
    best_value = 0
    best_distance = float("inf")
    for target, value in wheel:
        distance = sum((color[channel] - target[channel]) ** 2 for channel in range(3))
        if distance < best_distance:
            best_distance = distance
            best_value = value
    return best_value


def brightest_color(colors: list[tuple[int, int, int]], fallback: tuple[int, int, int]) -> tuple[int, int, int]:
    usable = [color for color in colors if max(color) > 8]
    return max(usable, key=lambda color: sum(color), default=fallback)


def led_color_at(
    colors: list[tuple[int, int, int]],
    x_position: float,
    fallback: tuple[int, int, int],
) -> tuple[int, int, int]:
    if not colors:
        return fallback
    index = int(clamp(round(x_position * (len(colors) - 1)), 0, len(colors) - 1))
    color = colors[index]
    return color if max(color) > 8 else fallback


def led_color_range(
    colors: list[tuple[int, int, int]],
    start: float,
    end: float,
    fallback: tuple[int, int, int],
) -> tuple[int, int, int]:
    if not colors:
        return fallback
    low = int(clamp(math.floor(start * len(colors)), 0, len(colors) - 1))
    high = int(clamp(math.ceil(end * len(colors)), low + 1, len(colors)))
    return brightest_color(colors[low:high], fallback)


def deep_merge(default: Any, override: Any) -> Any:
    if isinstance(default, dict) and isinstance(override, dict):
        merged = copy.deepcopy(default)
        for key, value in override.items():
            merged[key] = deep_merge(merged[key], value) if key in merged else copy.deepcopy(value)
        return merged
    return copy.deepcopy(override)


def normalized_config(config: dict[str, Any]) -> dict[str, Any]:
    merged = deep_merge(DEFAULT_CONFIG, config)
    merged["web_port"] = int(merged["web_port"])
    merged["os2l_port"] = int(merged["os2l_port"])
    merged["artnet_port"] = int(merged["artnet_port"])
    merged["artnet_universe"] = int(merged["artnet_universe"])
    merged["led_start_channel"] = int(merged["led_start_channel"])
    merged["segment_count"] = int(merged["segment_count"])
    merged["fps"] = int(merged["fps"])
    merged["master"] = float(clamp(float(merged["master"]), 0.0, 1.0))
    merged["led_master"] = float(clamp(float(merged.get("led_master", 1.0)), 0.0, 1.0))
    merged["motion_master"] = float(clamp(float(merged.get("motion_master", 1.0)), 0.0, 1.0))
    merged["mood"] = int(clamp(int(merged["mood"]), 0, 100))
    merged["enabled_effects"] = [
        effect for effect in merged["enabled_effects"] if effect in EFFECTS
    ] or ["pulse"]
    merged["enabled_motion_scenes"] = [
        scene
        for scene in merged.get("enabled_motion_scenes", [])
        if scene in MOTION_MODES and scene != "auto"
    ] or ["center_pulse"]
    if merged["preset"] not in PRESETS:
        merged["preset"] = "club"
    if merged.get("motion_mode") not in MOTION_MODES:
        merged["motion_mode"] = "auto"
    fixtures = merged.get("fixtures", {})
    for head in fixtures.get("moving_heads", []):
        head["pan_min"] = float(clamp(float(head.get("pan_min", 0.14)), 0.0, 1.0))
        head["pan_max"] = float(clamp(float(head.get("pan_max", 0.54)), 0.0, 1.0))
        head["pan_center"] = float(clamp(float(head.get("pan_center", 0.333)), 0.0, 1.0))
        head["pan_width"] = float(clamp(float(head.get("pan_width", 0.30)), 0.02, 1.0))
        head["tilt_min"] = float(clamp(float(head.get("tilt_min", 0.56)), 0.0, 1.0))
        head["tilt_max"] = float(clamp(float(head.get("tilt_max", 0.82)), 0.0, 1.0))
    led_bars = fixtures.get("led_bars", [])
    if led_bars:
        merged["segment_count"] = sum(
            int(bar.get("segments", 8))
            for bar in led_bars
            if bar.get("enabled", True)
        ) or int(merged["segment_count"])
    return merged


def load_config() -> dict[str, Any]:
    if not CONFIG_PATH.exists():
        return normalized_config({})
    try:
        return normalized_config(json.loads(CONFIG_PATH.read_text(encoding="utf-8")))
    except (OSError, json.JSONDecodeError, ValueError):
        return normalized_config({})


def save_config(config: dict[str, Any]) -> None:
    CONFIG_PATH.write_text(
        json.dumps(config, indent=2, sort_keys=True),
        encoding="utf-8",
    )


@dataclass
class RuntimeState:
    config: dict[str, Any]
    lock: threading.RLock = field(default_factory=threading.RLock)
    stop_event: threading.Event = field(default_factory=threading.Event)
    running: bool = False
    blackout: bool = False
    active_effect: str = "pulse"
    bpm: float = 120.0
    beat_pos: int = 0
    beat_count: int = 0
    last_beat_at: float = field(default_factory=time.time)
    last_os2l_at: float = 0.0
    last_music_beat_at: float = 0.0
    os2l_connected: bool = False
    os2l_connections: int = 0
    artnet_packets: int = 0
    last_error: str = ""
    palette: list[tuple[int, int, int]] = field(default_factory=list)
    preview: list[tuple[int, int, int]] = field(default_factory=list)
    effect_started_beat: int = 0
    next_effect_beat: int = 8
    whiteout_until: float = 0.0
    color_strobe_until: float = 0.0
    strobe_out_until: float = 0.0
    color_strobe_held: bool = False
    strobe_out_held: bool = False
    whiteout_held: bool = False
    blackout_held: bool = False
    reset_until: float = 0.0
    fog_until: float = 0.0
    sparkle_seed: int = 1
    last_show: dict[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        self.palette = self.make_palette()
        self.preview = [(0, 0, 0)] * self.config["segment_count"]

    def set_error(self, message: str) -> None:
        with self.lock:
            self.last_error = message

    def make_palette(self) -> list[tuple[int, int, int]]:
        preset = self.config.get("preset", "club")
        mood = self.config.get("mood", 58) / 100

        if preset == "custom":
            if mood < 0.35:
                colors = PRESET_COLOR_SETS["lounge"]
            elif mood > 0.74:
                colors = PRESET_COLOR_SETS["rave"]
            else:
                colors = PRESET_COLOR_SETS["club"]
        else:
            colors = PRESET_COLOR_SETS.get(preset, PURE_COLOR_BANK)

        rotation = random.randrange(len(colors))
        palette = [colors[(rotation + index) % len(colors)] for index in range(len(colors))]
        return [clean_rgb(color) for color in palette]

    def enabled_effects(self) -> list[str]:
        effects = [effect for effect in self.config["enabled_effects"] if effect in EFFECTS]
        return effects or ["pulse"]

    def choose_effect(self, force: bool = False) -> None:
        effects = self.enabled_effects()
        mood = self.config["mood"]
        current = self.active_effect
        weights: list[float] = []

        for effect in effects:
            weight = 1.0
            if effect in {"strobe", "gate", "siren"}:
                weight = 0.04 + (mood / 100) * 1.35
            elif effect in {"sparkle", "scanner", "blocks", "comet", "theater", "zipper", "orbit", "binary"}:
                weight = 0.6 + (mood / 100) * 1.1
            elif effect in {"fill", "traffic"}:
                weight = 0.85 + (mood / 100) * 0.55
            elif effect == "breathe":
                weight = 1.8 - (mood / 100) * 1.4
            if effect == current and not force:
                weight *= 0.28
            weights.append(max(0.05, weight))

        self.active_effect = random.choices(effects, weights=weights, k=1)[0]
        self.palette = self.make_palette()
        self.effect_started_beat = self.beat_count
        min_beats = int(lerp(16, 4, self.config["mood"] / 100))
        jitter = random.randint(0, max(2, min_beats // 2))
        self.next_effect_beat = self.beat_count + min_beats + jitter
        self.sparkle_seed = random.randint(1, 999999)

    def on_beat(self, payload: dict[str, Any], from_os2l: bool = True) -> None:
        with self.lock:
            now = time.time()
            self.last_beat_at = now
            self.beat_count += 1
            self.beat_pos = int(payload.get("pos", self.beat_pos + 1))
            bpm = payload.get("bpm")
            if isinstance(bpm, (int, float)) and 20 <= float(bpm) <= 260:
                self.bpm = float(bpm)
            if from_os2l:
                self.last_os2l_at = now
                self.last_music_beat_at = now
                self.os2l_connected = True

            is_change = bool(payload.get("change", False))
            on_bar = self.beat_count % 4 == 1
            should_change = is_change or (on_bar and self.beat_count >= self.next_effect_beat)
            if should_change:
                self.choose_effect(force=is_change)

    def on_os2l_button(self, payload: dict[str, Any]) -> None:
        name = "".join(char for char in str(payload.get("name", "")).lower() if char.isalnum())
        is_on = str(payload.get("state", "on")).lower() in {"on", "1", "true", "down"}
        with self.lock:
            # These names are used by the five VirtualDJ OS2L pad commands.
            if name in {"colorstrobe", "colourstrobe", "strobeoutcolor", "strobeoutcolorbeat", "strobe"}:
                self.color_strobe_held = is_on
                if not is_on:
                    self.color_strobe_until = 0.0
            elif name in {"strobeout", "whiteoutstrobe", "whitestrobe", "cmd2"}:
                self.strobe_out_held = is_on
                if not is_on:
                    self.strobe_out_until = 0.0
            elif name in {"whiteout", "whiteflash", "flash", "cmd1"}:
                self.whiteout_held = is_on
                if not is_on:
                    self.whiteout_until = 0.0
            elif name in {"blackout", "black"}:
                self.blackout_held = is_on
            elif not is_on:
                return
            elif name in {"fog", "fogburst", "fogmachine"}:
                fog_cfg = self.config["fixtures"]["fog"]
                if self.config.get("layers", {}).get("fog", False) and fog_cfg.get("armed", False):
                    self.fog_until = time.time() + 1.2
            elif name in {"next", "random", "nextlook"}:
                self.choose_effect(force=True)

    def maybe_internal_beat(self) -> None:
        with self.lock:
            now = time.time()
            if self.last_os2l_at and now - self.last_os2l_at < 3.0:
                return
            period = 60.0 / max(1.0, self.bpm)
            if now - self.last_beat_at >= period:
                self.on_beat({"pos": self.beat_pos + 1, "bpm": self.bpm}, from_os2l=False)

    def standby_active(self, now: float) -> bool:
        """Use a calm autonomous look when VirtualDJ is not sending beat events."""
        return not self.last_music_beat_at or now - self.last_music_beat_at > 4.0

    def manual_strobe_speed(self) -> float:
        return float(clamp(float(self.config.get("fixtures", {}).get("strobe", {}).get("speed", 1.0)), 0.0, 1.0))

    def manual_strobe_brightness(self) -> float:
        return float(clamp(float(self.config.get("fixtures", {}).get("strobe", {}).get("master", 1.0)), 0.0, 1.0))

    def manual_strobe_hz(self) -> float:
        return lerp(1.5, 14.0, self.manual_strobe_speed())

    def manual_head_strobe(self) -> int:
        return int(lerp(1, 255, self.manual_strobe_speed()))

    def apply_control(self, payload: dict[str, Any]) -> None:
        action = payload.get("action")
        with self.lock:
            if action == "set_mood":
                self.config["mood"] = int(clamp(int(payload.get("mood", 58)), 0, 100))
                self.config["preset"] = "custom" if payload.get("custom", True) else self.config["preset"]
                self.palette = self.make_palette()
            elif action == "set_master":
                self.config["master"] = float(clamp(float(payload.get("master", 1.0)), 0.0, 1.0))
            elif action == "set_output_master":
                target = str(payload.get("target", ""))
                value = float(clamp(float(payload.get("value", 1.0)), 0.0, 1.0))
                if target in {"led_master", "motion_master"}:
                    self.config[target] = value
            elif action == "set_running":
                was_running = self.running
                self.running = bool(payload.get("running", True))
                if was_running and not self.running:
                    # This fixture needs the reset range held long enough to complete
                    # its own calibration cycle before CH10 returns to zero.
                    self.reset_until = time.time() + 6.0
            elif action == "set_blackout":
                self.blackout = bool(payload.get("blackout", False))
            elif action == "set_layer":
                layer = str(payload.get("layer", ""))
                if layer in self.config.get("layers", {}):
                    self.config["layers"][layer] = bool(payload.get("enabled", True))
            elif action == "set_motion_mode":
                mode = str(payload.get("motion_mode", "auto"))
                if mode in MOTION_MODES:
                    self.config["motion_mode"] = mode
            elif action == "set_fixture_armed":
                fixture = str(payload.get("fixture", ""))
                armed = bool(payload.get("armed", False))
                if fixture in {"strobe", "fog"}:
                    self.config["fixtures"][fixture]["armed"] = armed
            elif action == "set_strobe_beat_pulse":
                self.config["fixtures"]["strobe"]["beat_pulse"] = bool(payload.get("enabled", False))
            elif action == "set_strobe_master":
                self.config["fixtures"]["strobe"]["master"] = float(clamp(float(payload.get("value", 1.0)), 0.0, 1.0))
            elif action == "set_strobe_speed":
                self.config["fixtures"]["strobe"]["speed"] = float(clamp(float(payload.get("value", 1.0)), 0.0, 1.0))
            elif action == "apply_preset":
                preset = str(payload.get("preset", "club"))
                if preset in PRESETS:
                    self.config["preset"] = preset
                    preset_config = PRESETS[preset]
                    if preset_config["mood"] is not None:
                        self.config["mood"] = preset_config["mood"]
                    if preset_config["effects"] is not None:
                        self.config["enabled_effects"] = list(preset_config["effects"])
                    if preset_config.get("motion_scenes") is not None:
                        self.config["enabled_motion_scenes"] = list(preset_config["motion_scenes"])
                        self.config["motion_mode"] = "auto"
                    self.choose_effect(force=True)
            elif action == "toggle_effect":
                effect = str(payload.get("effect", ""))
                enabled = bool(payload.get("enabled", True))
                effects = set(self.config["enabled_effects"])
                if effect in EFFECTS:
                    if enabled:
                        effects.add(effect)
                    else:
                        effects.discard(effect)
                    self.config["enabled_effects"] = sorted(effects)
                    self.config["preset"] = "custom"
                    if self.active_effect not in self.enabled_effects():
                        self.choose_effect(force=True)
            elif action == "toggle_motion_scene":
                scene = str(payload.get("scene", ""))
                enabled = bool(payload.get("enabled", True))
                scenes = set(self.config.get("enabled_motion_scenes", []))
                if scene in MOTION_MODES and scene != "auto":
                    if enabled:
                        scenes.add(scene)
                    else:
                        scenes.discard(scene)
                    self.config["enabled_motion_scenes"] = sorted(scenes) or ["center_pulse"]
                    self.config["preset"] = "custom"
                    if self.config.get("motion_mode") != "auto" and self.config["motion_mode"] not in self.config["enabled_motion_scenes"]:
                        self.config["motion_mode"] = "auto"
            elif action == "trigger":
                name = str(payload.get("name", "next"))
                now = time.time()
                if name == "whiteout":
                    self.whiteout_until = now + float(payload.get("seconds", 0.8))
                elif name == "color_strobe":
                    self.color_strobe_until = now + float(payload.get("seconds", 6.0))
                elif name == "strobe_out":
                    self.strobe_out_until = now + float(payload.get("seconds", 4.0))
                elif name == "fog":
                    if self.config["fixtures"]["fog"].get("armed", False):
                        self.fog_until = now + float(payload.get("seconds", 1.2))
                elif name == "next":
                    self.choose_effect(force=True)
                elif name == "blackout":
                    self.blackout = not self.blackout
            elif action == "set_hold_trigger":
                name = str(payload.get("name", ""))
                held = bool(payload.get("held", False))
                if name == "color_strobe":
                    self.color_strobe_held = held
                    if not held:
                        self.color_strobe_until = 0.0
                elif name == "strobe_out":
                    self.strobe_out_held = held
                    if not held:
                        self.strobe_out_until = 0.0
                elif name == "whiteout":
                    self.whiteout_held = held
                    if not held:
                        self.whiteout_until = 0.0
                elif name == "blackout":
                    self.blackout_held = held
            elif action == "set_artnet":
                self.config["artnet_host"] = str(payload.get("artnet_host", self.config["artnet_host"]))
                self.config["artnet_universe"] = int(payload.get("artnet_universe", self.config["artnet_universe"]))
                self.config["led_start_channel"] = int(payload.get("led_start_channel", self.config["led_start_channel"]))
                self.config["segment_count"] = int(clamp(int(payload.get("segment_count", self.config["segment_count"])), 1, 64))
                self.preview = [(0, 0, 0)] * self.config["segment_count"]
            elif action == "set_fixture_address":
                fixture = str(payload.get("fixture", ""))
                index = int(payload.get("index", 0))
                start = int(clamp(int(payload.get("start", 1)), 1, 512))
                fixtures = self.config.get("fixtures", {})
                if fixture == "led_bars" and 0 <= index < len(fixtures.get("led_bars", [])):
                    fixtures["led_bars"][index]["start"] = start
                elif fixture == "moving_heads" and 0 <= index < len(fixtures.get("moving_heads", [])):
                    fixtures["moving_heads"][index]["start"] = start
                elif fixture in {"strobe", "fog"}:
                    fixtures[fixture]["start"] = start
            save_config(self.config)

    def snapshot(self) -> dict[str, Any]:
        with self.lock:
            now = time.time()
            os2l_age = None if not self.last_os2l_at else round(now - self.last_os2l_at, 2)
            return {
                "running": self.running,
                "blackout": self.blackout or self.blackout_held,
                "active_effect": self.active_effect,
                "active_effect_label": EFFECTS.get(self.active_effect, self.active_effect),
                "bpm": round(self.bpm, 2),
                "beat_count": self.beat_count,
                "beat_pos": self.beat_pos,
                "last_os2l_age": os2l_age,
                "os2l_connected": bool(self.last_os2l_at and now - self.last_os2l_at < 3.0),
                "os2l_connections": self.os2l_connections,
                "artnet_packets": self.artnet_packets,
                "last_error": self.last_error,
                "config": self.config,
                "effects": EFFECTS,
                "motion_modes": MOTION_MODES,
                "motion_scenes": {
                    key: label
                    for key, label in MOTION_MODES.items()
                    if key != "auto"
                },
                "presets": sorted(PRESETS.keys()),
                "show": self.last_show,
                "preview": [
                    {"r": color[0], "g": color[1], "b": color[2]}
                    for color in self.preview
                ],
            }

    def render(self) -> tuple[list[tuple[int, int, int]], dict[str, Any]]:
        with self.lock:
            now = time.time()
            segments = int(self.config["segment_count"])
            if not self.running or self.blackout or self.blackout_held:
                colors = [(0, 0, 0)] * segments
                self.preview = colors
                self.last_show = self.render_idle_fixture_state(reset=not self.running and now < self.reset_until)
                return colors, self.config.copy()

            color_strobe_active = self.color_strobe_held or now < self.color_strobe_until
            white_strobe_active = self.strobe_out_held or now < self.strobe_out_until
            whiteout_active = self.whiteout_held or now < self.whiteout_until
            if self.standby_active(now) and not whiteout_active and not color_strobe_active and not white_strobe_active:
                standby_level = self.config.get("master", 1.0) * self.config.get("led_master", 1.0)
                colors = [
                    clean_rgb(scale_rgb(color, standby_level))
                    for color in self.render_standby_leds(segments, now)
                ]
                self.preview = colors
                self.last_show = self.render_standby_fixture_state(colors, now)
                return colors, self.config.copy()

            period = 60.0 / max(1.0, self.bpm)
            phase = clamp((now - self.last_beat_at) / period, 0.0, 1.0)
            beat_float = self.beat_count + phase
            mood = self.config["mood"] / 100
            master = self.config["master"]
            led_master = self.config.get("led_master", 1.0)
            palette = self.palette or self.make_palette()
            effect = self.active_effect
            allow_white = whiteout_active

            if allow_white:
                colors = [(255, 255, 255)] * segments
            elif white_strobe_active:
                strobe_on = int(now * self.manual_strobe_hz()) % 2 == 0
                colors = [(255, 255, 255) if strobe_on else (0, 0, 0)] * segments
            elif color_strobe_active:
                colors = self.render_color_strobe(segments, now, beat_float, phase, palette, mood, hard=True)
            elif effect == "ball":
                colors = self.render_ball(segments, beat_float, palette, mood)
            elif effect == "pair_swap":
                colors = self.render_pair_swap(segments, beat_float, phase, palette, mood)
            elif effect == "rainbow":
                colors = self.render_rainbow(segments, beat_float, mood)
            elif effect == "scanner":
                colors = self.render_scanner(segments, beat_float, palette, mood)
            elif effect == "sparkle":
                colors = self.render_sparkle(segments, beat_float, palette, mood)
            elif effect == "split":
                colors = self.render_split(segments, beat_float, phase, palette, mood)
            elif effect == "blocks":
                colors = self.render_blocks(segments, beat_float, palette, mood)
            elif effect == "comet":
                colors = self.render_comet(segments, beat_float, palette, mood)
            elif effect == "theater":
                colors = self.render_theater(segments, beat_float, palette, mood)
            elif effect == "zipper":
                colors = self.render_zipper(segments, beat_float, phase, palette, mood)
            elif effect == "orbit":
                colors = self.render_orbit(segments, beat_float, palette, mood)
            elif effect == "traffic":
                colors = self.render_traffic(segments, beat_float)
            elif effect == "gate":
                colors = self.render_gate(segments, beat_float, phase, palette, mood)
            elif effect == "binary":
                colors = self.render_binary(segments, beat_float, palette, mood)
            elif effect == "fill":
                colors = self.render_fill(segments, beat_float, phase, palette, mood)
            elif effect == "siren":
                colors = self.render_siren(segments, beat_float, phase, mood)
            elif effect == "strobe":
                colors = self.render_color_strobe(segments, now, beat_float, phase, palette, mood, hard=False)
            elif effect == "breathe":
                colors = self.render_breathe(segments, beat_float, palette, mood)
            else:
                colors = self.render_pulse(segments, beat_float, phase, palette, mood)

            beat_pop = math.exp(-phase * lerp(2.5, 8.0, mood))
            flash_amount = beat_pop * lerp(0.04, 0.32, mood)
            if self.beat_count % 16 == 1 and mood > 0.72:
                flash_amount += beat_pop * 0.22

            final: list[tuple[int, int, int]] = []
            beat_color = palette[int(beat_float) % len(palette)]
            for color in colors:
                if allow_white:
                    final.append(scale_rgb(color, master * led_master))
                    continue
                if white_strobe_active:
                    final.append(scale_rgb(color, master * led_master * self.manual_strobe_brightness()))
                    continue
                if max(color) < 8:
                    boosted = scale_rgb(beat_color, flash_amount * 1.65)
                else:
                    boosted = scale_rgb(color, 1.0 + flash_amount)
                final.append(clean_rgb(scale_rgb(boosted, master * led_master)))
            self.preview = final
            self.last_show = self.render_fixture_state(final, beat_float, phase, palette, mood, now)
            return final, self.config.copy()

    def render_idle_fixture_state(self, reset: bool = False) -> dict[str, Any]:
        fixtures = self.config.get("fixtures", {})
        # Keep the last commanded beam positions when output is stopped or blacked
        # out. Sending 0.5 here used to force every head into its centre position.
        previous_heads = {
            head.get("index"): head
            for head in self.last_show.get("moving_heads", [])
            if isinstance(head, dict)
        }
        show: dict[str, Any] = {
            "led_pixels": [
                {"r": 0, "g": 0, "b": 0}
                for _ in range(int(self.config.get("segment_count", 16)))
            ],
            "moving_heads": [],
            "strobe": {"level": 0, "rate": 0, "armed": fixtures.get("strobe", {}).get("armed", False)},
            "fog": {"level": 0, "armed": fixtures.get("fog", {}).get("armed", False)},
        }
        for index, head in enumerate(fixtures.get("moving_heads", [])):
            if not head.get("enabled", True):
                continue
            previous = previous_heads.get(index, {})
            pan = float(previous.get("pan", 0.5))
            tilt = float(previous.get("tilt", 0.5))
            show["moving_heads"].append({
                "index": index,
                "look": "reset" if reset else "idle",
                "requested_look": "idle",
                "target": previous.get("target", {"x": pan, "y": tilt}),
                "pan": pan,
                "tilt": tilt,
                "color": {"r": 255, "g": 255, "b": 255},
                "color_wheel": 0,
                "gobo": 0,
                "dimmer": 0,
                "strobe": 0,
                "reset": reset,
            })
        return show

    def standby_colors(self, now: float) -> tuple[tuple[int, int, int], tuple[int, int, int], float]:
        colors = [
            (0, 60, 255),
            (0, 210, 155),
            (135, 0, 255),
            (255, 35, 0),
        ]
        cycle = now / 42.0
        step = int(cycle) % len(colors)
        blend = 0.5 - 0.5 * math.cos((cycle % 1.0) * math.pi)
        return colors[step], colors[(step + 1) % len(colors)], blend

    def render_standby_leds(self, segments: int, now: float) -> list[tuple[int, int, int]]:
        primary, secondary, blend = self.standby_colors(now)
        calm = color_mix(primary, secondary, blend)
        mode = int(now // 36.0) % 3
        if mode == 0:
            level = 0.23 + 0.05 * (0.5 + 0.5 * math.sin(now * 0.22))
            return [scale_rgb(calm, level) for _ in range(segments)]
        if mode == 1:
            return [
                scale_rgb(calm if (index // 2) % 2 == 0 else secondary, 0.22)
                for index in range(segments)
            ]
        position = (now * 0.13) % max(1, segments * 2 - 2)
        if position > segments - 1:
            position = segments * 2 - 2 - position
        return [
            scale_rgb(calm, 0.08 + 0.24 * max(0.0, 1.0 - abs(index - position) / 2.6))
            for index in range(segments)
        ]

    def render_standby_fixture_state(
        self,
        led_pixels: list[tuple[int, int, int]],
        now: float,
    ) -> dict[str, Any]:
        fixtures = self.config.get("fixtures", {})
        primary, secondary, blend = self.standby_colors(now)
        color = color_mix(primary, secondary, blend)
        motion_master = self.config.get("motion_master", 1.0)
        master = self.config.get("master", 1.0)
        mode = int(now // 45.0) % 3
        names = ["standby_sweep", "standby_depth", "standby_orbit"]
        show: dict[str, Any] = {
            "led_pixels": [{"r": r, "g": g, "b": b} for r, g, b in led_pixels],
            "moving_heads": [],
            "strobe": {"level": 0, "rate": 0, "armed": fixtures.get("strobe", {}).get("armed", False)},
            "fog": {"level": 0, "armed": fixtures.get("fog", {}).get("armed", False)},
            "standby": True,
        }
        if not self.config.get("layers", {}).get("motion", True):
            return show
        enabled = [head for head in fixtures.get("moving_heads", []) if head.get("enabled", True)]
        head_count = max(1, len(enabled))
        requested_mode = str(self.config.get("motion_mode", "auto"))
        manual_motion = requested_mode if requested_mode in MOTION_MODES and requested_mode != "auto" else None
        standby_beat = now / (60.0 / max(1.0, self.bpm))
        for index, head in enumerate(fixtures.get("moving_heads", [])):
            if not head.get("enabled", True):
                continue
            pair = index // 2
            if manual_motion:
                # A selected position/scene always wins over the standby automation.
                x, y, _ = self.floor_target_for_head(
                    manual_motion,
                    index,
                    head_count,
                    standby_beat,
                    0.0,
                    0.28,
                )
                look = manual_motion
            elif mode == 0:
                x = 0.5 + math.sin(now * 0.28 + pair * math.pi) * 0.29
                y = 0.53 + math.cos(now * 0.18 + index * 0.45) * 0.15
                look = names[mode]
            elif mode == 1:
                x = 0.34 if pair == 0 else 0.66
                y = 0.52 + math.sin(now * 0.20 + pair * math.pi) * 0.28
                look = names[mode]
            else:
                x = 0.5 + math.sin(now * 0.22 + index * math.pi / 2) * 0.23
                y = 0.54 + math.cos(now * 0.22 + index * math.pi / 2) * 0.17
                look = names[mode]
            x, y = self.push_floor_forward(x, y)
            pan, tilt = self.target_to_pan_tilt(x, y, index, head_count, head)
            head_color = color if pair == 0 else color_mix(color, secondary, 0.24)
            show["moving_heads"].append({
                "index": index,
                "look": look,
                "requested_look": requested_mode if manual_motion else "standby",
                "target": {"x": round(x, 3), "y": round(y, 3)},
                "pan": clamp(pan, 0.0, 1.0),
                "tilt": clamp(tilt, 0.0, 1.0),
                "color": {"r": head_color[0], "g": head_color[1], "b": head_color[2]},
                "color_wheel": tmh17_color_wheel_value(head_color),
                "gobo": 0,
                "dimmer": int(clamp(92 * motion_master * master, 0, 255)),
                "strobe": 0,
            })
        return show

    def resolved_motion_mode(self, requested: str, effect: str, mood: float) -> str:
        if requested != "auto" and requested in MOTION_MODES:
            return requested
        enabled = [
            scene
            for scene in self.config.get("enabled_motion_scenes", [])
            if scene in MOTION_MODES and scene != "auto"
        ] or ["center_pulse"]
        preferred: list[str]
        if effect in {"split", "siren", "traffic"}:
            preferred = ["split_strobe", "cross_pairs", "side_pingpong"]
        elif effect in {"blocks", "pair_swap", "binary", "gate"}:
            preferred = ["corner_swap", "pair_random", "x_cross"]
        elif effect in {"ball", "scanner", "comet"}:
            preferred = ["gobo_chase", "line_sweep", "point_chase"]
        elif effect in {"rainbow", "theater", "zipper", "orbit", "fill"}:
            preferred = ["color_fan", "depth_sweep", "pair_orbit"]
        elif effect in {"sparkle", "strobe"} or mood > 0.86:
            preferred = ["x_cross", "all_random", "duo_random", "split_strobe"]
        else:
            preferred = ["center_pulse", "point_chase", "depth_sweep"]
        pool = [scene for scene in preferred if scene in enabled] or enabled
        scene_seed = self.effect_started_beat + sum(ord(char) for char in effect)
        return pool[scene_seed % len(pool)]

    def random_floor_point(self, seed: int) -> tuple[float, float]:
        rng = random.Random(seed)
        return self.floor_point(RANDOM_MOTION_POINTS[rng.randrange(len(RANDOM_MOTION_POINTS))])

    def floor_point(self, name: str) -> tuple[float, float]:
        x, y = MOTION_POINTS[name]
        return x, clamp(y + FLOOR_FORWARD_OFFSET, 0.08, 0.95)

    def push_floor_forward(self, x: float, y: float) -> tuple[float, float]:
        return x, clamp(y + FLOOR_FORWARD_OFFSET, 0.08, 0.95)

    def floor_target_for_head(
        self,
        mode: str,
        index: int,
        head_count: int,
        beat_float: float,
        phase: float,
        mood: float,
    ) -> tuple[float, float, float]:
        step_fast = max(2, int(lerp(8, 3, mood)))
        step_medium = max(4, int(lerp(16, 6, mood)))
        beat_step = int(beat_float // step_fast)
        pair = index // 2

        if mode in MOTION_POINTS:
            x, y = self.floor_point(mode)
            return x, y, lerp(0.78, 1.0, math.exp(-phase * 5.0))

        if mode == "point_chase":
            sequence = ["center", "front", "right", "back", "left", "front_left", "front_right", "center"]
            point = sequence[(int(beat_float // step_medium) + index % 2) % len(sequence)]
            x, y = self.floor_point(point)
            return x, y, lerp(0.52, 1.0, math.exp(-phase * lerp(2.8, 7.0, mood)))

        if mode == "line_sweep":
            sweep = (math.sin(beat_float * lerp(0.28, 0.84, mood) + (index % 2) * math.pi) + 1.0) / 2.0
            x = lerp(0.14, 0.86, sweep)
            y = 0.54 + math.sin(beat_float * 0.19 + index) * 0.13
            x, y = self.push_floor_forward(x, y)
            return x, y, lerp(0.55, 1.0, math.exp(-phase * lerp(2.6, 6.8, mood)))

        if mode == "depth_sweep":
            sweep = (math.sin(beat_float * lerp(0.18, 0.56, mood) + pair * math.pi) + 1.0) / 2.0
            x = 0.35 if index % 2 == 0 else 0.65
            y = lerp(0.18, 0.90, sweep)
            x, y = self.push_floor_forward(x, y)
            return x, y, lerp(0.58, 0.96, math.exp(-phase * lerp(2.4, 6.4, mood)))

        if mode == "cross_pairs":
            swap = int(beat_float // max(4, int(lerp(8, 4, mood)))) % 2 == 1
            left_x, right_x = (0.24, 0.76) if not swap else (0.76, 0.24)
            x = left_x if pair == 0 else right_x
            y = 0.48 + (0.16 if index % 2 else -0.04)
            x, y = self.push_floor_forward(x, y)
            pair_on = (int(beat_float) + pair) % 2 == 0 or phase < 0.24
            return x, y, 1.0 if pair_on else lerp(0.20, 0.48, mood)

        if mode == "split_strobe":
            x, y = self.floor_point("left" if pair == 0 else "right")
            pair_on = (int(beat_float * 2) + pair) % 2 == 0
            return x, y, 1.0 if pair_on else 0.06

        if mode == "corner_swap":
            swap = int(beat_float // max(4, int(lerp(8, 4, mood)))) % 2 == 1
            points = ["front_left", "front_right", "back_left", "back_right"]
            if swap:
                points = ["back_right", "back_left", "front_right", "front_left"]
            x, y = self.floor_point(points[index % len(points)])
            return x, y, lerp(0.55, 1.0, math.exp(-phase * 5.5))

        if mode == "center_pulse":
            x, y = self.floor_point("center")
            return x, y, lerp(0.30, 1.0, beat_hit := math.exp(-phase * lerp(3.0, 9.0, mood)))

        if mode == "gobo_chase":
            points = ["back_left", "center", "front_right", "front_left", "center", "back_right"]
            chase = int(beat_float // max(3, int(lerp(6, 3, mood)))) % len(points)
            x, y = self.floor_point(points[(chase + index) % len(points)])
            return x, y, 1.0 if phase < lerp(0.20, 0.42, mood) else lerp(0.35, 0.72, mood)

        if mode == "side_pingpong":
            side = int(beat_float // max(2, int(lerp(4, 2, mood)))) % 2
            point = "left" if (side + index) % 2 == 0 else "right"
            x, y = self.floor_point(point)
            return x, y, 1.0 if phase < 0.55 else lerp(0.28, 0.66, mood)

        if mode == "x_cross":
            points = ["front_left", "front_right", "back_right", "back_left"]
            if int(beat_float // max(4, int(lerp(8, 4, mood)))) % 2:
                points = ["back_right", "back_left", "front_left", "front_right"]
            x, y = self.floor_point(points[index % len(points)])
            return x, y, lerp(0.50, 1.0, math.exp(-phase * 4.8))

        if mode == "color_fan":
            fan = [0.16, 0.38, 0.62, 0.84]
            x = fan[index % len(fan)]
            y = 0.55 + math.sin(beat_float * lerp(0.10, 0.28, mood) + index) * 0.18
            x, y = self.push_floor_forward(x, y)
            return x, y, lerp(0.55, 0.95, math.exp(-phase * 4.0))

        if mode == "pair_orbit":
            angle = beat_float * lerp(0.18, 0.46, mood) + pair * math.pi
            x = 0.50 + math.sin(angle) * (0.22 if index % 2 == 0 else 0.34)
            y = 0.50 + math.cos(angle) * 0.25
            x, y = self.push_floor_forward(x, y)
            return x, y, lerp(0.45, 0.96, math.exp(-phase * 4.5))

        if mode == "pair_random":
            seed = self.sparkle_seed + beat_step * 97 + pair * 211
            x, y = self.random_floor_point(seed)
            return x, y, 1.0 if phase < lerp(0.18, 0.38, mood) else lerp(0.36, 0.76, mood)

        if mode == "duo_random":
            random_pair = (int(beat_float // max(2, step_medium * 2)) % 2)
            if pair == random_pair:
                seed = self.sparkle_seed + beat_step * 151 + index * 37
                x, y = self.random_floor_point(seed)
                dimmer_scale = 1.0 if phase < lerp(0.16, 0.32, mood) else lerp(0.34, 0.70, mood)
            else:
                sequence = ["center", "front", "back", "left", "right"]
                point = sequence[int(beat_float // step_medium) % len(sequence)]
                x, y = self.floor_point(point)
                dimmer_scale = lerp(0.62, 0.95, math.exp(-phase * 4.0))
            return x, y, dimmer_scale

        seed = self.sparkle_seed + beat_step * 83 + index * 271
        x, y = self.random_floor_point(seed)
        return x, y, 1.0 if phase < lerp(0.14, 0.34, mood) else lerp(0.28, 0.68, mood)

    def target_to_pan_tilt(
        self,
        target_x: float,
        target_y: float,
        index: int,
        head_count: int,
        head: dict[str, Any],
    ) -> tuple[float, float]:
        pan_low, pan_high = sorted((
            float(head.get("pan_min", 0.24)),
            float(head.get("pan_max", 0.76)),
        ))
        tilt_low, tilt_high = sorted((
            float(head.get("tilt_min", 0.56)),
            float(head.get("tilt_max", 0.82)),
        ))
        pan_center = float(head.get("pan_center", 0.333))
        pan_width = float(head.get("pan_width", 0.30)) - target_y * 0.03
        pan = pan_center + (target_x - 0.5) * pan_width
        tilt = lerp(tilt_low, tilt_high, target_y)
        return clamp(pan, pan_low, pan_high), clamp(tilt, tilt_low, tilt_high)

    def render_fixture_state(
        self,
        led_pixels: list[tuple[int, int, int]],
        beat_float: float,
        phase: float,
        palette: list[tuple[int, int, int]],
        mood: float,
        now: float,
    ) -> dict[str, Any]:
        layers = self.config.get("layers", {})
        fixtures = self.config.get("fixtures", {})
        beat_hit = math.exp(-phase * lerp(2.5, 9.0, mood))
        show: dict[str, Any] = {
            "led_pixels": [
                {"r": r, "g": g, "b": b}
                for r, g, b in led_pixels
            ],
            "moving_heads": [],
            "strobe": {"level": 0, "rate": 0, "armed": fixtures.get("strobe", {}).get("armed", False)},
            "fog": {"level": 0, "armed": fixtures.get("fog", {}).get("armed", False)},
        }

        if layers.get("motion", True):
            head_configs = [head for head in fixtures.get("moving_heads", []) if head.get("enabled", True)]
            head_count = max(1, len(head_configs))
            effect = self.active_effect
            requested_mode = str(self.config.get("motion_mode", "auto"))
            motion_mode = self.resolved_motion_mode(requested_mode, effect, mood)
            fixed_point_mode = motion_mode in MOTION_POINTS
            whiteout_active = self.whiteout_held or now < self.whiteout_until
            color_strobe_active = self.color_strobe_held or now < self.color_strobe_until
            white_strobe_active = self.strobe_out_held or now < self.strobe_out_until
            synced_color = brightest_color(
                led_pixels,
                palette[int(beat_float // 4) % len(palette)],
            )
            left_color = led_color_range(led_pixels, 0.0, 0.5, synced_color)
            right_color = led_color_range(led_pixels, 0.5, 1.0, synced_color)
            color_hold_beats = max(6, int(lerp(16, 8, mood)))
            motion_color_step = int((beat_float - self.effect_started_beat) // color_hold_beats)
            motion_palette = [color for color in palette if max(color) > 20] or [synced_color]
            scene_color = motion_palette[motion_color_step % len(motion_palette)]
            left_scene_color = motion_palette[motion_color_step % len(motion_palette)]
            right_scene_color = motion_palette[(motion_color_step + 1) % len(motion_palette)]

            gobo_steps = [0, 10, 18, 26, 34, 42, 50, 58]
            gobo_scene_step = int((beat_float - self.effect_started_beat) // max(2, int(lerp(8, 3, mood))))
            gobo_base = gobo_steps[gobo_scene_step % len(gobo_steps)]
            for index, head in enumerate(fixtures.get("moving_heads", [])):
                if not head.get("enabled", True):
                    continue
                pair = index // 2
                target_x, target_y, dimmer_scale = self.floor_target_for_head(
                    motion_mode,
                    index,
                    head_count,
                    beat_float,
                    phase,
                    mood,
                )
                spatial_color = led_color_at(led_pixels, target_x, synced_color)
                if fixed_point_mode:
                    color = scene_color
                elif motion_mode in {"split_strobe", "cross_pairs", "side_pingpong"}:
                    color = left_scene_color if target_x < 0.5 else right_scene_color
                elif motion_mode in {"color_fan", "gobo_chase", "pair_orbit", "x_cross"}:
                    color = motion_palette[(motion_color_step + pair) % len(motion_palette)]
                else:
                    color = color_mix(scene_color, spatial_color, 0.22)
                if whiteout_active or white_strobe_active:
                    color = (255, 255, 255)
                elif color_strobe_active:
                    color = brightest_color(led_pixels, palette[int(beat_float * 4) % len(palette)])
                pan, tilt = self.target_to_pan_tilt(target_x, target_y, index, head_count, head)
                if motion_mode == "gobo_chase":
                    gobo = [10, 18, 26, 34, 42, 50, 58][(gobo_scene_step + index) % 7]
                elif motion_mode in {"all_random", "pair_random", "duo_random"}:
                    gobo = [10, 18, 26, 34, 42, 50, 58][
                        (gobo_scene_step + pair) % 7
                    ]
                elif motion_mode in {"corner_swap", "x_cross", "pair_orbit"}:
                    gobo = gobo_base
                elif motion_mode == "split_strobe":
                    gobo = 0 if pair == 0 else gobo_base
                elif motion_mode in {"side_pingpong", "color_fan"}:
                    gobo = gobo_base
                elif motion_mode in {"center", "front", "back", "left", "right"}:
                    gobo = 0 if mood < 0.55 else gobo_base
                else:
                    gobo = gobo_base if mood > 0.42 else 0
                if whiteout_active or white_strobe_active:
                    gobo = 0

                motion_master = self.config.get("motion_master", 1.0)
                dimmer = int((lerp(105, 240, mood) + beat_hit * lerp(15, 80, mood)) * dimmer_scale * motion_master * self.config.get("master", 1.0))
                strobe = 0
                if whiteout_active:
                    dimmer = int(255 * motion_master * self.config.get("master", 1.0))
                if white_strobe_active:
                    dimmer = int(255 * motion_master * self.config.get("master", 1.0) * self.manual_strobe_brightness())
                    strobe = self.manual_head_strobe()
                if color_strobe_active:
                    strobe = int(80 + beat_hit * 120) if phase < 0.18 else 0
                if color_strobe_active:
                    dimmer = int(255 * motion_master * self.config.get("master", 1.0))
                    strobe = self.manual_head_strobe()
                if not color_strobe_active and motion_mode == "split_strobe":
                    pair_on = dimmer_scale > 0.5
                    strobe = int(150 + beat_hit * 80) if pair_on else 0
                elif not color_strobe_active and motion_mode == "gobo_chase" and phase < 0.20:
                    strobe = int(70 + beat_hit * 70)
                elif not color_strobe_active and motion_mode == "side_pingpong" and phase < 0.16:
                    strobe = int(90 + beat_hit * 80)
                show["moving_heads"].append({
                    "index": index,
                    "look": motion_mode,
                    "requested_look": requested_mode,
                    "target": {"x": round(target_x, 3), "y": round(target_y, 3)},
                    "pan": clamp(pan, 0.0, 1.0),
                    "tilt": clamp(tilt, 0.0, 1.0),
                    "color": {"r": color[0], "g": color[1], "b": color[2]},
                    "color_wheel": tmh17_color_wheel_value(color),
                    "gobo": int(clamp(gobo, 0, 255)),
                    "dimmer": int(clamp(dimmer, 0, 255)),
                    "strobe": strobe,
                })

        strobe_cfg = fixtures.get("strobe", {})
        if strobe_cfg.get("armed", False):
            strobe_master = self.manual_strobe_brightness()
            strobe_speed = float(clamp(float(strobe_cfg.get("speed", 1.0)), 0.0, 1.0))
            if self.strobe_out_held or now < self.strobe_out_until:
                # Strobe Out is the only manual trigger that operates the big white strobe.
                show["strobe"] = {"level": int(255 * strobe_master), "rate": int(255 * strobe_speed), "armed": True}
            elif strobe_cfg.get("beat_pulse", False) and not self.color_strobe_held and now >= self.color_strobe_until and phase < 0.14:
                show["strobe"] = {"level": int(255 * strobe_master), "rate": int(255 * strobe_speed), "armed": True}

        fog_cfg = fixtures.get("fog", {})
        if layers.get("fog", False) and fog_cfg.get("armed", False) and now < self.fog_until:
            show["fog"] = {"level": 255, "armed": True}

        return show

    def render_pulse(
        self,
        segments: int,
        beat_float: float,
        phase: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        color = palette[int(beat_float // 4) % len(palette)]
        base = lerp(0.22, 0.42, mood)
        pulse = math.exp(-phase * lerp(2.0, 7.0, mood))
        return [scale_rgb(color, base + pulse * lerp(0.35, 0.78, mood)) for _ in range(segments)]

    def render_ball(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        bg = scale_rgb(palette[0], lerp(0.10, 0.24, mood))
        ball = palette[1 % len(palette)]
        travel = max(1, segments - 1)
        speed = lerp(0.38, 1.25, mood)
        raw = (beat_float * speed) % (travel * 2)
        pos = raw if raw <= travel else travel * 2 - raw
        width = lerp(1.45, 0.65, mood)
        colors = []
        for index in range(segments):
            dist = abs(index - pos)
            amount = max(0.0, 1.0 - dist / width)
            colors.append(color_mix(bg, ball, amount))
        return colors

    def render_pair_swap(
        self,
        segments: int,
        beat_float: float,
        phase: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        smooth = 0.5 - 0.5 * math.cos(phase * math.pi)
        if mood > 0.58:
            smooth = 1.0 if phase > 0.18 else smooth
        swapped = int(beat_float // 2) % 2 == 1
        a = palette[0]
        b = palette[2 % len(palette)]
        if swapped:
            a, b = b, a
        colors = []
        for index in range(segments):
            block = (index // 2) % 2
            src = a if block == 0 else b
            dst = b if block == 0 else a
            colors.append(color_mix(src, dst, smooth * 0.12 if mood < 0.45 else 0.0))
        return colors

    def render_rainbow(self, segments: int, beat_float: float, mood: float) -> list[tuple[int, int, int]]:
        speed = lerp(0.03, 0.12, mood)
        sat = lerp(0.68, 1.0, mood)
        val = lerp(0.55, 1.0, mood)
        return [
            clean_rgb(hsv_rgb((index / max(1, segments) + beat_float * speed) % 1.0, sat, val))
            for index in range(segments)
        ]

    def render_scanner(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        travel = max(1, segments - 1)
        speed = lerp(0.55, 1.8, mood)
        raw = (beat_float * speed) % (travel * 2)
        pos = raw if raw <= travel else travel * 2 - raw
        head = palette[int(beat_float // 8) % len(palette)]
        tail = lerp(1.8, 0.8, mood)
        colors = []
        for index in range(segments):
            dist = abs(index - pos)
            amount = max(0.0, 1.0 - dist / tail)
            colors.append(scale_rgb(head, amount))
        return colors

    def render_sparkle(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        base = scale_rgb(palette[0], lerp(0.06, 0.18, mood))
        colors = [base for _ in range(segments)]
        tick = int(beat_float * lerp(3.0, 9.0, mood))
        rng = random.Random(self.sparkle_seed + tick)
        count = max(1, int(lerp(1, max(2, segments // 2), mood)))
        for _ in range(count):
            index = rng.randrange(segments)
            color = rng.choice(palette)
            colors[index] = color_mix(colors[index], color, rng.uniform(0.55, 1.0))
        return colors

    def render_split(
        self,
        segments: int,
        beat_float: float,
        phase: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        side_swap = int(beat_float // 4) % 2 == 1
        left = palette[0]
        right = palette[2 % len(palette)]
        if side_swap:
            left, right = right, left
        hit = math.exp(-phase * lerp(2.0, 7.0, mood))
        colors = []
        for index in range(segments):
            side = left if index < segments / 2 else right
            center_boost = 1.0 - abs(index - (segments - 1) / 2) / max(1, segments / 2)
            amount = 0.48 + hit * 0.36 + center_boost * hit * mood * 0.16
            colors.append(scale_rgb(side, amount))
        return colors

    def render_blocks(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        step = int(beat_float * lerp(0.5, 1.5, mood))
        colors = []
        for index in range(segments):
            block = ((index // 2) + step) % len(palette)
            colors.append(clean_rgb(palette[block]))
        return colors

    def render_color_strobe(
        self,
        segments: int,
        now: float,
        beat_float: float,
        phase: float,
        palette: list[tuple[int, int, int]],
        mood: float,
        hard: bool,
    ) -> list[tuple[int, int, int]]:
        speed = self.manual_strobe_hz() if hard else lerp(2.0, 8.0, mood)
        gate = (now * speed) % 1.0 if hard else (phase * speed) % 1.0
        on = gate < (0.16 if hard or mood > 0.7 else 0.26)
        color_step = int(now * speed) if hard else int(beat_float * speed)
        color = clean_rgb(palette[color_step % len(palette)])
        return [color if on else (0, 0, 0) for _ in range(segments)]

    def render_breathe(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        wave = 0.5 + 0.5 * math.sin(beat_float * lerp(0.18, 0.42, mood))
        color = color_mix(palette[0], palette[1 % len(palette)], wave)
        amount = lerp(0.22, 0.62, wave)
        return [scale_rgb(color, amount) for _ in range(segments)]

    def render_comet(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        speed = lerp(0.45, 1.45, mood)
        pos = (beat_float * speed) % max(1, segments)
        tail = lerp(3.4, 1.4, mood)
        color = palette[int(beat_float // 8) % len(palette)]
        colors = []
        for index in range(segments):
            dist = (pos - index) % max(1, segments)
            amount = max(0.0, 1.0 - dist / tail)
            colors.append(scale_rgb(color, amount))
        return colors

    def render_theater(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        step = int(beat_float * lerp(1.0, 2.5, mood))
        gap = 3 if mood < 0.72 else 2
        colors = []
        for index in range(segments):
            if (index + step) % gap == 0:
                colors.append(palette[((index + step) // gap) % len(palette)])
            else:
                colors.append((0, 0, 0))
        return colors

    def render_zipper(
        self,
        segments: int,
        beat_float: float,
        phase: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        cycle = int(beat_float // 4)
        progress = ((beat_float % 4) + phase) / 4
        fill = int(clamp(progress, 0.0, 1.0) * (segments // 2 + 1))
        color = palette[cycle % len(palette)]
        bg = scale_rgb(palette[(cycle + 2) % len(palette)], lerp(0.04, 0.12, mood))
        colors = []
        inward = cycle % 2 == 0
        center = (segments - 1) / 2
        for index in range(segments):
            if inward:
                active = index < fill or index >= segments - fill
            else:
                active = abs(index - center) <= fill - 0.5
            colors.append(color if active else bg)
        return colors

    def render_orbit(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        speed = lerp(0.35, 1.2, mood)
        pos_a = (beat_float * speed) % max(1, segments)
        pos_b = (pos_a + segments / 2) % max(1, segments)
        color_a = palette[int(beat_float // 8) % len(palette)]
        color_b = palette[(int(beat_float // 8) + 2) % len(palette)]
        bg = scale_rgb(palette[(int(beat_float // 16) + 3) % len(palette)], 0.05)
        width = lerp(1.45, 0.75, mood)
        colors = []
        for index in range(segments):
            dist_a = min(abs(index - pos_a), segments - abs(index - pos_a))
            dist_b = min(abs(index - pos_b), segments - abs(index - pos_b))
            amount_a = max(0.0, 1.0 - dist_a / width)
            amount_b = max(0.0, 1.0 - dist_b / width)
            if amount_a >= amount_b:
                colors.append(color_mix(bg, color_a, amount_a))
            else:
                colors.append(color_mix(bg, color_b, amount_b))
        return colors

    def render_traffic(self, segments: int, beat_float: float) -> list[tuple[int, int, int]]:
        pattern = [(255, 0, 0), (255, 190, 0), (0, 255, 0), (0, 55, 255)]
        step = int(beat_float)
        return [pattern[((index // 2) + step) % len(pattern)] for index in range(segments)]

    def render_gate(
        self,
        segments: int,
        beat_float: float,
        phase: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        width = lerp(0.58, 0.18, mood)
        on = phase < width
        color = palette[int(beat_float // 2) % len(palette)]
        return [color if on else (0, 0, 0) for _ in range(segments)]

    def render_binary(
        self,
        segments: int,
        beat_float: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        tick = int(beat_float * lerp(1.0, 3.0, mood))
        rng = random.Random(self.sparkle_seed + tick * 97)
        density = lerp(0.28, 0.68, mood)
        colors = []
        for index in range(segments):
            if rng.random() < density:
                colors.append(palette[(index + tick) % len(palette)])
            else:
                colors.append((0, 0, 0))
        return colors

    def render_fill(
        self,
        segments: int,
        beat_float: float,
        phase: float,
        palette: list[tuple[int, int, int]],
        mood: float,
    ) -> list[tuple[int, int, int]]:
        step_float = (beat_float * lerp(0.8, 1.6, mood)) % max(1, segments * 2)
        step = int(step_float)
        color = palette[int(beat_float // 8) % len(palette)]
        bg = scale_rgb(palette[(int(beat_float // 8) + 2) % len(palette)], 0.06)
        if step < segments:
            return [color if index <= step else bg for index in range(segments)]
        clear_to = step - segments
        return [bg if index <= clear_to else color for index in range(segments)]

    def render_siren(
        self,
        segments: int,
        beat_float: float,
        phase: float,
        mood: float,
    ) -> list[tuple[int, int, int]]:
        red = (255, 0, 0)
        blue = (0, 55, 255)
        flip = int(beat_float * lerp(1.0, 2.0, mood)) % 2 == 1
        left = blue if flip else red
        right = red if flip else blue
        chase = int((beat_float + phase) * lerp(1.0, 3.0, mood)) % max(1, segments)
        colors = []
        for index in range(segments):
            side = left if index < segments / 2 else right
            colors.append(side if index == chase or mood > 0.45 else scale_rgb(side, 0.55))
        return colors


class ArtNetSender:
    def __init__(self, state: RuntimeState):
        self.state = state
        self.sequence = 1
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    def artnet_targets(self, config: dict[str, Any]) -> list[str]:
        raw_host = str(config["artnet_host"])
        targets = [target.strip() for target in raw_host.split(",") if target.strip()]
        return targets or ["127.0.0.1"]

    def send(self, colors: list[tuple[int, int, int]], config: dict[str, Any]) -> None:
        dmx = bytearray(512)
        show = self.state.last_show or {}
        fixtures = config.get("fixtures", {})
        layers = config.get("layers", {})
        led_index = 0

        if layers.get("led_bars", True):
            for bar in fixtures.get("led_bars", []):
                if not bar.get("enabled", True):
                    continue
                start = max(1, int(bar.get("start", config.get("led_start_channel", 1)))) - 1
                segment_count = int(bar.get("segments", 8))
                for segment in range(segment_count):
                    if led_index >= len(colors):
                        break
                    offset = start + segment * 3
                    if offset + 2 >= len(dmx):
                        break
                    color = colors[led_index]
                    dmx[offset] = color[0]
                    dmx[offset + 1] = color[1]
                    dmx[offset + 2] = color[2]
                    led_index += 1

        for head_state, head_config in zip(show.get("moving_heads", []), fixtures.get("moving_heads", [])):
            if not head_config.get("enabled", True):
                continue
            start = max(1, int(head_config.get("start", 1))) - 1
            if start + 10 >= len(dmx):
                continue
            color = head_state.get("color", {})
            pan = int(clamp(float(head_state.get("pan", 0.5)), 0.0, 1.0) * 255)
            tilt = int(clamp(float(head_state.get("tilt", 0.5)), 0.0, 1.0) * 255)
            profile = str(head_config.get("profile", "tmh17")).lower()
            if profile == "rgb11":
                dmx[start + 0] = pan
                dmx[start + 1] = tilt
                dmx[start + 2] = int(lerp(190, 90, config.get("mood", 58) / 100))
                dmx[start + 3] = int(clamp(int(head_state.get("dimmer", 0)), 0, 255))
                dmx[start + 4] = int(clamp(int(head_state.get("strobe", 0)), 0, 255))
                dmx[start + 5] = int(clamp(int(color.get("r", 0)), 0, 255))
                dmx[start + 6] = int(clamp(int(color.get("g", 0)), 0, 255))
                dmx[start + 7] = int(clamp(int(color.get("b", 0)), 0, 255))
                dmx[start + 8] = 0
                dmx[start + 9] = 0
                dmx[start + 10] = 0
            elif profile == "zkymzl11":
                look = str(head_state.get("look", ""))
                reset_active = bool(head_state.get("reset", False))
                dimmer_value = int(clamp(int(head_state.get("dimmer", 0)), 0, 255))
                shutter = 10
                if int(head_state.get("strobe", 0)) > 0:
                    shutter = int(lerp(18, 131, clamp(int(head_state.get("strobe", 0)) / 255, 0.0, 1.0)))
                if dimmer_value <= 0:
                    shutter = 0
                gobo_value = int(clamp(int(head_state.get("gobo", 0)), 0, 127))
                if gobo_value > 0 and look in {"gobo_chase", "x_cross", "pair_orbit"} and config.get("mood", 58) / 100 > 0.62:
                    gobo_value = int(clamp(gobo_value + 64, 64, 127))
                dmx[start + 0] = pan
                dmx[start + 1] = 0
                dmx[start + 2] = tilt
                dmx[start + 3] = 0
                dmx[start + 4] = zkymzl_color_wheel_value((
                    int(color.get("r", 0)),
                    int(color.get("g", 0)),
                    int(color.get("b", 0)),
                ))
                dmx[start + 5] = gobo_value
                dmx[start + 6] = shutter
                dmx[start + 7] = dimmer_value
                dmx[start + 8] = 150 if look.startswith("standby_") else int(lerp(210, 90, config.get("mood", 58) / 100))
                # CH10: 200-209 is the fixture's reset command range.
                dmx[start + 9] = 204 if reset_active else 0
                dmx[start + 10] = 0
            else:
                dmx[start + 0] = pan
                dmx[start + 1] = 0
                dmx[start + 2] = tilt
                dmx[start + 3] = 0
                dmx[start + 4] = int(clamp(int(head_state.get("color_wheel", 0)), 0, 255))
                dmx[start + 5] = int(clamp(int(head_state.get("gobo", 0)), 0, 255))
                dmx[start + 6] = int(clamp(int(head_state.get("strobe", 0)), 0, 255))
                dmx[start + 7] = int(clamp(int(head_state.get("dimmer", 0)), 0, 255))
                dmx[start + 8] = int(lerp(210, 90, config.get("mood", 58) / 100))
                dmx[start + 9] = 0
                dmx[start + 10] = 0

        strobe_config = fixtures.get("strobe", {})
        if strobe_config.get("enabled", True):
            start = max(1, int(strobe_config.get("start", 1))) - 1
            strobe_state = show.get("strobe", {})
            if start + 1 < len(dmx):
                # Stairville 1500W Strobe: CH1 = flash speed, CH2 = brightness.
                dmx[start] = int(clamp(int(strobe_state.get("rate", 0)), 0, 255))
                dmx[start + 1] = int(clamp(int(strobe_state.get("level", 0)), 0, 255))

        fog_config = fixtures.get("fog", {})
        if fog_config.get("enabled", True):
            start = max(1, int(fog_config.get("start", 1))) - 1
            fog_state = show.get("fog", {})
            if start < len(dmx):
                dmx[start] = int(clamp(int(fog_state.get("level", 0)), 0, 255))

        universe = int(config["artnet_universe"])
        packet = (
            b"Art-Net\x00"
            + struct.pack("<H", 0x5000)
            + struct.pack(">H", 14)
            + bytes([self.sequence, 0])
            + struct.pack("<H", universe)
            + struct.pack(">H", len(dmx))
            + bytes(dmx)
        )
        self.sequence = 1 if self.sequence >= 255 else self.sequence + 1
        for target in self.artnet_targets(config):
            self.sock.sendto(packet, (target, int(config["artnet_port"])))
        with self.state.lock:
            self.state.artnet_packets += 1


class JsonStream:
    def __init__(self) -> None:
        self.decoder = json.JSONDecoder()
        self.buffer = ""

    def feed(self, data: bytes) -> list[dict[str, Any]]:
        self.buffer += data.decode("utf-8", errors="ignore")
        messages: list[dict[str, Any]] = []
        while True:
            self.buffer = self.buffer.lstrip("\ufeff \t\r\n")
            if not self.buffer:
                break
            try:
                message, end = self.decoder.raw_decode(self.buffer)
            except json.JSONDecodeError:
                if len(self.buffer) > 8192:
                    self.buffer = self.buffer[-1024:]
                break
            self.buffer = self.buffer[end:]
            if isinstance(message, dict):
                messages.append(message)
        return messages


def os2l_server(state: RuntimeState) -> None:
    host = state.config["os2l_host"]
    port = int(state.config["os2l_port"])
    try:
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((host, port))
        server.listen(4)
        server.settimeout(0.5)
    except OSError as exc:
        state.set_error(f"OS2L port {host}:{port} konnte nicht gestartet werden: {exc}")
        return

    state.set_error("")
    while not state.stop_event.is_set():
        try:
            client, address = server.accept()
        except socket.timeout:
            continue
        except OSError as exc:
            state.set_error(f"OS2L accept error: {exc}")
            break

        with state.lock:
            state.os2l_connections += 1
            state.os2l_connected = True
            state.last_os2l_at = time.time()
        threading.Thread(
            target=os2l_client,
            args=(state, client, address),
            daemon=True,
        ).start()

    server.close()


def os2l_client(state: RuntimeState, client: socket.socket, address: Any) -> None:
    stream = JsonStream()
    client.settimeout(1.0)
    with client:
        while not state.stop_event.is_set():
            try:
                data = client.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                break
            if not data:
                break
            for message in stream.feed(data):
                event = str(message.get("evt", "")).lower()
                if event == "beat":
                    state.on_beat(message, from_os2l=True)
                elif event == "btn":
                    state.on_os2l_button(message)
                else:
                    with state.lock:
                        state.last_os2l_at = time.time()


def output_loop(state: RuntimeState) -> None:
    sender = ArtNetSender(state)
    while not state.stop_event.is_set():
        frame_started = time.time()
        state.maybe_internal_beat()
        try:
            colors, config = state.render()
            sender.send(colors, config)
        except OSError as exc:
            state.set_error(f"ArtNet send error: {exc}")
        fps = max(1, int(state.config.get("fps", 40)))
        delay = max(0.0, (1.0 / fps) - (time.time() - frame_started))
        time.sleep(delay)


def make_http_handler(state: RuntimeState) -> type[BaseHTTPRequestHandler]:
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format: str, *args: Any) -> None:
            return

        def do_GET(self) -> None:
            if self.path == "/api/state":
                self.send_json(state.snapshot())
                return

            path = self.path.split("?", 1)[0]
            if path in {"/", "/web", "/web/"}:
                path = "/index.html"
            file_path = (WEB_DIR / path.lstrip("/")).resolve()
            if WEB_DIR.resolve() not in file_path.parents and file_path != WEB_DIR.resolve():
                self.send_error(403)
                return
            if not file_path.exists() or not file_path.is_file():
                self.send_error(404)
                return

            content = file_path.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", self.content_type(file_path))
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        def do_POST(self) -> None:
            if self.path != "/api/control":
                self.send_error(404)
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                payload = json.loads(self.rfile.read(length).decode("utf-8"))
                if not isinstance(payload, dict):
                    raise ValueError("Payload must be an object")
                state.apply_control(payload)
                self.send_json(state.snapshot())
            except (ValueError, json.JSONDecodeError) as exc:
                self.send_json({"ok": False, "error": str(exc)}, status=400)

        def send_json(self, payload: dict[str, Any], status: int = 200) -> None:
            content = json.dumps(payload).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        @staticmethod
        def content_type(path: Path) -> str:
            if path.suffix == ".html":
                return "text/html; charset=utf-8"
            if path.suffix == ".css":
                return "text/css; charset=utf-8"
            if path.suffix == ".js":
                return "application/javascript; charset=utf-8"
            return "application/octet-stream"

    return Handler


def run_http(state: RuntimeState) -> ThreadingHTTPServer:
    host = state.config["web_host"]
    port = int(state.config["web_port"])
    server = ThreadingHTTPServer((host, port), make_http_handler(state))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="VirtualDJ OS2L to ArtNet mini light engine")
    parser.add_argument("--web-host", help="Web UI bind address, for example 0.0.0.0 on a Raspberry Pi")
    parser.add_argument("--web-port", type=int, help="Local web UI port")
    parser.add_argument("--os2l-host", help="OS2L bind address, for example 0.0.0.0 on a Raspberry Pi")
    parser.add_argument("--os2l-port", type=int, help="OS2L TCP port")
    parser.add_argument("--artnet-host", help="ArtNet target or broadcast address")
    parser.add_argument("--universe", type=int, help="ArtNet universe")
    parser.add_argument("--start-channel", type=int, help="First DMX channel of the LED bar, 1-based")
    parser.add_argument("--segments", type=int, help="RGB segment count")
    parser.add_argument("--fps", type=int, help="DMX output frames per second")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    config = load_config()
    if args.web_host:
        config["web_host"] = args.web_host
    if args.web_port is not None:
        config["web_port"] = args.web_port
    if args.os2l_host:
        config["os2l_host"] = args.os2l_host
    if args.os2l_port is not None:
        config["os2l_port"] = args.os2l_port
    if args.artnet_host:
        config["artnet_host"] = args.artnet_host
    if args.universe is not None:
        config["artnet_universe"] = args.universe
    if args.start_channel is not None:
        config["led_start_channel"] = args.start_channel
    if args.segments is not None:
        config["segment_count"] = args.segments
    if args.fps is not None:
        config["fps"] = args.fps
    config = normalized_config(config)

    state = RuntimeState(config)
    state.choose_effect(force=True)
    threading.Thread(target=os2l_server, args=(state,), daemon=True).start()
    threading.Thread(target=output_loop, args=(state,), daemon=True).start()
    http_server = run_http(state)

    url = f"http://{config['web_host']}:{config['web_port']}"
    print("Mini Light Engine running")
    print(f"Web UI: {url}")
    print(f"OS2L:   {config['os2l_host']}:{config['os2l_port']}")
    print(f"ArtNet: {config['artnet_host']} universe {config['artnet_universe']}")
    print("Press Ctrl+C to stop.")

    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        state.stop_event.set()
        http_server.shutdown()


if __name__ == "__main__":
    main()
