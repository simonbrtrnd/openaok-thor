#!/usr/bin/env bash
# Fetches the libraries the Android build compiles from source -- SDL 2.32.10
# and FreeType 2.13.3 -- into android/deps (not kept in git), and checks each
# download against its known SHA-256 before unpacking it.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p deps
cd deps

verify() {
    local actual
    if command -v sha256sum >/dev/null; then
        actual=$(sha256sum "$1" | cut -d' ' -f1)
    else
        actual=$(shasum -a 256 "$1" | cut -d' ' -f1)
    fi
    if [ "$actual" != "$2" ]; then
        rm -f "$1"
        echo "$1: SHA-256 is $actual, expected $2" >&2
        exit 1
    fi
}

if [ ! -d SDL2-2.32.10 ]; then
    curl -fsSL --retry 5 --retry-all-errors -o SDL2-2.32.10.tar.gz \
        https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
    verify SDL2-2.32.10.tar.gz 5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
    tar xzf SDL2-2.32.10.tar.gz
fi
if [ ! -d freetype-VER-2-13-3 ]; then
    curl -fsSL --retry 5 --retry-all-errors -o freetype-2.13.3.tar.gz \
        https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz
    verify freetype-2.13.3.tar.gz bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103
    tar xzf freetype-2.13.3.tar.gz
fi
echo "deps ready in $(pwd)"
