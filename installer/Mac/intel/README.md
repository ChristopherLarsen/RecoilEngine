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

## Graphical engine (runs, experimental)
Mesa EGL + Zink on MoltenVK (KosmicKrisp is Apple Silicon only). `make-mesa-zink-only.sh` is Leon Seemann's
`make-mesa.sh` (branch `macos-port-2026.07.04`) with the KosmicKrisp half removed. Open question: Zink needs
`robustness2.nullDescriptor`, which MoltenVK lacks.

Result 2026-10-30: `engine-legacy` builds. Mesa 26.3.0-rc1 EGL + Zink builds with `make-mesa-zink-only.sh` (link
`/usr/local/lib/libvulkan.1.dylib` into the Mesa prefix's `lib/`, or Zink cannot load the loader through `@rpath`).
Run with `VK_DRIVER_FILES=/usr/local/etc/vulkan/icd.d/MoltenVK_icd.json`. MoltenVK 1.4.2 on the Radeon Pro 5700 XT
exposes VK_EXT/KHR_robustness2 but `nullDescriptor = false`, and Zink stops at init:
`Zink requires the nullDescriptor feature of KHR/EXT robustness2.` (engine exits: could not create Mesa EGL/Zink GL context).

Result 2026-10-30, with `0001-zink-allow-no-null-descriptor.patch` and `ZINK_ALLOW_NO_NULL_DESCRIPTOR=1`
(`run-intel.sh`): GL `4.6 (Compatibility Profile) Mesa 26.3.0-rc1`, renderer `zink Vulkan 1.4(AMD Radeon Pro 5700 XT
(MOLTENVK))`. A 1800-frame skirmish against KardashevAI on T0 Tiny, fullscreen 5120x2880, played to the end. Owner's
look: "pretty good"; delayed texture loading, a subtle flicker, short stutters. Known: 2x
`vkCreateGraphicsPipelines failed (VK_ERROR_INITIALIZATION_FAILED)`; Mesa warns of no depth_clip_enable, logicOp,
custom_border_color on this device; `[DISPLAY] window off the displays` repeats (Retina size); exit code 21 at shutdown
after the game ended. Engine content: copy `cont/fonts`, `cont/cmdcolors.txt`, `cont/ctrlpanel.txt` next to `spring`.

### Stutter measurements (2026-10-30, 1920x1080 window, T0 Tiny, 1800 frames, temp frame-time gadget)
| Setting | Avg FPS | Hitches > 50 ms (not counting autotest screenshots) | Window "rescues" |
|---|---|---|---|
| MacHighDPI=1 (Retina, 3840x2160 px), cold shader cache | 37.8 | 7 | 58 |
| MacHighDPI=1, warm cache | 37.0 | 5 | 58 |
| MacHighDPI=0 (1x) | 59.4 (vsync) | 4 | 0 |
- The 1-3.4 s stalls in both Retina runs are the m0 autotest's own `/screenshot` PNG saves, not gameplay.
- The Mesa shader disk cache does not remove the hitches (cold and warm runs match); the late ones are probably first
  draws of new units/effects (MSL conversion and Metal pipeline compile in MoltenVK).
- `gfx_display.lua` (game) compares the window in Retina pixels (`GetWindowGeometry`) with the display in points
  (`GetScreenGeometry`), sees a window 28 % on screen, and re-applies window geometry every second. Not Intel specific:
  any Mac with MacHighDPI=1. The engine knows the ratio (`globalRendering->pixelRatio`) but does not expose it to Lua.
