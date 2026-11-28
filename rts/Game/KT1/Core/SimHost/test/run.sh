#!/bin/sh
# Engine-free tests of Core/SimHost, no engine build. As Rules/Tiers/test/run.sh: cross-compiles in the engine's build
# image and runs the .exe on Windows (WSL interop). With a host g++: g++ -std=c++17 simhost_test.cpp && ./a.out
set -e
here="$(cd "$(dirname "$0")" && pwd)"
img=$(podman images -q | head -1)
podman run --rm -v "$here/..":/t "$img" sh -c "cd /t/test && x86_64-w64-mingw32-g++-posix -std=c++17 -O1 -Wall -Wextra -static -o /t/test/simhost_test.exe simhost_test.cpp"
out=$(mktemp -d /mnt/c/Users/Public/kt1simhost.XXXX 2>/dev/null || mktemp -d)
rc=0
mv "$here/simhost_test.exe" "$out/" && { "$out/simhost_test.exe" || rc=$?; }
rm -rf "$out"; exit $rc
