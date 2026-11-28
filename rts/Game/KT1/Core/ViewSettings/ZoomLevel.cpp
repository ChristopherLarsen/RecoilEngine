/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of ZoomLevel.h (LUA-FREE H01). FRAME step D07 writes the bodies.

#include "ZoomLevel.h"

namespace KT1::Core::View {

int RawLevel(const ZoomBounds&, float) { return ZOOM_NONE; }
int LevelFor(const ZoomBounds&, float, int, bool) { return ZOOM_NONE; }

int DetailIndex(const DetailConfig&, int) { return 1; }
float SweepFactor(const DetailSweep&, int) { return 1; }
float DetailFactor(const DetailConfig&, const std::string&, int) { return 1; }

bool CompileRows(const ViewSettingsConfig&, ViewTable&) { return false; }
float Factor(const ViewGate&, int) { return 1; }
float Start(const ViewGate&, int) { return 0; }
float End(const ViewGate&, int) { return NO_END; }
bool HoldsAtLevel(const ViewGate&, int) { return false; }
bool HoldsAtDistance(const ViewGate&, float, int, bool) { return false; }
bool HoldsAtDepth(const ViewGate&, float, int) { return false; }
uint64_t Evaluate(const ViewTable&, int, float, int, uint64_t, bool) { return 0; }

} // namespace KT1::Core::View
