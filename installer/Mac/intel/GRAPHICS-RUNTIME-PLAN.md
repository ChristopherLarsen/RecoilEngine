# Mac graphics runtime and zero-copy present: plan

Status: planned, not started (owner 2026-10-30: "we will come back to this and implement this combination later").
Branch for the work: `supcom-mac-zerocopy`, off `supcom-mac-intel`. Measurements and today's path: [README.md](README.md).

## Why

On an Intel Mac the engine renders through Mesa Zink on MoltenVK, then the EGL bridge reads every frame back to the CPU
(`glReadPixels`), copies it into a Metal buffer and blits it into the window. At Retina that is about 33 MB a frame
through system memory: about 62 FPS uncapped at Retina against about 190 at 1x on a Radeon Pro 5700 XT. Packaging today is
several loose pieces (Mesa libraries, the Vulkan loader, MoltenVK, an ICD file and two environment variables).

## Part A: one graphics runtime folder

Today: `libEGL` + `libGLESv2` + `libgallium` (Mesa OpenGL and Zink together) from Mesa; `libvulkan` (loader) and
`libMoltenVK` from Homebrew, found through `MoltenVK_icd.json` and `VK_DRIVER_FILES`; `ZINK_ALLOW_NO_NULL_DESCRIPTOR=1`.

| Step | What | Estimate |
|---|---|---|
| A1 | One build script: Mesa with our patches (`000*.patch`) plus MoltenVK, installed as `mesa/` next to the engine (the bridge looks there first) with a version file | about half a day |
| A2 | Drop the Vulkan loader: Zink loads `libMoltenVK.dylib` directly (it exports the Vulkan entry points); no ICD file, no `VK_DRIVER_FILES`. Small Mesa patch (Zink looks for `libvulkan`). Make the nullDescriptor allowance automatic on MoltenVK instead of an environment variable | about a day |
| A3 | Optional: MoltenVK linked statically into `libgallium` (one library). Little gain over A2; MoltenVK updates would need a Mesa rebuild | not planned |

Result: Intel runtime = `libEGL` + `libgallium` + `libMoltenVK`, one versioned folder, no environment variables. Apple
Silicon keeps the same layout with KosmicKrisp inside Mesa and no MoltenVK.

## Part B: zero-copy present (frame stays on the GPU)

MoltenVK here has `VK_EXT_metal_objects` and `VK_EXT_external_memory_metal`: the Metal texture behind a Vulkan image can be
exported. Mesa has no Metal handle type and no macOS window surface for EGL (pbuffer and surfaceless only), so the work is
in Mesa and the bridge. MoltenVK stays stock.

| Share | Milestone | Work |
|---|---|---|
| ~10 % | Feasibility spike | Zink: a new handle type in `resource_get_handle` that calls `vkExportMetalObjectsEXT` on the image. Show the engine can get an `MTLTexture` for one GL texture. Risk: MoltenVK may require images created with `VkExportMetalObjectCreateInfoEXT`; then patch Zink's image creation for exportable textures, or switch to importing an IOSurface into Vulkan (+1-2 days) |
| ~25 % | First frame on screen | An EGL/GL entry point for that export. The bridge renders the final frame into an exportable texture, flips it on the GPU, and Metal blits it into the drawable. CPU fence wait for sync at first |
| ~60 % | Correct and robust | Resize, Retina switch, fullscreen, load screen, hardware cursor, screenshots (still need readback), vsync on and off, fallback to today's readback path on any failure |
| 100 % | Fast and finished | GPU-to-GPU sync (Metal shared event from a Vulkan timeline semaphore) instead of the CPU wait; measurements; patch files here; check Apple Silicon is untouched |

Estimate: 3 to 5 days of focused work, most of the risk in the first 10 %. Expected gain: most of the gap between Retina
(~62 FPS uncapped) and the GPU's own limit; a number only after the first frame is on screen.

## Shipping

- Mesa is already shipped with the Mac build (Apple's OpenGL stops at 4.1). Intel Macs already need patched Mesa (0001-0004)
  plus MoltenVK; this plan adds Mesa patches only. Windows is unaffected (no Mesa).
- Licences: Mesa MIT; MoltenVK and the Vulkan loader Apache 2.0; bundling is allowed with their notices. A new third-party
  component still needs the owner's approval with name, source, version and licence recorded (as O-32).
- To shrink what is custom: offer the general MoltenVK fixes upstream to Mesa (sampler name 0003, no reorder on MoltenVK
  0002, no invalidate without readback 0004). Keep the Intel build a separate, best-effort package; Apple Silicon stays on
  unpatched Mesa.

## Order when this resumes

A1, then B (spike first, report how long the 10 % took), then A2 together with the B Mesa patches.
