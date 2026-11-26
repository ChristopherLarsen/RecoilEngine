#!/bin/sh
# Engine-free tests for KT1 rule Tiers, no engine build. WSL has no host compiler, so this cross-compiles in the engine's
# build image and runs the .exe on Windows (WSL interop). With a host g++, run: g++ -std=c++17 tiers_test.cpp && ./a.out
set -e
here="$(cd "$(dirname "$0")" && pwd)"
img=$(podman images -q | head -1)
podman run --rm -v "$here":/t "$img" sh -c "cd /t && x86_64-w64-mingw32-g++-posix -std=c++17 -O1 -Wall -static -o /t/tiers_test.exe tiers_test.cpp"
out=$(mktemp -d /mnt/c/Users/Public/kt1tiers.XXXX 2>/dev/null || mktemp -d)
mv "$here/tiers_test.exe" "$out/" && "$out/tiers_test.exe"; rc=$?
rm -rf "$out"; exit $rc
