<p align="center">
  <img src="images/logo.png" alt="clawdmeter plus" width="760">
</p>

# Clawdmeter Plus

A tiny round AMOLED desk display that shows your live **Claude Code** usage, the
time, the London weather, the health of your background agents, and an animated
pixel mascot - and greets you out loud twice a day. It runs on a
[Waveshare ESP32-S3-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm?&aff_id=149786)
and talks to a small macOS daemon over Bluetooth LE.

> This is a personal fork/extension of the open-source
> [**Clawdmeter** by Hermann Bjorgvin](https://github.com/HermannBjorgvin/Clawdmeter).
> It is an independent, **unofficial** hobby project and is not affiliated with,
> endorsed by, or sponsored by Anthropic. See [Credits & License](#credits--license).

<p align="center">
  <img src="images/photo-usage.jpg" alt="Clawdmeter on a desk showing the usage screen" width="330">
  <img src="images/photo-status.jpg" alt="Clawdmeter on a desk showing the status screen" width="330">
</p>

> The finished build on my desk. The crisp screenshots further down are captured
> straight off the panel's framebuffer.

---

## What it shows

The display has two pages. Press the middle **PWR** button to flip between them.

### 1. Usage screen (the core)

Your live Claude Code limits, straight from the Anthropic usage API:

- **Session limit** (the rolling 5-hour window) and **weekly limit**, as rings/bars
- Current burn rate, so you can see how fast you're spending
- An animated **Clawd** pixel mascot
- Optional clock in place of the "Usage" title

<p align="center">
  <img src="images/usage.png" alt="Usage screen showing session and weekly Claude Code limits" width="300">
  <img src="images/splash.png" alt="Animated Clawd mascot splash" width="300">
</p>

### 2. Status screen (this fork's additions)

A bento-style dashboard:

- **Clock** (12h/24h)
- **London weather** - current temperature + a condition icon (open-meteo, no API key)
- **Tomorrow** - tomorrow's forecast high + icon
- **Agent dots** - five health dots for background Claude agents running in a local
  `tmux` session, each with an identity icon. Whichever agent is *actively working*
  gets a **pulsing orange highlight** behind its cell.
- The animated mascot, on this page too

<p align="center">
  <img src="images/agents.png" alt="Status screen with a pulsing orange highlight on the working agents" width="300">
  <img src="images/status-alt.png" alt="Alternate status screen capture" width="300">
</p>

The agent-dots panel is tailored to a specific multi-agent setup (it looks for
named `tmux` windows). If you don't run agents that way the dots simply stay
idle - everything else works unchanged. See [Customizing](#customizing).

### Controls

| Action | What it does |
| --- | --- |
| Tap **PWR** (middle button) | Cycle pages: Usage ⟷ Status |
| Hold **PWR** ~3 s | Pairing mode (clears the Bluetooth bond) |
| Side buttons | BLE HID keys you can use inside Claude Code (e.g. push-to-talk) |

---

## Hardware

| Part | Notes |
| --- | --- |
| **[Waveshare ESP32-S3-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm?&aff_id=149786)** | The board. 480×480 round AMOLED, capacitive touch, IMU, onboard speaker, USB-C. This is the main build target (`waveshare_amoled_216`). |
| **LiPo battery** (optional) | A single-cell 3.7 V LiPo with the board's JST connector makes it cordless. The onboard AXP2101 charges it over USB by default. Mind the connector polarity. |
| **Onboard speaker** | The 2.16" board has an ES8311 codec + speaker, used for the session-reset chime and the [daily voice](#the-daily-voice). |
| **USB-C data cable** | Needed to flash. Use a real **data** cable, not a charge-only one. |

Other boards in the same family are also supported by the firmware - see
[Supported boards](#supported-boards).

> On battery the device sleeps after a few minutes idle to save power and wakes
> on a PWR press; on USB it stays always-on. A dark screen after a night on
> battery is normal - just tap PWR.

---

## Assembly

1. If you're adding a battery, plug the LiPo into the board's battery JST
   connector (check polarity), and seat it in the case.
2. Connect the board to your Mac with a USB-C **data** cable.
3. That's the whole build - flash the firmware, then run the daemon.

*(Enclosure/wiring photos will be added here.)*

---

## Flash the firmware

The firmware is a [PlatformIO](https://platformio.org/) project (C++ / LVGL 9).

**Prerequisites**

```bash
brew install platformio        # provides the `pio` CLI
```

**Flash (convenience script, auto-detects the USB port):**

```bash
./flash-mac.sh waveshare_amoled_216
# or pin the port explicitly:
./flash-mac.sh waveshare_amoled_216 /dev/cu.usbmodem1101
```

**Flash (raw PlatformIO):**

```bash
pio run -e waveshare_amoled_216 -t upload
```

If no `/dev/cu.usbmodem*` device is found, the cable is charge-only or the board
needs download mode - hold **BOOT** while plugging in, then flash.

Monitor serial output with:

```bash
pio device monitor -e waveshare_amoled_216 -b 115200
```

---

## Run the Mac daemon

The board is just a screen; a small Python daemon (`bleak` + `httpx`) is the
brains. It reads your Claude usage, fetches the weather, checks agent health, and
pushes a compact JSON payload to the device over BLE every ~60 s.

**It never stores a token in this repo.** On macOS it reads your Claude Code
OAuth token from the login **Keychain** (service `Claude Code-credentials`) at
runtime - the same credential Claude Code itself uses. You just need to be
signed in to Claude Code.

**Install:**

```bash
./install-mac.sh
```

This will:

1. Create a Python virtualenv at `daemon/.venv` (installs `bleak` + `httpx`).
2. Render and load a **launchd** agent (`com.user.claude-usage-daemon`) so the
   daemon starts at login and restarts if it crashes.
3. Optionally install `blueutil` (auto-recovers the BLE bond after a reflash).
4. Ask a couple of config questions (clock, chime, multi-plan).

**Pair the device** (once, after flashing):

1. Power on the board.
2. System Settings → **Bluetooth** → **Connect** next to **"Clawdmeter"**.
3. macOS will ask to allow Bluetooth for the daemon - click **Allow**. (A
   "Keyboard Setup Assistant" window may pop up because the board also exposes
   BLE HID keys - just close it; don't press any keys.)

The daemon discovers the paired device within ~30 s and starts sending.

**Logs & control:**

```bash
tail -F ~/Library/Logs/claude-usage-daemon.out.log     # "Sending: {…ok:true}" = healthy
launchctl unload -w ~/Library/LaunchAgents/com.user.claude-usage-daemon.plist  # stop
launchctl load   -w ~/Library/LaunchAgents/com.user.claude-usage-daemon.plist  # start
```

> **Linux / Windows:** the upstream project also ships a `systemd` unit
> (`daemon/claude-usage-daemon.sh`, `install.sh`) and a Windows daemon + tray
> (`daemon/*_windows.*`, `install-windows.ps1`, `daemon/README-windows.md`).
> This fork's extra features are developed and tested on macOS.

### Keep the token warm (optional)

The Keychain OAuth token has a short TTL and only refreshes when *some* Claude
Code process makes a call. During long idle stretches it can expire and the
device shows stale data until the next activity. `daemon/token_keepwarm.sh` is a
tiny launchd job that spends one throwaway `claude -p` call only when the token
is within ~6 min of expiry, forcing a refresh. Optional but nice for an
always-on desk display.

---

## Configuration

Daemon options live in a plain config file that's re-read every poll (no restart
needed):

```
~/.config/claude-usage-monitor/config
```

Copy [`daemon/config.example`](daemon/config.example) there to start. Keys:

- `clock` - `off` / `auto` / `12` / `24` (show a clock on the usage screen)
- `chime` - `on` / `off` (play a sound through the speaker when your 5-hour
  session limit resets)
- `config_dirs` - poll more than one `~/.claude*` plan and show whichever is
  active

---

## The daily voice

The board speaks to you through its onboard speaker twice a day:

- **08:00** - a short motivating morning greeting
- **20:00** - a wind-down / rest message in the evening

Each plays once per day, scheduled off the device clock. The audio is embedded
in the firmware as 12 kHz PCM (`firmware/src/voice_morning_pcm.h` and
`voice_evening_pcm.h`) and played back via the ES8311 codec.

**Test the clips over serial** (no need to wait for the hour):

```
voice1   # play the morning clip
voice2   # play the evening clip
```

**Change the times:** edit the schedule in `firmware/src/main.cpp` (search for
`voice_schedule_tick`) - the two lines compare the current minute-of-day against
`8 * 60` and `20 * 60`. Change those and reflash.

**Change the messages / language:** the easiest way is the included helper.
Type your text, in any language, and it regenerates the header for you:

```bash
tools/gen_voice.sh morning en "Good morning! Have a productive day."
tools/gen_voice.sh evening uk "Добрий вечір. Час відпочивати."
# then reflash:
./flash-mac.sh waveshare_amoled_216
```

It uses free Google Translate TTS (no API key) and writes a 12 kHz / 16-bit /
mono PCM header with the right symbol names. Needs `ffmpeg` (`brew install
ffmpeg`). Keep clips short - they live in the app partition alongside the
firmware. Prefer your own audio? Any 12 kHz/16-bit/mono PCM emitted as a C byte
array in the same shape works too (mirror `bell_pcm.h`).

> The bundled clips are in **English** (generic greetings, so anyone can use the
> build as-is). Swap them for your own language or wording with the command
> above.

---

## Customizing

- **Weather city** - the daemon fetches London by default. Edit the open-meteo
  latitude/longitude (the `WX_URL` request) in
  `daemon/claude_usage_daemon.py` for your city.
- **Agent dots** - the daemon lights these from named background agents in a
  local `tmux` session. If you don't use that setup, the dots stay idle and
  harmless; or adapt `add_agent_health_fields` / `add_agent_busy_fields` in the
  daemon to your own signal.
- **Add a board** - the firmware is structured around a small HAL so new panels
  drop into `firmware/src/boards/<name>/`. See
  [`docs/porting/adding-a-board.md`](docs/porting/adding-a-board.md).

---

## Troubleshooting

| Symptom | Likely cause / fix |
| --- | --- |
| Screen dark / not updating | Usually **battery sleep**, not a fault. Tap PWR to wake, or keep it on USB (never sleeps on USB). Check `blueutil --connected \| grep -i clawd` - absent = asleep. |
| No Claude data on screen | Token issue. Look for `HTTP 401` in the daemon log. Make sure you're signed in to Claude Code; consider the [keep-warm helper](#keep-the-token-warm-optional). |
| Daemon never connects under launchd | It needs its **own** Bluetooth permission. Grant Python under System Settings → Privacy & Security → Bluetooth, then `launchctl kickstart -k gui/$(id -u)/com.user.claude-usage-daemon`. |
| `install-mac.sh` hangs in a non-interactive shell | Step [5/6] runs a foreground priming scan. Run it as `echo n \| ./install-mac.sh` to skip that and still load the launchd agent. |
| BLE won't reconnect after a reflash | Stale bond. Install `blueutil` (the installer offers this) for auto-recovery, or "Forget This Device" in Bluetooth settings and re-pair. |

---

## Supported boards

The firmware supports four boards from the same family (each is a PlatformIO
env). This build targets the first:

- `waveshare_amoled_216` - **Waveshare ESP32-S3-Touch-AMOLED-2.16** (480×480, main target)
- `waveshare_amoled_18` - Waveshare ESP32-S3-Touch-AMOLED-1.8 (368×448)
- `waveshare_amoled_216_c6` - ESP32-C6 variant (no PSRAM)
- `waveshare_amoled_18_c6` - ESP32-C6 variant (no PSRAM)

Audio features (chime, daily voice) require a board with the onboard codec/speaker
(the 2.16" and 1.8" S3 boards).

---

## Repository layout

```
firmware/          PlatformIO project (C++ / LVGL 9)
  platformio.ini   board envs + build config
  src/             shared UI + per-board HAL (src/boards/, src/hal/)
daemon/            macOS/Linux/Windows host daemon (Python) + install scripts
  claude_usage_daemon.py   the macOS daemon (bleak + httpx, BLE push)
  config.example           daemon config template
  token_keepwarm.sh        optional OAuth token keep-warm (launchd)
tools/             icon/font/sprite generators
docs/porting/      how to add a new board
assets/            fonts, icons, demo media (see LICENSE notice)
images/            screenshots used in this README
flash-mac.sh       build + upload firmware (macOS)
install-mac.sh     set up the daemon (venv + launchd)
```

---

## Credits & License

- Built on the open-source **[Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter)**
  by **Hermann Bjorgvin** - the original ESP32 Claude usage display, its firmware
  architecture, and the Clawd mascot integration. Huge thanks. ⭐
- Weather by [open-meteo](https://open-meteo.com/) (free, no key).
- Built with [PlatformIO](https://platformio.org/), [LVGL](https://lvgl.io/),
  [bleak](https://github.com/hbldh/bleak), and [httpx](https://www.python-httpx.org/).

**License:** the source code is offered under the **MIT License**. However, this
repo (like upstream) bundles **proprietary fonts** (Styrene B, Tiempos) and the
**copyrighted Clawd / Claude mascot artwork**, which are the property of their
owners and are **not** covered by MIT. "Claude", "Claude Code", and "Anthropic"
are trademarks of Anthropic PBC. **Read [`LICENSE`](LICENSE) in full before
redistributing** - if you publish a build, you're responsible for the rights to
those assets (or replace them). The upstream author's original notes are kept in
[`README.upstream.md`](README.upstream.md).

---

## Author

Made by **Oleh** - [@iam.oleh on Instagram](https://www.instagram.com/iam.oleh).
Follow along for more desk-hardware and AI tinkering.
