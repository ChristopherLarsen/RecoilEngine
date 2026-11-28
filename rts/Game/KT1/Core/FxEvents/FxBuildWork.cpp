/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the build work rules. Stub (LUA-FREE H01): step S30 writes the bodies.

#include "Game/KT1/Core/FxEvents/FxBuildWork.h"

namespace KT1::Core::Fx {

BuildTarget TargetOf(const BuilderOrder&, int32_t) { return {}; }
bool InReach(const BuildWorkConfig&, float, float, float) { return false; }
int32_t NanoPiece(const BuildWorkConfig&, std::span<const std::string_view>) { return -1; }
void LoopsDue(const BuildWorkConfig&, std::vector<LoopState>&, std::span<const WorkingBuilder>, float, std::vector<int32_t>&) {}
void LoopNext(const BuildWorkConfig&, std::vector<LoopState>&, int32_t, float, float) {}

} // namespace KT1::Core::Fx
