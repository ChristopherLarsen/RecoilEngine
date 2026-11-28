/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of ViewSettingsState.h (LUA-FREE H01). FRAME step D07 writes the bodies.

#include "ViewSettingsState.h"

namespace KT1::Core::View {

void SettingsLog::Record(std::string_view, int32_t, float) {}

bool WholeMapCheckNeeded(const ZoomBounds&, float, int) { return false; }
bool UpdateView(ViewState&, const ViewSettingsConfig&, const ViewInputs&) { return false; }
void RowsToWrite(ViewState&, const ViewSettingsConfig&, std::vector<int>&) {}
int FindGate(const ViewSettingsConfig&, std::string_view) { return -1; }
int HoldingRow(const ViewState&, const ViewSettingsConfig&, int) { return -1; }

} // namespace KT1::Core::View
