# Predictive Input Buffer

An experimental input assistant for Geometry Dash 2.2081. It predicts the next few
frames with a deliberately simple heuristic and, if a click would put you inside a
hazard, holds that click until a later safe frame. It can also hold back a Left/Right
release for a few milliseconds while airborne.

> **This changes your input timing.** Completions made with this mod are not legit for
> leaderboards or level lists. Treat it as a training / accessibility tool.

## Features

### 1. Predictive input buffer

When a press arrives, the mod looks at hazards within `scan-radius` units of the player.
If the next frame's predicted player box would overlap one of them, the press is queued
instead of being forwarded to the game. The queue is drained from the game layer's own
`update`, so the click is delivered on a later frame at the same code path as a normal
click.

- Base window: `window-size` frames (default 8).
- Extended window: `extended-window-size` (default 16) when at least `density-threshold`
  hazards are nearby.
- Frame choice: `humanization-chance` (default 80%) the earliest safe frame, otherwise a
  random safe frame from the window.
- If no frame in the window is safe, the click is passed through untouched, so the mod
  never invents an input that the game would not have seen.
- A release that arrives before its buffered press is delivered is swallowed and
  re-issued one frame after the press, so a quick tap does not leave the button stuck.
- By default only the flight modes (ship, UFO, wave, swing) are buffered. In the gravity
  modes (cube, ball, robot, spider) a one-frame delay can turn a required jump into a
  death, so presses there are passed through until `buffer-in-cube-mode` is enabled.

### 2. Directional release protection

`PlayerObject::releaseButton` is intercepted for `Left`/`Right`. While airborne in a
flying mode (ship, UFO, wave/dart, swing) or during a dash, the release is scheduled
`release-delay-ms` milliseconds later (default 20 ms). A new press on the same button
cancels the pending release, so fast tap-hold patterns keep working.

The delay is skipped when the player is on the ground, and `Jump` releases are never
delayed.

### 3. Humanization and logging

Frame selection is randomized as described above. With `debug-logs` enabled, every
buffered click is logged with its chosen delay, and leaving a level prints a summary:
number of buffered clicks, average click delay, number of delayed releases and early
releases.

## Prediction modes

Both modes are heuristics, not a physics reimplementation.

| Mode | What it does |
| --- | --- |
| `aabb` (default) | Reads the player's current rect and velocity, extrapolates linearly for each frame in the window and checks AABB overlaps with nearby hazards. Fast, but ignores gravity and slopes. |
| `trajectory` | Integrates position frame by frame (`p + v*t + 0.5*a*t^2`) using an assumed gravity magnitude and the player's gravity direction, checking every intermediate frame. Better on arcs, still approximate. |

Known simplifications (documented in `PredictionEngine.cpp`):

- Orb, pad, portal and slope interactions are not simulated.
- Gravity uses a fixed assumed acceleration; only its direction comes from the player.
- The player hitbox comes from `getObjectRect()` scaled down by a constant, not from the
  game's internal collision shape.
- Only `Hazard`, `AnimatedHazard` and slope-hazard objects count as lethal.

## Settings

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `predictive-buffer` | bool | `true` | Master switch for the click buffer |
| `buffer-in-cube-mode` | bool | `false` | Also buffer in cube/ball/robot/spider; off to protect required jumps |
| `window-size` | int | `8` | Base window in frames |
| `extended-window-size` | int | `16` | Window on dense sections |
| `density-threshold` | int | `12` | Nearby hazards needed for the extended window |
| `scan-radius` | float | `200` | Hazard search radius in units |
| `prediction-mode` | string | `aabb` | `aabb` or `trajectory` |
| `humanization-chance` | int | `80` | Chance to use the earliest safe frame |
| `release-protection` | bool | `true` | Enable delayed Left/Right releases |
| `release-delay-ms` | int | `20` | Release delay |
| `debug-logs` | bool | `false` | Verbose logging and session summary |

## Notes

- Frame = one `GJBaseGameLayer::update` call, not a fixed 1/60 s step.
- The AABB mode extrapolates linearly and ignores gravity, so on ground sections it can
  call a frame "safe" that is really a fall into a hazard. That is why ground-mode
  buffering is off by default; enable `buffer-in-cube-mode` only for experimentation.
- If `syzzi.click_between_frames` (or any mod that calls `GJBaseGameLayer::update` more
  than once per rendered frame) is installed, every call counts as one frame. The N-frame
  delay then spans less real time, and the ms-based release delay spans more updates.
- Each press scans `m_objects` once with `scan-radius` filtering, which is O(n) in the
  level's object count. On very large levels a click can cost around a millisecond;
  nothing is scanned while no press arrives.
- The installed mod `ai.predictivebuffer` hooks the same functions. Disable or remove it
  before testing, otherwise both buffers act on the same click.
- The mod ID is `lilwc.predictive-input-buffer`; change the `id`, `developer` and
  `version` fields in `mod.json` before publishing.

## Building

With the Geode SDK installed and `GEODE_SDK` set:

```sh
geode build
```

The build output is a `.geode` file next to the project; drop it into
`<GD>/geode/mods/`.
