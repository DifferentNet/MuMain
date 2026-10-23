#!/usr/bin/env bash
# Checks MuPacketCrypto (src/source/Core/Platform/web) byte for byte against OpenMU's network library.
# See README.md. Usage: tools/web/crypto-test/run.sh [OpenMU DLL folder ...]
#   Without arguments it checks against MUnique.OpenMU.Network 0.9.10 (the NuGet package the desktop
#   ClientLibrary uses) and, when it exists, ../server/addons/ref (the DLLs of the game server's image).
# Environment: CRYPTO_TEST_OUT (scratch folder, default $TMPDIR/mu-crypto-test), EMXX (default em++),
# NODE (default node).
set -euo pipefail

# Under Git Bash, Windows-style paths, so dotnet and node (Windows programs) take them as they are.
abspath() {
    (cd "$1" && { pwd -W 2>/dev/null || pwd; })
}

TOOL_DIR="$(abspath "$(dirname "${BASH_SOURCE[0]}")")"
CLIENT_SRC="$(abspath "$TOOL_DIR/../../..")"
OUT="${CRYPTO_TEST_OUT:-${TMPDIR:-/tmp}/mu-crypto-test}"
mkdir -p "$OUT"
OUT="$(abspath "$OUT")"
EMXX="${EMXX:-em++}"
NODE="${NODE:-node}"

# Builds the generator in its own scratch folder (one per OpenMU version) and writes vectors-<name>.txt.
generate() {
    local name="$1"
    shift
    local project="$OUT/vectorgen-$name"
    mkdir -p "$project"
    cp "$TOOL_DIR/VectorGen.csproj" "$project/VectorGen.csproj"
    echo "== vectors from OpenMU ($name)"
    dotnet run --project "$project/VectorGen.csproj" -c Release -p:ToolDir="$TOOL_DIR" "$@" -- "$OUT/vectors-$name.txt"
    VECTORS+=("$OUT/vectors-$name.txt")
}

VECTORS=()
if [ "$#" -eq 0 ]; then
    generate nuget-0.9.10
    SERVER_REF="$CLIENT_SRC/../server/addons/ref"
    if [ -f "$SERVER_REF/MUnique.OpenMU.Network.dll" ]; then
        generate server-ref -p:OpenMuRef="$(abspath "$SERVER_REF")"
    fi
fi
index=0
for ref in "$@"; do
    index=$((index + 1))
    generate "ref-$index" -p:OpenMuRef="$(abspath "$ref")"
done

echo "== C++ test (Emscripten)"
"$EMXX" -std=c++20 -O2 -Wall -Wextra -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 \
    -I "$CLIENT_SRC/src/source" \
    "$TOOL_DIR/crypto_test.cpp" "$CLIENT_SRC/src/source/Core/Platform/web/MuPacketCrypto.cpp" \
    -o "$OUT/crypto_test.js"
"$NODE" "$OUT/crypto_test.js" "${VECTORS[@]}"
