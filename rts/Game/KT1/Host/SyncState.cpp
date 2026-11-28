/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: native synced state in the sync checksum. See SyncState.h.

#include "SyncState.h"

#include "Game/KT1/Core/SimHost/Ordered.h"
#ifdef SYNCCHECK
#include "System/Sync/SyncChecker.h"
#endif

namespace KT1 {
namespace {

struct Column {
	SyncState::Writer writer;
	uint32_t hash = 0;
};

Core::SimHost::Ordered<Column> columns;

} // namespace

void SyncState::Add(const char* system, const char* column, Writer writer) {
	columns.Add(0, std::string(system) + "/" + column, Column{std::move(writer), 0});
}

void SyncState::Close() {
	for (auto& item : columns) {
		Core::SimHost::StateHash h;
		item.value.writer(h);
		item.value.hash = h.Get();
#ifdef SYNCCHECK
		CSyncChecker::Sync(item.value.hash);
#endif
	}
}

void SyncState::ForEachHash(const std::function<void(const std::string&, uint32_t)>& fn) {
	for (const auto& item : columns) fn(item.name, item.value.hash);
}

void SyncState::Reset() { columns.Clear(); }

} // namespace KT1
