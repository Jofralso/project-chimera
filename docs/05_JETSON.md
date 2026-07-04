# Jetson Orin Nano Deployment

## Hardware

- NVIDIA Jetson Orin Nano 8GB
- Official 7" touchscreen (800x480, DSI) or HDMI display with touch overlay
- Power supply: 19V / 4.74A barrel jack (use official NVIDIA supply)
- Storage: 32GB+ microSD (U3/V30) or NVMe (recommended for samples)

## One-Command Setup

```bash
git clone https://github.com/Jofralso/project-chimera.git
cd project-chimera
sudo ./scripts/setup_jetson.sh
```

This installs dependencies, builds, runs tests, installs binaries to `/opt/chimera/bin/`, sets up the systemd autostart service, and starts Chimera on boot.

### User-only mode (no sudo)

```bash
git clone https://github.com/Jofralso/project-chimera.git
cd project-chimera
./scripts/setup_jetson.sh --user
```

Installs to `~/.local/bin/` without systemd.

## What Gets Installed

| Path | Purpose |
|---|---|
| `/opt/chimera/bin/chimera-desktop` | Main GUI app |
| `/opt/chimera/bin/chimera-play` | CLI audio engine |
| `/opt/chimera/sessions/` | `.chimera` session files |
| `/etc/systemd/system/chimera-desktop.service` | Autostart unit |
| `/etc/asound.conf` | Default ALSA config |

## Running

### Via systemd (autostart)

```bash
sudo systemctl start chimera-desktop
sudo systemctl status chimera-desktop
journalctl -u chimera-desktop -f
```

### Direct invocation

```bash
chimera-desktop --touch --alsa --scale 3
```

### CLI mode

```bash
chimera-play --seq --alsa --drum-bpm 128
```

## Display Drivers

Chimera auto-sets `SDL_VIDEODRIVER=kmsdrm` when built with `-DCHIMERA_JETSON=ON`.
This gives direct DRM/KMS rendering without X11 — **but only when running locally
on the Jetson console or via sudo.**

| Driver | When to use | Notes |
|---|---|---|
| `kmsdrm` | Local console (not SSH), or via sudo | Direct DRM/KMS, hardware accelerated |
| `x11` | Over SSH with X forwarding, or under GDM/LightDM | Runs as a window inside the desktop |
| `wayland` | If running under a Wayland compositor | Weston, GNOME, etc. |
| `directfb` | If kmsdrm/x11/wayland fail | Requires `libdirectfb-dev` |
| `fbcon` | Last resort | Pure framebuffer, no acceleration |

If `kmsdrm` fails with `SDL_Init failed: kmsdrm not available`, the exact fix
depends on how you are accessing the Jetson:

```bash
# If running over SSH (most common cause):
export SDL_VIDEODRIVER=x11
chimera-desktop --touch --alsa --scale 3

# If running locally on the Jetson's own desktop:
# Just run it normally — kmsdrm should work automatically

# To auto-detect the best available driver:
unset SDL_VIDEODRIVER
chimera-desktop --touch --alsa --scale 3
```

## Touchscreen

The official Jetson 7" DSI display is auto-detected. For USB touchscreens:

```bash
# List input devices
cat /proc/bus/input/devices | grep -i touch

# Test raw events
sudo evtest /dev/input/eventX

# SDL2 with kmsdrm handles touch automatically if the device appears
```

If touch is unresponsive:
1. Verify the touchscreen appears in `/proc/bus/input/devices`
2. Check SDL2 can see it: `export SDL_VIDEODRIVER=kmsdrm && chimera-desktop --touch --alsa`
3. Try a different scale: `--scale 2` or `--scale 4`

## Audio (ALSA)

ALSA is the primary backend. The setup script writes `/etc/asound.conf` pointing to `hw:0,0`.

```bash
aplay -l                    # list devices
arecord -l                  # list capture devices
aplay /usr/share/sounds/alsa/Front_Left.wav  # test playback
```

To use a different device:
```bash
chimera-desktop --touch --alsa --device hw:1,0
```

### USB Audio Interfaces

```bash
# Plug in USB audio, then:
aplay -l
# Note the card number, then:
export AUDIODEV=hw:1,0
chimera-desktop --touch --alsa --device hw:1,0
```

### Capture / Input Passthrough

```bash
chimera-play --capture --alsa --device hw:0,0
```

## CPU Affinity & Real-Time

The systemd service pins Chimera to CPU 0 and sets `nice=-20` for lowest latency.

```bash
# Check affinity
taskset -pc $(pgrep chimera-desktop)

# Check scheduling policy
chrt -p $(pgrep chimera-desktop)

# Manual real-time run (if not using systemd)
sudo chrt -f 50 chimera-desktop --touch --alsa --scale 3
```

### Power Modes

```bash
# Max performance (15W)
sudo nvpmodel -m 0

# Balanced (10W)
sudo nvpmodel -m 1

# Low power (5W)
sudo nvpmodel -m 2
```

## Filesystem Layout

```
/opt/chimera/
├── bin/
│   ├── chimera-desktop
│   └── chimera-play
├── lib/
│   └── (shared libraries if statically linked)
└── sessions/
    └── (user .chimera files)
```

User-mode install uses `~/.local/bin/` and `~/.local/share/chimera/sessions/`.

## Updating

```bash
cd /home/jetson/project-chimera
git pull
sudo ./scripts/setup_jetson.sh
```

Or just rebuild without reinstalling:
```bash
cd /home/jetson/project-chimera
git pull
cd build-jetson
make -j$(nproc)
sudo systemctl restart chimera-desktop
```

## Uninstalling

```bash
sudo systemctl stop chimera-desktop
sudo systemctl disable chimera-desktop
sudo rm /etc/systemd/system/chimera-desktop.service
sudo systemctl daemon-reload
sudo rm -rf /opt/chimera
sudo rm /usr/local/bin/chimera-desktop /usr/local/bin/chimera-play
```

---

# Troubleshooting

## Build Issues

### `SDL.h: No such file or directory`

```bash
sudo apt install -y libsdl2-dev
```

### `alsa/asoundlib.h: No such file or directory`

```bash
sudo apt install -y libasound2-dev
```

### Out of memory during build

```bash
# Reduce parallel jobs
make -j2
# Or disable tests to save RAM
cmake .. -DCHIMERA_BUILD_TESTS=OFF
```

## Runtime Issues

### Blank screen / no display

```bash
# Check display is detected
cat /sys/kernel/debug/dri/0/status

# Try a different SDL driver
export SDL_VIDEODRIVER=directfb
chimera-desktop --touch --alsa

# Check HDMI is enabled in Jetson IO
sudo /opt/nvidia/jetson-io/jetson-io.py
```

### No touch input

```bash
# List touch devices
cat /proc/bus/input/devices | grep -i touch

# Test with evtest
sudo evtest /dev/input/eventX

# Ensure kmsdrm is used (required for touch on most Jetson displays)
export SDL_VIDEODRIVER=kmsdrm
chimera-desktop --touch --alsa

# If still no touch, try without --touch to see if SDL detects it
chimera-desktop --alsa
```

### No audio

```bash
# Verify ALSA sees the device
aplay -l

# Test playback directly
speaker-test -c 2 -t sine -f 440

# Check asound.conf
cat /etc/asound.conf

# Try explicit device
chimera-desktop --touch --alsa --device hw:0,0

# If using USB audio, ensure it's the default or pass --device
```

### Audio crackling / dropouts

```bash
# Set CPU to max performance
sudo nvpmodel -m 0

# Increase process priority manually (if not using systemd)
sudo chrt -f 50 chimera-desktop --touch --alsa

# Check for thermal throttling
sudo tegrastats
# If CPU is throttled, improve cooling

# Reduce block size in engine config (advanced, requires code change)
```

### Overheating / throttling

```bash
# Monitor temperature and clocks
sudo tegrastats

# If CPU freq is below max, check cooling
# Active cooling (fan/heatsink) is required for sustained load

# Set max performance mode
sudo nvpmodel -m 0

# Disable GPU boost if not needed (saves power/heat)
sudo jetson_clocks --store
```

### Low frame rate / laggy UI

```bash
# Reduce window scale
chimera-desktop --touch --alsa --scale 2

# Max performance mode
sudo nvpmodel -m 0

# Check if running under X11 (kmsdrm is faster)
echo $XDG_SESSION_TYPE
# Should be empty or "tty" for kmsdrm
```

### systemd service fails to start

```bash
# Check logs
journalctl -u chimera-desktop -e

# Common causes:
# 1. SDL can't open display — ensure kmsdrm or a display server is running
# 2. No audio device — check aplay -l
# 3. Permission issues — ensure the service runs as the correct user

# Test manually first
sudo -u jetson chimera-desktop --touch --alsa --scale 3

# If it works manually but not in systemd, check:
# - Environment=SDL_VIDEODRIVER=kmsdrm in the service file
# - User/Group in the service file match your setup
```

### MIDI not working

```bash
# List MIDI ports
aconnect -i
aconnect -o

# Connect MIDI devices
aconnect 14 0

# Chimera logs MIDI device open on startup
journalctl -u chimera-desktop | grep -i midi
```

### Session save/load fails

```bash
# Ensure sessions directory exists and is writable
ls -la /opt/chimera/sessions/
sudo chown -R jetson:jetson /opt/chimera/sessions

# For user-mode:
mkdir -p ~/.local/share/chimera/sessions
```

## Performance Tips

```bash
# Max performance profile
sudo nvpmodel -m 0
sudo jetson_clocks --store

# Verify clocks are maxed
sudo jetson_clocks --show

# Disable desktop compositor if running X11
# (kmsdrm mode doesn't need this)
```

## Support

- Issues: https://github.com/Jofralso/project-chimera/issues
- Docs: `docs/02_ARCHITECTURE.md`, `docs/03_Interfaces.md`, `docs/04_UX_UI.md`
