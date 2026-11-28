/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: ViewSettings' state of the zoom level and the gates, and the log of everything it writes (LUA-FREE H01,
// FRAME §4-5; replaces the state of api_zoom_level and gfx_draw_zoom_gates). No engine include.
// The camera is the input: nothing here is simulation state. The unit, wreck and prop mask state is ViewCullState.h,
// the water, reflection and ground look state ViewSceneState.h.
//
// Check column "settings_written": every engine setting and draw mask written, name and value, in order (SettingsLog).

#ifndef KT1_CORE_VIEW_SETTINGS_STATE_H
#define KT1_CORE_VIEW_SETTINGS_STATE_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ZoomLevel.h"

namespace KT1::Core::View {

constexpr int TARGET_TEXT = 48;   // DESIGN: longest setting or mask name logged, with its end

struct SettingWrite {
	char target[TARGET_TEXT] = {0};
	int32_t id = -1;              // the unit or feature of a draw mask; -1: an engine setting
	float value = 0.0f;
};

class SettingsLog {
public:
	void Record(std::string_view target, int32_t id, float value);   // a longer name is cut
	const std::vector<SettingWrite>& Writes() const { return writes; }
	void Clear() { writes.clear(); }

private:
	std::vector<SettingWrite> writes;
};

// what the adapter reads from the engine once per drawn frame
struct ViewInputs {
	float distance = -1.0f;      // camera-to-focus distance, elmos; -1: this camera mode has none
	float height = 0.0f;         // camera height, elmos (the fallback distance)
	int engineLevel = ZOOM_NONE; // the engine's own level when it has one (SupComZoomLevel), else ZOOM_NONE
	bool wholeMap = false;       // the four map corners project inside the view (asked only past whole_map_from)
	int detail = 1;              // DetailIndex of the engine setting
};

struct ViewState {
	ViewTable table;
	int level = ZOOM_NONE;
	int detail = 1;
	float distance = -1.0f;
	bool wholeMap = false;
	uint64_t mask = 0;           // Evaluate's answer
	uint64_t changed = 0;        // the bits that moved in the last update
	uint64_t disabled = 0;       // rows turned off by their mod option
	uint32_t changes = 0;        // counts level and option changes
	bool first = true;
	SettingsLog log;
};

bool WholeMapCheckNeeded(const ZoomBounds& bounds, float distance, int engineLevel);   // ask the corners only then
// Level, detail and gate mask from the inputs; true when `changes` moved.
bool UpdateView(ViewState& state, const ViewSettingsConfig& config, const ViewInputs& inputs);
// The rows with a target whose bit is in state.changed, in table order: logs what is to be written and appends their
// indices to `out`.
void RowsToWrite(ViewState& state, const ViewSettingsConfig& config, std::vector<int>& out);
int FindGate(const ViewSettingsConfig& config, std::string_view name);   // the first row of that gate; -1: unknown
// the first holding row of the gate that `gate` (a row index from FindGate) belongs to; -1: none holds
int HoldingRow(const ViewState& state, const ViewSettingsConfig& config, int gate);

} // namespace KT1::Core::View

#endif
