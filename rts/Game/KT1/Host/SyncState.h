/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the synced state of native systems, in the sync checksum (SYNC-RULES S1.6, S3).
//
// A system registers each column of its per-unit synced state at game setup:
//   KT1::SyncState::Add("Census", "weight", index, weight);                  a Core/Common column
//   KT1::SyncState::Add("Census", "teams", [this](StateHash& h) { ... });    anything else: id, fields, fixed order
// At every step's end each column is hashed in (system, column) order and fed to the engine's sync checksum, so two
// clients whose columns differ report a desync in that frame. ForEachHash() names the column that differs (S3).

#ifndef KT1_HOST_SYNC_STATE_H
#define KT1_HOST_SYNC_STATE_H

#include <cstdint>
#include <functional>
#include <string>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/SimHost/StateHash.h"

namespace KT1 {

class SyncState {
public:
	using Writer = std::function<void(Core::SimHost::StateHash&)>;   // writes the column: id, then its fields, ascending id

	static void Add(const char* system, const char* column, Writer writer);
	// A per-unit column (Core/Common): id and value of every unit the index holds, ascending id. Both must outlive
	// the game.
	template<class T> static void Add(const char* system, const char* column, Core::DenseIndex& index, const Core::Column<T>& data) {
		Add(system, column, [&index, &data](Core::SimHost::StateHash& h) {
			for (const int32_t id : index.AscendingIds()) {
				h.Add(id);
				h.Add(data[index.SlotOf(id)]);
			}
		});
	}
	static void Close();   // the step's end (Phases): nothing is fed when nothing is registered
	// The last closed step's hash of every column, as "system/column".
	static void ForEachHash(const std::function<void(const std::string&, uint32_t)>& fn);
	static void Reset();   // game shutdown
};

} // namespace KT1

#endif
