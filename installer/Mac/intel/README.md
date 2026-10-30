# Intel Mac (x86_64) experiment

Experimental and separate from the Apple Silicon port. Lives only on branch `supcom-mac-intel`; drop the branch to
discard it. Nothing here changes the pin or the Apple Silicon build.

Machine: iMac20,2 (i9-10910, Radeon Pro 5700 XT), macOS 26.7.1, Homebrew in /usr/local, Homebrew GCC 16.2.

## Headless engine (works)
1. `git -C tools/pr-downloader apply ../../installer/Mac/intel/pr-downloader-macos-x64.patch`
   (pr-downloader is an upstream submodule with no Intel-Mac platform entry; the patch adds `MacOS_x64`).
2. `cmake -B build-macos-gcc -G Ninja -DCMAKE_C_COMPILER=/usr/local/bin/gcc-16 -DCMAKE_CXX_COMPILER=/usr/local/bin/g++-16 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVERSION_OVERRIDE="<pin sync string>"`
3. `cmake --build build-macos-gcc --target engine-headless`

Streflop configures in x86 SSE mode, like the Windows build. Homebrew's old gcc 13.2 bottle fails against the
macOS 26 SDK (`stdio.h`), so use gcc 16.

## Graphical engine (in progress)
Mesa EGL + Zink on MoltenVK (KosmicKrisp is Apple Silicon only). `make-mesa-zink-only.sh` is Leon Seemann's
`make-mesa.sh` (branch `macos-port-2026.07.04`) with the KosmicKrisp half removed. Open question: Zink needs
`robustness2.nullDescriptor`, which MoltenVK lacks.
