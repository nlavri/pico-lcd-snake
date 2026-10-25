# Original 2D Platformer Plan

## Decision

Build an original, 8-bit-inspired action platformer as a normal launcher app
first. Do not start from a Mario, Contra, or other commercial-game port, and do
not copy their characters, levels, art, music, or exact rules.

On the 240 x 135 Waveshare target, the first version should coexist with the
current Pico W network and SSH features. The game can fit if its mutable state
stays inside the existing app state pool and its art and level data remain
`const` in flash. The 240 x 240 SpotPear build is already much closer to its
static-RAM limit, so it must be held to a no-new-BSS rule and may need the
game-focused preset sooner. Add feature switches from the beginning so that
preset can exclude SSH and other network applications without forking code.

The main engineering risk is LCD throughput, not game-state memory. Rendering
work should precede production-level content.

## Current constraints

Measurements are from the current debug ELF files and source layout.

| Resource | Current value | Consequence |
| --- | ---: | --- |
| RP2040 SRAM | 256 KiB | Shared by framebuffer, FreeRTOS, lwIP, stacks, heaps, and app state |
| Pico W Waveshare linked static RAM | about 196 KiB through the end of BSS | About 60 KiB of raw address-space headroom remains |
| Pico W SpotPear BSS end | `0x2003f0e0` in the current map | Only about 3.8 KiB remains below the 256 KiB RAM boundary |
| Pico W flash image | about 423 KiB | Plenty of the 2 MiB flash remains for indexed art and levels |
| App state pool | 4 KiB | All enabled app states share this fixed pool; measure the remaining aligned capacity before registering the game |
| Waveshare framebuffer | 240 x 135 x 16-bit = 64.8 KiB | Keep the existing single framebuffer |
| SpotPear framebuffer | 240 x 240 x 16-bit = 115.2 KiB | Avoid any second framebuffer or large render cache |
| Waveshare LCD SPI clock | 62.5 MHz | DMA full-frame upload measured 9.881-9.893 ms at 240 x 135, clean during moving Snake gameplay |
| SpotPear LCD SPI clock | 62.5 MHz | DMA full-frame upload measured 19.223 ms at 240 x 240, clean during moving Snake gameplay |
| UI task cadence | nominal 5 ms | A blocking full-screen upload currently dominates the frame time |
| Inputs | joystick, click, A, B | Enough for move, jump, attack, and pause/interact |

The Pico W image already reserves a 32 KiB FreeRTOS heap. The SSH task and
session allocations also consume that heap at runtime, so the game should not
depend on heap allocation and should not keep decompressed asset caches.

## Game concept and scope

Working concept: **Signal Runner**, an original compact action platformer set
inside a failing relay station. The player restores three signal cores while
crossing machinery, vents, and overgrown exterior structures. The visual
language may use a limited palette, hard silhouettes, tile art, and small
animation sets without imitating the assets or level grammar of a particular
retro title.

The first playable slice should contain:

- one player with run, variable-height jump, coyote time, jump buffering, and
  a short-range pulse attack;
- one 2-3 minute level, one checkpoint, an exit, hazards, moving platforms,
  and three enemy behaviours;
- a title/pause/death/complete flow and a small HUD;
- three short animation sets for the player: idle, run, and jump/fall, plus
  attack and damage frames;
- deterministic integer simulation and no audio in the first milestone.

Suggested controls:

| Input | Action |
| --- | --- |
| Left/right | Run |
| Down | Drop/crouch where supported |
| A | Jump; release early for a shorter jump |
| B | Pulse attack |
| Joystick click | Pause/interact |
| A+B held | Existing global return-to-launcher gesture |

The game must track press and release events as a held-button mask. Direction
repeat events are suitable for menus, not continuous movement. During play,
ignore repeat events and update motion from the held mask each simulation tick.
The global A+B gesture should be tested carefully because simultaneous jump and
attack input can otherwise return to the launcher after 750 ms.

## Simulation architecture

Keep the rules independent of the Pico SDK, FreeRTOS, display, and networking:

```text
apps/platformer/
  platformer_app.c       launcher lifecycle and input adaptation
  platformer_game.c/.h   deterministic simulation and collisions
  platformer_render.c/.h tile/sprite renderer
  platformer_assets.c/.h const indexed art and palettes
  platformer_levels.c/.h const compressed/chunked level data
  platformer_link.c/.h   transport-neutral two-player messages
```

Use a fixed 60 Hz simulation step with an accumulator and render at 30 Hz when
the display path can sustain it. Cap catch-up work to avoid a long LCD transfer
causing an unbounded update spiral. All gameplay calculations should use
integers or fixed point, for example signed 24.8 positions and velocities.

Recommended fixed limits for the first slice:

- 16 x 16 pixel tiles and a 240-pixel-wide camera;
- at most 24 active actors, 8 projectiles, and 8 transient effects;
- axis-separated tile collision against only the tiles crossed by an actor;
- fixed arrays with active flags or a free-list, never per-frame allocation;
- compact actor state with type/state/frame stored as 8-bit values;
- chunked or row-compressed maps read from XIP flash, with only the visible
  neighbourhood decoded into a small scratch buffer.

Target less than 2 KiB for the complete mutable `PlatformerAppState`, then
verify the aligned total of every enabled app fits the existing 4 KiB pool. Make it a transient app initially so
returning from the launcher starts a clean run; later, retain only a compact
checkpoint/save record through the preferences layer.

## Rendering plan

Do not add another RGB565 framebuffer. Store source art in flash as 4-bit
palette indices and convert to RGB565 while blitting. A 16 x 16 tile then costs
128 bytes plus a shared palette instead of 512 bytes. Use transparency as one
palette index and precompute flipped variants only when profiling proves that
it is worthwhile.

Before building many levels, extend the display abstraction with measured game
rendering support:

1. Add clipped filled-rectangle and indexed-sprite blitters that write directly
   to `DisplaySurface.pixels` instead of calling the point primitive per pixel.
2. Add `display_present_region()` so menus, HUD updates, and mostly-static
   scenes can submit dirty rectangles.
3. Both display paths now use board-specific 62.5 MHz presets, verified clean
   during moving gameplay on their respective hardware.
4. DMA-backed asynchronous submission is now implemented. Rendering waits for
   the previous transfer before modifying the single framebuffer, while the CPU
   remains available to FreeRTOS and Pico W services during transmission.
5. The launcher benchmark now instruments CPU render time. On the RP2040
   Waveshare build, the scrolling tile map, parallax background, HUD, player,
   and eight animated indexed sprites render in about 13.5 ms, with 13.872 ms
   maximum observed. Still add end-to-end frame cadence, missed simulation
   ticks, minimum FreeRTOS heap, and stack high-water marks.

At 30 full frames per second, raw RGB565 payload requires about 15.6 Mbit/s for
240 x 135 and 27.6 Mbit/s for 240 x 240, before command overhead. The tested
Waveshare path completes a full-frame submission in 9.881-9.893 ms when polled
promptly, while the tested SpotPear path completes one in 19.223 ms. Keep
simulation at 60 Hz and initially cap rendering near 50 Hz; dirty regions can
reduce bandwidth for mostly-static scenes.

Use a common 240-pixel-wide world and preserve identical collision and level
coordinates on both displays. The 240 x 240 display may expose more vertical
scenery by shifting the same terrain downward; this changes visibility, not
simulation. Keep gameplay tests and future multiplayer behaviour independent
of the physical viewport height.

## Build strategy

Two independent CMake options gate the optional features without forking the
product:

```cmake
option(APP_ENABLE_PLATFORMER "Include the platformer app" ON)
option(APP_ENABLE_SSH "Include SSH support" ON)
```

The regular Pico and Pico W presets enable both features. SpotPear uses two
mutually focused debug presets because its 115.2 KiB framebuffer leaves little
static SRAM:

| Preset | Contents | When to use |
| --- | --- | --- |
| existing Pico/Pico W presets | launcher, platformer, SSH/network where supported | Default development |
| `apps-picow-spotpear-debug` | platformer and network/weather; SSH omitted | SpotPear game development; about 13.0 KiB static SRAM margin |
| `apps-picow-spotpear-ssh-debug` | SSH and network/weather; platformer omitted | SpotPear SSH testing; about 7.8 KiB static SRAM margin |

The platformer itself adds no separate BSS: its small mutable state lives in
the existing app pool and its art lives in flash. Therefore omitting it saves
flash but essentially no static SRAM; omitting SSH recovers about 5.2 KiB of
static SRAM by removing its service state.

Set explicit build gates rather than deciding from intuition:

- platformer mutable state no more than 4 KiB;
- no more than 512 bytes of new BSS; game state must use the already-reserved
  app pool and large read-only objects must be verified in flash;
- retain at least the current linker margin on SpotPear and preferably reclaim
  enough service/pool memory to reach 12 KiB before a combined release;
- at least 8 KiB minimum-ever-free FreeRTOS heap after an SSH connect, command,
  disconnect, and a complete level run;
- no task stack with less than 25% measured headroom;
- stable 60 Hz simulation and a minimum 25 fps render target on Waveshare;
- stable 60 Hz simulation and a measured, documented render target on SpotPear.

If a combined Pico W build misses a gate, first trim pools/stacks based on
high-water measurements. If that is insufficient, enable the game-only preset;
do not weaken collision, determinism, or memory safety to retain unused
services.

## Two-Pico multiplayer seam

Make the single-player simulation a two-player-capable simulation from the
start, even though the second player is disabled. Represent each player's
input as a small per-tick value and never let core rules read GPIO directly.

```c
typedef struct PlatformerInputFrame {
    uint16_t tick;
    uint8_t buttons;
} PlatformerInputFrame;
```

Define a `PlatformerLink` interface that can initially use a loopback/null
implementation. Add a Pico W UDP implementation later through the existing
network service, keeping sockets and lwIP calls out of the UI/game code.

Recommended first multiplayer mode:

- two-player cooperative play on the same level, each display following its
  local player;
- one Pico acts as host and owns enemy decisions, pickups, checkpoints, and
  level transitions;
- peers exchange tick-numbered input bundles at about 30 packets/s, including
  several recent inputs so an occasional lost UDP packet is harmless;
- run deterministic integer physics with a small input delay and repeat the
  last directional state briefly on packet loss;
- exchange a state checksum periodically and let the host send a compact
  authoritative snapshot when the clients diverge;
- include protocol version, session ID, sequence/ack fields, and a timeout in
  every session design;
- begin with both Pico W boards on the same Wi-Fi network. A host-created access
  point or wired UART transport can be a later adapter behind the same link
  interface.

Do not attempt general rollback networking in the first multiplayer milestone.
For two nearby boards, redundant inputs plus host correction is substantially
simpler and has bounded memory use.

## Delivery milestones

### 0. Prove the display budget (benchmark complete)

The full-frame transport baseline is complete on both displays. At 62.5 MHz,
asynchronous DMA transfers 64,800 bytes to the Waveshare panel in 9.881-9.893
ms and 115,200 bytes to the SpotPear panel in 19.223 ms; both were visually
clean during moving Snake gameplay. Dirty-region submission remains the main
unfinished display experiment.

The first two remaining items are now implemented as `Platformer Demo` in the
existing launcher. On the 240 x 135 RP2040 target, a full procedural tile scene
with parallax, HUD, and eight animated indexed sprites rendered in about 13.5
ms (13.872 ms maximum observed) and remained visually clean. A single
framebuffer prevents CPU rendering from overlapping the active DMA transfer,
so dirty-region submission or targeted renderer improvements remain valuable.

### 1. Host-tested game kernel

Implement fixed-step motion, jump feel, slopes excluded, solid/one-way tiles,
hazards, checkpoints, actor pools, and deterministic replay tests. Feed a saved
input sequence twice and assert identical checksums.

### 2. One playable level in the launcher

Add the app lifecycle, original art, camera, HUD, pause/death/complete screens,
and one polished level. Validate launcher return/restart and both screen sizes.

### 3. Content and performance pass

Add two more short levels, enemies, moving platforms, compact save data, asset
conversion tooling, and on-device memory/stack instrumentation. Decide from the
build gates whether a separate game preset is actually needed.

### 4. Two-board prototype

Add link loopback tests, packet encode/decode tests, UDP discovery/join, host
authority, redundant input exchange, checksums, and snapshot correction. Test
loss, reordering, disconnect, and mismatched protocol versions.

## First implementation slice

This slice is complete: the host-tested indexed blitter and on-device launcher
benchmark scroll a procedural tile map with parallax and eight animated
sprites. SPI, DMA, and full-frame baselines are settled, and real render cost is
recorded above. The next slice should build the host-tested fixed-step game
kernel while dirty-region support is evaluated separately.
