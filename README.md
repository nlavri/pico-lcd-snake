# Pico LCD applications

Small FreeRTOS application firmware for Raspberry Pi Pico and Pico W with a
launcher, Calculator, Snake game, a scrolling Platformer Demo,
onboard-temperature display, Weather, SSH, and Settings apps.

The application firmware supports the Waveshare Pico LCD 1.14 and the
SpotPear 1.54-inch Game LCD HAT used through the project's Pico wiring adapter.

The planned multi-application FreeRTOS and standalone DOOM structure is
documented in [docs/architecture/README.md](docs/architecture/README.md).

The saved Spotpear 1.54-inch HAT connector map is available in
[docs/hardware/spotpear-pico-wiring.html](docs/hardware/spotpear-pico-wiring.html).

## Build

This project uses Raspberry Pi Pico SDK 2.3.1. The application firmware can be
built for the original RP2040-based Pico or Pico W.

```sh
cmake --preset apps-pico-debug
cmake --build --preset apps-pico-debug
```

For Pico W, use the `apps-picow-debug` preset instead. The UF2 outputs are
`build/apps-pico/firmware/apps/pico-apps.uf2` and
`build/apps-picow/firmware/apps/pico-apps.uf2`. Hold BOOTSEL while connecting
the board for the first flash, then copy the matching UF2 to `RPI-RP2`.

For a Pico W wired to the SpotPear display, choose one memory-focused preset:

- `apps-picow-spotpear-debug` includes Platformer Demo and omits SSH. Its UF2
  is `build/apps-picow-spotpear/firmware/apps/pico-apps.uf2`.
- `apps-picow-spotpear-ssh-debug` includes SSH and omits Platformer Demo. Its
  UF2 is `build/apps-picow-spotpear-ssh/firmware/apps/pico-apps.uf2`.

The underlying `APP_ENABLE_PLATFORMER` and `APP_ENABLE_SSH` CMake options can
also be set independently for custom builds. The normal Pico and Pico W
presets keep both enabled.

After the first flash, VS Code's Run and Debug panel provides two workflows:

- `Pico: Upload via USB (no debug)` builds, uploads with `picotool`, and runs.
- `Pico W: Upload via USB (no debug)` does the same using the Pico W build.
- `Pico W SpotPear: Upload via USB (no debug)` builds and uploads the 240 x 240
  SpotPear variant.
- `Pico: Debug with probe` builds, uploads over SWD, and starts GDB.

The standalone no-audio DOOM firmware has its own `Doom: Build`,
`Doom: Flash all with probe`, and `Doom: Debug with probe` workflows. See
[firmware/doom/README.md](firmware/doom/README.md) for its separate firmware and
game-data flash layout.

## Controls

The firmware starts in the application launcher. Use up/down to select an app,
then joystick-right or joystick-click to start or resume it. A shows launcher
help and B restarts the selected app. Application state is retained while the
launcher is visible. Holding A+B for 750 ms inside an application is the global
return-to-launcher gesture; all joystick controls remain available to apps.
Weather, Temperature, and Settings also use B as contextual Back: it returns
home from their top-level screen and goes back one level in nested screens.
Controls that need B, such as Snake speed adjustment and password deletion,
keep their app-specific behavior.

Platformer Demo exercises the game rendering path with a scrolling tile world,
parallax scenery, and animated indexed-color sprites. Left/right chooses the
scroll direction, up/down changes speed, click pauses, A switches between four
and eight sprites, and B resets the camera. The HUD reports CPU render time;
the LCD transfer continues asynchronously through DMA. The 240 x 240 SpotPear
variant exposes more vertical scenery while preserving the same world layout.

The Temperature app samples the RP2040 internal sensor twice per second. Its
large reading moves through a violet, blue, cyan, green, yellow, orange, and red
color spectrum as the measured temperature rises.

The Calculator app supports decimal numbers, unary signs, parentheses, and the
usual `+`, `-`, `*`, and `/` precedence. Move over its keypad with the joystick
and click to press a key. A evaluates from anywhere, B deletes one character,
and B on an empty expression returns to the launcher.

The Weather app is available on Pico W after Wi-Fi is connected. It makes one
asynchronous request to `wttr.in`, which chooses the approximate location from
the Pico's public IP address. Its three cyclic pages show current conditions
(temperature, humidity, wind, and visibility) followed by one page for each of
the next two forecast days (condition, min/max temperature, wind, and rain
chance). Move the
joystick left or right to switch pages and press A to refresh; no API key or
location setting is required.
Weather remains available when the Pico W is offline: it reports the Wi-Fi or
internet state and automatically requests data after Wi-Fi connects.

The SSH app is available only on Pico W. It provides a compact password-based
terminal over Wi-Fi: enter a host name or IPv4 address, user, and password,
then inspect and explicitly accept the server's SHA-256 host-key fingerprint.
The Host row is initially prefilled with `192.168.100.7` for local testing and
remains editable through the virtual keyboard.
Click in the terminal to open the shared virtual keyboard and submit one line
at a time. Passwords are kept only for the current connection attempt and are
not saved to flash or written to logs. Host fingerprints are not persisted in
this first version, so every new connection must be accepted explicitly.

The SSH transport uses a pinned local copy of libssh2 1.11.1 with a small
Pico-specific lwIP/Mbed TLS adapter. The firmware build disables legacy
ciphers, compression, file-transfer features, and unused libssh2 modules; the
vendored sources and BSD license remain in `third_party/libssh2` so the build
does not depend on the upstream repository remaining available.

## Wi-Fi settings

Settings is available on both boards. On a regular Pico its Wi-Fi row is
disabled and marked `Requires Pico W`. On Pico W, open Settings, choose Wi-Fi,
and press A on Networks to scan.

In the network list, up/down selects a network, joystick-right or click opens
it, left goes back, and A rescans. A secured network opens a compact virtual
keyboard with a permanent number row, QWERTY letters, one-shot Shift, and a
symbols page. Move with the joystick, click to activate the highlighted key,
use B to delete (or go back when empty), and A to connect. Click Shift twice
to lock uppercase; click it again to unlock. A successful connection is saved
and restored on the next boot. A on the Wi-Fi status screen forgets the saved
network.

Credentials are stored as plain data in the final 4 KiB flash sector. They are
never printed to the log, but the storage is not encrypted and should not be
treated as protection against physical access to the device.

The UI uses one shared compact header, footer, and menu style across the
launcher and system apps. Settings -> Accent color offers Violet, Blue, Sky,
Mint, Terminal Green, Amber, Orange, Orange Red, and Dark Red. Moving through
the palette previews the choice, click saves it, and left cancels the preview.
The saved accent occupies the
second-to-last 4 KiB flash sector and survives firmware updates. It colors UI
chrome and interaction cues; app-specific semantic colors, such as temperature
and weather-condition colors, remain independent.

Settings -> Brightness controls the display backlight from 10% to 100% in 10%
steps. Up/down previews the level, click saves it, and left restores the
previous value. Brightness is stored with the accent preference. The directly
wired SpotPear HAT does not expose a backlight-control pin, so this setting is
retained but cannot change that display's brightness.

For a locally preconfigured network, copy
`firmware/apps/network_defaults.local.example.h` to
`firmware/apps/network_defaults.local.h` and fill in the values. The local file
is ignored by Git. Saved credentials take priority; when none exist, the
firmware tries the local default and stores it after a successful connection.

For local SSH test defaults, copy
`apps/ssh/ssh_defaults.local.example.h` to
`apps/ssh/ssh_defaults.local.h` and fill in the host, username, and password.
The local file is ignored by Git and the password stays masked in the UI. These
values are compiled into the local firmware image, so use only disposable test
credentials and do not distribute that build artifact.

On Pico W, FreeRTOS runs the UI task, the application-owned network and SSH
tasks, the SDK-owned CYW43 driver task (`async_context_task`), and the SDK-owned
lwIP TCP/IP task on RP2040 core 0. The last is named `tcpip_thread` by lwIP, but
is created as a FreeRTOS task. The CYW43439 chip handles radio and 802.11 work;
lwIP and application networking run on RP2040.
