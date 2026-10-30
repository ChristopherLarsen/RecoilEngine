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

### Load screen pop-in and frame rate (2026-10-30)
Apply all `000*.patch` to Mesa 26.3.0-rc1 before building (`patch -p1` in the Mesa source).
- Pop-in was not slow loading: for ~5 s the load screen's glass panels drew as opaque white (frame dumps of what the
  bridge presented). Cause: Zink's command reordering on MoltenVK; uploads reached draws late, so translucent fills
  lost their colour. `0002` keeps the command stream in order on MoltenVK (as `ZINK_DEBUG=noreorder`): white frames
  21/37 -> 0/37, no FPS cost (48.1 vs 47.7). `0002` also binds a zeroed dummy buffer where Zink would bind a null
  vertex/uniform/storage buffer (no nullDescriptor).
- The 2 failed pipelines: Mesa's tgsi_to_nir names its texture "sampler", which MoltenVK's MSL output cannot use
  (`texture2d<float> sampler` hides Metal's `sampler` type). `0003` renames it; failures 2 -> 0.
- Retina 36 -> ~48 FPS: the bridge's frame copy on worker threads (fork branch `supcom-mac-present`, cherry-picked here).

### Play profiling (2026-10-30, 15 min against KardashevAI, Retina 1920x1080 window)
- Main thread: about 58 % in the bridge's frame copy. Timed per step: read 3.8 ms, copy into SDL's lock buffer 9.7 ms
  (SDL allocates a fresh Metal buffer per frame), SDL_UpdateTexture 22 ms. Fork `supcom-mac-present` now presents through
  a CAMetalLayer with three persistent upload buffers (`MacMetalPresent`, default on for x86_64): 47 -> 57 FPS.
- Commander deaths: no hitch when reproduced (local test gadget). The big hitches were the game's step-down ladder
  (`gfx_ladder.lua`): water_basic 2.7-3.1 s and shadows back on at game over 0.2-0.5 s (shader rebuilds). Game branch
  `mac-retina-display`: restore at shutdown instead of game over. The ladder and effects guard use a 60 FPS threshold, which a
  vsync-capped 60 Hz display averages just under, so they always step down there (owner decision pending).
- The "every 22 s" hitch was the profiler (sample in 20 s slices); absent in unsampled runs.
- HardwareCursor on by default on macOS (fork `supcom-mac-present`): the software cursor lagged by the presented frame.

### After the vsync change (2026-10-30)
- `0004`: Zink on MoltenVK invalidated every buffer map, and MoltenVK turns an invalidate into a full GPU wait; skipped
  when nothing can be read back (write-only map of a never-written, discarded or subdata range). It removes the waits
  from `VBO::New`; the frame rate did not change measurably (52.3 -> 52.8 FPS in the commander-death test).
- The slow seconds after a commander dies are GPU fill rate at Retina: 6 s at 7-20 FPS at 2x, a 2 s dip to 31 FPS and no
  hitches at 1x (MacHighDPI=0). A Retina-sized effect pass on a Radeon Pro 5700 XT through MoltenVK.
