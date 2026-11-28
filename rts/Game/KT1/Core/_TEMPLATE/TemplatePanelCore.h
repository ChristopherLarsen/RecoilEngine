/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: the logic of an interface panel. No engine include: tested engine-free
// (Test/_TEMPLATE/TemplatePanelTest.cpp). Copy to Core/<System>/<System>Core.h.
//
// The example: the selection as one row per unit def (def id, how many), rebuilt only when the selection changes, never
// per drawn frame (LUA-FREE-PLAN 2.6).

#ifndef KT1_TEMPLATE_PANEL_CORE_H
#define KT1_TEMPLATE_PANEL_CORE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace KT1::Core::Template {

struct SelectionRow {
	int32_t defId = 0;
	int32_t count = 0;
	bool operator==(const SelectionRow&) const = default;
};

class PanelModel {
public:
	// every allocation happens here
	void Init(int32_t maxIds, int32_t maxRows) {
		defIds.reserve(static_cast<size_t>(std::max<int32_t>(0, maxIds)));
		rows.reserve(static_cast<size_t>(std::max<int32_t>(0, maxRows)));
		limit = static_cast<size_t>(std::max<int32_t>(0, maxRows));
		Forget();
	}

	// the next Stale answers true
	void Forget() { seen = false; }

	// true when the selection's change counter moved since the last Rebuild
	bool Stale(uint32_t changes) const { return !seen || changes != lastChanges; }

	// the caller fills this with the selected units' def ids, any order, then calls Rebuild
	std::vector<int32_t>& DefIds() { return defIds; }

	// rows in ascending def id; defs past the row limit are left out and counted
	void Rebuild(uint32_t changes) {
		std::sort(defIds.begin(), defIds.end());
		rows.clear();
		leftOut = 0;
		for (const int32_t def : defIds) {
			if (!rows.empty() && rows.back().defId == def) ++rows.back().count;
			else if (rows.size() < limit) rows.push_back({def, 1});
			else ++leftOut;
		}
		lastChanges = changes;
		seen = true;
	}

	const std::vector<SelectionRow>& Rows() const { return rows; }
	int32_t LeftOut() const { return leftOut; }   // units whose def got no row

private:
	std::vector<int32_t> defIds;
	std::vector<SelectionRow> rows;
	size_t limit = 0;
	int32_t leftOut = 0;
	uint32_t lastChanges = 0;
	bool seen = false;
};

} // namespace KT1::Core::Template

#endif
