#!/bin/sh
# Engine-free tests for KT1 Core/Common (F02), no engine build. WSL has no host compiler, so this cross-compiles in the
# engine's build image (the engine's own compiler and FP flags: C++23, no contraction, float constants, rounding-math) and runs the .exe on Windows (WSL interop).
# With a host g++: g++ -std=c++23 -ffp-contract=off common_test.cpp && ./a.out
set -e
here="$(cd "$(dirname "$0")" && pwd)"
img=$(podman images -q | head -1)
podman run --rm -v "$here/..":/c "$img" sh -c "cd /c/test && x86_64-w64-mingw32-g++-posix -std=c++23 -O2 -Wall -Wextra -Werror -ffp-contract=off -fsingle-precision-constant -frounding-math -static -o /c/test/common_test.exe common_test.cpp"
out=$(mktemp -d /mnt/c/Users/Public/kt1common.XXXX 2>/dev/null || mktemp -d)
mv "$here/common_test.exe" "$out/" && "$out/common_test.exe"; rc=$?
rm -rf "$out"; exit $rc
