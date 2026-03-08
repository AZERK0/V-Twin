#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

missing=0
for cmd in cmake git pkg-config; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Missing required command: $cmd"
    missing=1
  fi
done

if [[ $missing -ne 0 ]]; then
  exit 1
fi

echo "Syncing submodules..."
git -C "$ROOT_DIR" submodule sync --recursive
git -C "$ROOT_DIR" submodule update --init --recursive

echo "Writing runtime delta.conf files..."

mkdir -p "$ROOT_DIR/build"
cat > "$ROOT_DIR/build/delta.conf" <<'EOF'
../dependencies/submodules/delta-studio/engines/basic
../assets
EOF

for dir in "$ROOT_DIR/build/linux-release" "$ROOT_DIR/build/linux-debug"; do
  mkdir -p "$dir"
  cat > "$dir/delta.conf" <<'EOF'
../../dependencies/submodules/delta-studio/engines/basic
../../assets
EOF
done

echo "Bootstrap done."
