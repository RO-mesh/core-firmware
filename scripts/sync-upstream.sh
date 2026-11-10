#!/usr/bin/env bash
# ====================================================================
# RO-mesh Core Firmware: Upstream Synchronization Playbook
# Syncs pristine lib/MeshCore from upstream meshcore-dev/MeshCore
# ====================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

UPSTREAM_URL="https://github.com/meshcore-dev/MeshCore.git"
UPSTREAM_BRANCH="main"

echo "=== [RO-mesh] Starting Upstream Synchronization ==="
echo "Project Root: ${PROJECT_ROOT}"

cd "${PROJECT_ROOT}"

# 1. Verify git submodule status
if [ ! -d "lib/MeshCore/.git" ] && [ ! -f "lib/MeshCore/.git" ]; then
    echo "[-] Initializing submodule lib/MeshCore..."
    git submodule update --init --recursive lib/MeshCore
fi

# 2. Fetch and merge latest upstream commits
echo "[+] Fetching upstream commits from ${UPSTREAM_URL} (${UPSTREAM_BRANCH})..."
cd lib/MeshCore
git remote set-url origin "${UPSTREAM_URL}" 2>/dev/null || git remote add origin "${UPSTREAM_URL}"
git fetch origin "${UPSTREAM_BRANCH}"

PREV_REV=$(git rev-parse HEAD)
git checkout -q origin/"${UPSTREAM_BRANCH}"
NEW_REV=$(git rev-parse HEAD)

echo "[+] Upstream submodule updated:"
echo "    Previous: ${PREV_REV}"
echo "    Current:  ${NEW_REV}"

cd "${PROJECT_ROOT}"

# 3. Compile and execute native unit test verification suite
echo "[+] Running RO-mesh native regression test suite..."
mkdir -p build
g++ -std=c++17 -w \
  -I test \
  -I src \
  -I src/ro_mesh \
  -I lib/MeshCore/src \
  test/test_all.cpp \
  test/mock_region_map.cpp \
  src/ro_mesh/regions/AutoRegionsRO.cpp \
  src/ro_mesh/hal/AdaptiveTier.cpp \
  src/ro_mesh/hal/PaGainLimiter.cpp \
  src/ro_mesh/filter/QuarantineEngine.cpp \
  src/ro_mesh/filter/BatteryGate.cpp \
  src/ro_mesh/filter/PacketFilter.cpp \
  src/ro_mesh/switcher/PresetCutover.cpp \
  src/ro_mesh/switcher/EmergencyProfileSwitcher.cpp \
  src/ro_mesh/advert/DeterministicAdverts.cpp \
  src/ro_mesh/companion/T1000eSos.cpp \
  src/ro_mesh/companion/ChannelCli.cpp \
  src/ro_mesh/companion/TelemetryBeacon.cpp \
  src/ro_mesh/companion/MessageStore.cpp \
  -o build/test_runner

./build/test_runner

echo "=== [RO-mesh] Upstream synchronization and verification complete! (100% Green) ==="

