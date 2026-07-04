#!/usr/bin/env bash
set -euo pipefail

# Jetson Orin Nano — one-command setup for Project Chimera
#
# Usage:
#   git clone https://github.com/Jofralso/project-chimera.git
#   cd project-chimera
#   sudo ./scripts/setup_jetson.sh
#
# Or for the current user (no sudo, no systemd):
#   ./scripts/setup_jetson.sh --user

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INSTALL_MODE="system"
SERVICE_NAME="chimera-desktop"

usage() {
  cat <<EOF
Usage: $0 [--user]

Modes:
  (default)  Install system-wide with systemd autostart (requires sudo)
  --user     Install to ~/.local/ only (no systemd)

EOF
  exit 1
}

for arg in "$@"; do
  case "$arg" in
    --user) INSTALL_MODE="user" ;;
    -h|--help) usage ;;
  esac
done

info()  { echo -e "\033[1;32m[INFO]\033[0m  $*"; }
warn()  { echo -e "\033[1;33m[WARN]\033[0m  $*"; }
error() { echo -e "\033[1;31m[ERR]\033[0m   $*" >&2; exit 1; }

# Detect model
detect_model() {
  if [ -f /etc/nv_tegra_release ]; then
    grep -oP 'Tegra\d+' /proc/device-tree/model 2>/dev/null || echo "Jetson"
  else
    warn "Not running on a Jetson device — continuing anyway"
    echo "unknown"
  fi
}

MODEL=$(detect_model)
info "Detected: $MODEL"

# ---------------------------------------------------------------------------
# Install system packages
# ---------------------------------------------------------------------------
info "Installing system dependencies..."
if [ "$INSTALL_MODE" = "system" ]; then
  if [ "$(id -u)" -ne 0 ]; then
    error "System install requires root. Re-run with sudo, or use --user."
  fi

  apt-get update -qq
  apt-get install -y --no-install-recommends \
    build-essential cmake git \
    libsdl2-dev libasound2-dev libasound2-plugins \
    libdrm-dev libevdev-dev \
    python3

  info "Dependencies installed."
else
  warn "User mode — skipping apt-get. Ensure build-essential, cmake, libsdl2-dev, libasound2-dev are installed."
fi

# ---------------------------------------------------------------------------
# Configure + Build
# ---------------------------------------------------------------------------
BUILD_DIR="${PROJ_ROOT}/build-jetson"
info "Configuring build (${BUILD_DIR})..."
mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR"

if [ "$INSTALL_MODE" = "system" ]; then
  cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCHIMERA_JETSON=ON \
    -DCHIMERA_USE_JACK=OFF \
    -DCHIMERA_USE_PIPEWIRE=OFF \
    -DCHIMERA_BUILD_TESTS=ON
else
  cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCHIMERA_JETSON=ON \
    -DCHIMERA_USE_JACK=OFF \
    -DCHIMERA_USE_PIPEWIRE=OFF \
    -DCHIMERA_BUILD_TESTS=ON
fi

info "Building (this takes 5-10 min on Orin Nano)..."
make -j"$(nproc)"

info "Running tests..."
ctest --output-on-failure

cd "$PROJ_ROOT"

# ---------------------------------------------------------------------------
# Install binaries
# ---------------------------------------------------------------------------
if [ "$INSTALL_MODE" = "system" ]; then
  BIN_DIR="/opt/chimera/bin"
  SESSION_DIR="/opt/chimera/sessions"
  install -d -m 755 "${BIN_DIR}"
  install -d -m 755 "${SESSION_DIR}"

  info "Installing binaries to ${BIN_DIR}..."
  install -m 755 "${BUILD_DIR}/software/apps/chimera-desktop" "${BIN_DIR}/"
  install -m 755 "${BUILD_DIR}/software/apps/chimera-play"    "${BIN_DIR}/"

  # Also link to /usr/local/bin for convenience
  ln -sf "${BIN_DIR}/chimera-desktop" /usr/local/bin/chimera-desktop
  ln -sf "${BIN_DIR}/chimera-play"    /usr/local/bin/chimera-play

  # Permissions
  chown -R root:root "${BIN_DIR}" "${SESSION_DIR}"

  # -----------------------------------------------------------------------
  # systemd service
  # -----------------------------------------------------------------------
  info "Installing systemd service..."
  if [ -f "${PROJ_ROOT}/systemd/${SERVICE_NAME}.service" ]; then
    install -D -m 644 "${PROJ_ROOT}/systemd/${SERVICE_NAME}.service" \
      "/etc/systemd/system/${SERVICE_NAME}.service"

    # Reload + enable
    systemctl daemon-reload
    systemctl enable "${SERVICE_NAME}"
    systemctl restart "${SERVICE_NAME}"

    info "Service installed. Status:"
    systemctl --no-pager status "${SERVICE_NAME}" || true
  else
    warn "systemd unit not found at systemd/${SERVICE_NAME}.service — skipping"
  fi

  # -----------------------------------------------------------------------
  # ALSA config
  # -----------------------------------------------------------------------
  info "Writing default ALSA config..."
  if [ ! -f /etc/asound.conf ]; then
    cat > /etc/asound.conf <<'ALSAEOF'
pcm.!default {
  type plug
  slave.pcm "hw:0,0"
}
ctl.!default {
  type hw
  card 0
}
ALSAEOF
  else
    warn "/etc/asound.conf already exists — leaving it alone"
  fi

else
  # --user mode
  USER_BIN_DIR="${HOME}/.local/bin"
  USER_SESSION_DIR="${HOME}/.local/share/chimera/sessions"
  install -d -m 755 "${USER_BIN_DIR}"
  install -d -m 755 "${USER_SESSION_DIR}"

  info "Installing binaries to ${USER_BIN_DIR}..."
  install -m 755 "${BUILD_DIR}/software/apps/chimera-desktop" "${USER_BIN_DIR}/"
  install -m 755 "${BUILD_DIR}/software/apps/chimera-play"    "${USER_BIN_DIR}/"

  info "User install complete."
  info "Binaries: ${USER_BIN_DIR}"
  info "Sessions: ${USER_SESSION_DIR}"
fi

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
echo ""
info "=== Project Chimera is ready on $MODEL ==="
echo ""
echo "Run:"
echo "  chimera-desktop --touch --alsa --scale 3          (headless / kmsdrm)"
echo "  chimera-play --seq --dummy                         (CLI test)"
echo ""
echo "Logs (if using systemd):"
echo "  journalctl -u ${SERVICE_NAME} -f"
echo ""
echo "Troubleshooting:"
echo "  export SDL_VIDEODRIVER=kmsdrm   # direct DRM (no X11)"
echo "  export SDL_VIDEODRIVER=directfb # if kmsdrm fails"
echo "  aplay -l                        # verify ALSA device"
echo ""
