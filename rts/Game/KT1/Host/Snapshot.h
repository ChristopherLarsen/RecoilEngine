/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: Snapshot<T>, the draw-facing per-unit data a sim system publishes once per step (game-build
// docs/engine/LUA-FREE-PLAN.md 2.2 principle 8 and 8.1 contract 1). No engine include: tested engine-free.
//
// It holds the current and the previous step's values and both step numbers, so a draw system can blend between two steps
// or extrapolate from the current one (which of the two is NEW-ENGINE-PLAN's choice).
//
// Publisher (sim side):  Begin(step); Put(id, serial, value) for every unit it owns; Commit().
//                        Put calls may be spread over several calls (a sliced step): they touch only a third buffer, so a
//                        reader keeps seeing the last two committed steps until Commit.
// Reader (draw side):    Current(id), Previous(id), Step(), PrevStep(), Ids().
// Commit is the one moment publisher and reader must not run at once (the sync point of a later threaded sim).
//
// `serial` tells two units apart that had the same id one after the other: Previous returns nothing for a new serial, so a
// reused id never blends from the dead unit's place.

#ifndef KT1_HOST_SNAPSHOT_H
#define KT1_HOST_SNAPSHOT_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace KT1 {

template<typename T>
class Snapshot {
public:
	// ids 0 .. capacity - 1; every allocation happens here
	void Init(int capacity) {
		cap = std::max(capacity, 0);
		for (Buffer& b : buffers) b.Init(cap);
		cur = 0; prev = 1; write = 2;
		open = false;
		commits = 0;
	}

	void Begin(int step) {
		Buffer& b = buffers[write];
		b.Open(step);
		open = true;
	}

	// ignored outside Begin .. Commit and for an id out of range; a second Put of an id replaces the first
	void Put(int id, uint32_t serial, const T& value) {
		if (!open || id < 0 || id >= cap) return;
		buffers[write].Put(id, serial, value);
	}

	void Commit() {
		if (!open) return;
		open = false;
		const int free = prev;
		prev = cur;
		cur = write;
		write = free;
		commits++;
	}

	int Capacity() const { return cap; }
	bool Ready() const { return commits > 0; }
	int Step() const { return buffers[cur].step; }              // meaningful once Ready()
	int PrevStep() const { return buffers[prev].step; }         // equals nothing until the second Commit: see HasPrevious
	bool HasPrevious() const { return commits > 1; }
	const std::vector<int>& Ids() const { return buffers[cur].ids; }   // the current step's ids, in Put order

	// nullptr: the id is not in the current step
	const T* Current(int id) const { return buffers[cur].Has(id) ? &buffers[cur].values[id] : nullptr; }

	// the same unit's value one step earlier; nullptr when it is new (or gone) in the current step
	const T* Previous(int id) const {
		const Buffer& c = buffers[cur];
		const Buffer& p = buffers[prev];
		if (!c.Has(id) || !p.Has(id) || c.serials[id] != p.serials[id]) return nullptr;
		return &p.values[id];
	}

private:
	struct Buffer {
		std::vector<T> values;
		std::vector<uint32_t> serials, stamps;
		std::vector<int> ids;
		uint32_t epoch = 0;     // 0: never opened
		int step = 0;

		void Init(int n) {
			values.assign(n, T{});
			serials.assign(n, 0u);
			stamps.assign(n, 0u);
			ids.clear();
			ids.reserve(n);
			epoch = 0;
			step = 0;
		}
		void Open(int s) {
			if (++epoch == 0) {   // the counter wrapped: old stamps could match again
				std::fill(stamps.begin(), stamps.end(), 0u);
				epoch = 1;
			}
			ids.clear();
			step = s;
		}
		void Put(int id, uint32_t serial, const T& v) {
			if (stamps[id] != epoch) ids.push_back(id);
			stamps[id] = epoch;
			serials[id] = serial;
			values[id] = v;
		}
		bool Has(int id) const {
			return epoch != 0 && id >= 0 && static_cast<size_t>(id) < stamps.size() && stamps[id] == epoch;
		}
	};

	Buffer buffers[3];
	int cap = 0;
	int cur = 0, prev = 1, write = 2;
	int commits = 0;
	bool open = false;
};

// Where draw code finds a snapshot without naming the sim system that publishes it: one snapshot per value type T.
// A value type lives in a small header of its own that both sides may include.
class SnapshotBoard {
public:
	static SnapshotBoard& Get() {
		static SnapshotBoard board;
		return board;
	}

	// the publisher keeps ownership; it calls Clear (or the host does at game end) before the snapshot dies
	template<typename T> void Publish(const Snapshot<T>& s) {
		for (auto& e : entries) {
			if (e.first == Key<T>()) { e.second = &s; return; }
		}
		entries.emplace_back(Key<T>(), &s);
	}

	template<typename T> const Snapshot<T>* Find() const {
		for (const auto& e : entries) {
			if (e.first == Key<T>()) return static_cast<const Snapshot<T>*>(e.second);
		}
		return nullptr;
	}

	void Clear() { entries.clear(); }

private:
	template<typename T> static const void* Key() {
		static const char key = 0;
		return &key;
	}

	std::vector<std::pair<const void*, const void*>> entries;
};

} // namespace KT1

#endif
