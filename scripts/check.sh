#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"$ROOT_DIR/scripts/bootstrap-linux.sh"
"$ROOT_DIR/scripts/configure.sh" linux-release
"$ROOT_DIR/scripts/build.sh" build-release

if [[ -f "$ROOT_DIR/build/linux-release/CTestTestfile.cmake" ]]; then
  ctest --test-dir "$ROOT_DIR/build/linux-release" --output-on-failure
else
  echo "No tests configured (ENGINE_SIM_BUILD_TESTS=OFF or missing test target)."
fi

echo "Check pipeline passed."
