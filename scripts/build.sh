#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_PRESET="${1:-build-release}"

cmake --build --preset "$BUILD_PRESET"

case "$BUILD_PRESET" in
  build-release) RUNTIME_DIR="$ROOT_DIR/build/linux-release" ;;
  build-debug) RUNTIME_DIR="$ROOT_DIR/build/linux-debug" ;;
  *) RUNTIME_DIR="$ROOT_DIR/build/linux-release" ;;
esac

if [[ -d "$RUNTIME_DIR" ]]; then
  cat > "$RUNTIME_DIR/delta.conf" <<'EOF'
../dependencies/submodules/delta-studio/engines/basic
../assets
EOF
fi
