#!/usr/bin/env bash
set -euo pipefail

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICE_NAME="chimera-desktop"

if [ "$(id -u)" -ne 0 ]; then
  echo "Please run as root (sudo $0)" >&2
  exit 1
fi

install -D -m 644 "${PROJ_ROOT}/systemd/${SERVICE_NAME}.service" \
  "/etc/systemd/system/${SERVICE_NAME}.service"

BIN_DIR="/opt/chimera/bin"
mkdir -p "${BIN_DIR}"
mkdir -p /opt/chimera/sessions

if [ -f "${PROJ_ROOT}/build/software/apps/chimera-desktop" ]; then
  install -m 755 "${PROJ_ROOT}/build/software/apps/chimera-desktop" "${BIN_DIR}/"
elif [ -f "${PROJ_ROOT}/build-jetson/software/apps/chimera-desktop" ]; then
  install -m 755 "${PROJ_ROOT}/build-jetson/software/apps/chimera-desktop" "${BIN_DIR}/"
else
  echo "Binary not found. Build first:"
  echo "  ./scripts/run_chimera.sh jetson"
  exit 1
fi

systemctl daemon-reload
systemctl enable "${SERVICE_NAME}"
systemctl restart "${SERVICE_NAME}"

echo "Installed and started ${SERVICE_NAME}"
systemctl status "${SERVICE_NAME}"
