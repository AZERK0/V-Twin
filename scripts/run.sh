#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_PATH="${1:-$ROOT_DIR/build/linux-release/engine-sim-app}"

if [[ ! -x "$APP_PATH" ]]; then
  echo "Binary not found: $APP_PATH"
  echo "Run ./scripts/build.sh first."
  exit 1
fi

export ENGINE_SIM_DATA_ROOT="$ROOT_DIR"
exec "$APP_PATH"
