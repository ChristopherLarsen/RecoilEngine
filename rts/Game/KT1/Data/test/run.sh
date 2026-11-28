#!/bin/sh
# Engine-free tests for KT1 Data, no engine build. WSL has no host compiler, so this cross-compiles in the engine's build
# image and runs the .exe on Windows (WSL interop). simdjson (rts/lib/simdjson, a submodule) is compiled from its single
# file once and its object kept in ~/.cache. With a host g++:
#   g++ -std=c++20 -I<simdjson/singleheader> data_test.cpp ../*.cpp <simdjson/singleheader>/simdjson.cpp && ./a.out
set -e
here="$(cd "$(dirname "$0")" && pwd)"
data="$(dirname "$here")"
json="$data/../../../lib/simdjson/singleheader"
[ -f "$json/simdjson.cpp" ] || { echo "no $json: git submodule update --init rts/lib/simdjson" >&2; exit 2; }
img=$(podman images -q | head -1)
cache="$HOME/.cache/kt1data"
mkdir -p "$cache"
key=$(sha1sum "$json/simdjson.cpp" | cut -c1-12)
cxx="x86_64-w64-mingw32-g++-posix -std=c++20 -O1 -Wall -Wextra -I/j"
run() { podman run --rm -v "$data":/d -v "$json":/j -v "$cache":/c "$img" sh -c "$1"; }
[ -f "$cache/simdjson-$key.o" ] || run "$cxx -w -c -o /c/simdjson-$key.o /j/simdjson.cpp"
run "$cxx -static -o /c/data_test.exe /d/test/data_test.cpp /d/Config.cpp /d/Schema.cpp /d/Reload.cpp /c/simdjson-$key.o"
out=$(mktemp -d /mnt/c/Users/Public/kt1data.XXXX 2>/dev/null || mktemp -d)
mv "$cache/data_test.exe" "$out/"
rc=0
"$out/data_test.exe" || rc=$?
rm -rf "$out"
exit $rc
