#!/usr/bin/env bash
# Checks the web build's generated packet functions (tools/web/gen_web_packets.py) byte for byte
# against the desktop library's own Send functions. See README.md. Usage: tools/web/packet-test/run.sh
# Environment: PACKET_TEST_OUT (scratch folder, default $TMPDIR/mu-packet-test), NUGET_PACKAGES (the
# NuGet cache, default client-src/.nuget like the CMake build), OPENMU_PACKETS_VERSION (default 0.9.11,
# as in src/CMakeLists.txt and ClientLibrary), PYTHON (default python), EMXX (default em++),
# NODE (default node).
set -euo pipefail

# Under Git Bash, Windows-style paths, so dotnet, python and node (Windows programs) take them as they are.
abspath() {
    (cd "$1" && { pwd -W 2>/dev/null || pwd; })
}

TOOL_DIR="$(abspath "$(dirname "${BASH_SOURCE[0]}")")"
CLIENT_SRC="$(abspath "$TOOL_DIR/../../..")"
OUT="${PACKET_TEST_OUT:-${TMPDIR:-/tmp}/mu-packet-test}"
mkdir -p "$OUT"
OUT="$(abspath "$OUT")"
export NUGET_PACKAGES="${NUGET_PACKAGES:-$CLIENT_SRC/.nuget}"
VERSION="${OPENMU_PACKETS_VERSION:-0.9.11}"
XML_DIR="$NUGET_PACKAGES/munique.openmu.network.packets/$VERSION/contentFiles/any/net10.0"
PYTHON="${PYTHON:-python}"
EMXX="${EMXX:-em++}"
NODE="${NODE:-node}"

echo "== web packet functions (gen_web_packets.py)"
"$PYTHON" -I "$CLIENT_SRC/tools/web/gen_web_packets.py" --xml-dir "$XML_DIR" \
    --bindings-dir "$CLIENT_SRC/src/source/Dotnet" --out-dir "$OUT/generated"

echo "== vectors from the desktop library's Send functions"
mkdir -p "$OUT/vectorgen"
cp "$TOOL_DIR/PacketVectorGen.csproj" "$OUT/vectorgen/PacketVectorGen.csproj"
dotnet run --project "$OUT/vectorgen/PacketVectorGen.csproj" -c Release \
    -p:ToolDir="$TOOL_DIR" -p:ClientLibraryDir="$CLIENT_SRC/ClientLibrary" -p:OpenMuPacketsVersion="$VERSION" \
    -- "$XML_DIR" "$OUT/vectors.txt" "$OUT/generated/packet_calls.generated.inc"

echo "== C++ test (Emscripten, with AddressSanitizer: the builders write into buffers)"
"$EMXX" -std=c++20 -O2 -Wall -Wextra -fsanitize=address -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 \
    -I "$CLIENT_SRC/src/source" -I "$OUT/generated" \
    "$TOOL_DIR/packet_test.cpp" "$OUT/generated/WebPacketFunctions.generated.cpp" \
    -o "$OUT/packet_test.js"
"$NODE" "$OUT/packet_test.js" "$OUT/vectors.txt"
