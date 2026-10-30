#!/bin/bash
# Run the graphical engine on an Intel Mac: Mesa Zink on MoltenVK (experimental, branch supcom-mac-intel).
# Usage: run-intel.sh <engine dir> <write dir (absolute)> <start script>
# The engine dir needs: spring, base/, fonts/, cmdcolors.txt, ctrlpanel.txt, and mesa -> a prefix built with
# make-mesa-zink-only.sh plus 0001-zink-allow-no-null-descriptor.patch, with libvulkan.1.dylib linked into its lib/.
set -euo pipefail
ENGINE=$1; DATA=$2; SCRIPT=$3
export ZINK_ALLOW_NO_NULL_DESCRIPTOR=1
export VK_DRIVER_FILES="${VK_DRIVER_FILES:-$(brew --prefix)/etc/vulkan/icd.d/MoltenVK_icd.json}"
cd "$ENGINE"
exec ./spring --isolation --write-dir "$DATA" "$SCRIPT"
