/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: the per-step event batches (game-build docs/engine/rules/simhost.md). No engine include.
//
// The sim raises events; the log keeps one list per event type, each in raise order, and stamps every event with a
// sequence number so the order across types is kept too. Each reader (one per system) takes what is new for it:
//   batch.ForEachById(type, ...)   one type, ascending id, equal ids in raise order
//   batch.ForEachRaised(...)       every subscribed type, exactly as the engine raised them
// An event raised while a batch is being read lands in the reader's next batch. Compact() drops what every reader has
// taken; call it only when no batch is alive (the end of the step). No allocation once the lists have grown.

#ifndef KT1_SIM_EVENT_LOG_H
#define KT1_SIM_EVENT_LOG_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace KT1::SimHost {

// What each type fills in Event (unlisted fields are -1 or 0).
enum class EventType : uint8_t {
	UnitCreated,         // other = builder
	UnitFinished,
	UnitFromFactory,     // other = factory; flags = user orders
	UnitDestroyed,       // other = attacker; arg = weapon def
	UnitGiven,           // arg = old team; arg2 = new team
	UnitTaken,           // arg = old team; arg2 = new team
	UnitDamaged,         // other = attacker; arg = weapon def; arg2 = projectile; value = damage; flags = paralyzer
	UnitLoaded,          // other = transport
	UnitUnloaded,        // other = transport
	UnitCmdDone,         // arg = command id; arg2 = command tag
	UnitIdle,
	ProjectileCreated,   // id = projectile; def = weapon def (-1: none); other = owner unit
	ProjectileDestroyed, // as ProjectileCreated
	Explosion,           // id = weapon def; other = owner unit; arg = hit unit; arg2 = hit feature; pos
	FeatureCreated,      // id = feature; def = feature def
	FeatureDestroyed,    // as FeatureCreated
	Count
};

constexpr size_t EVENT_TYPES = static_cast<size_t>(EventType::Count);
using EventMask = uint32_t;

constexpr EventMask MaskOf(EventType t) { return EventMask(1) << static_cast<unsigned>(t); }
constexpr EventMask ALL_EVENTS = (EventMask(1) << EVENT_TYPES) - 1;

struct Event {
	uint64_t seq = 0;               // raise order across every type; set by the log
	int32_t id = -1;                // the subject: unit, projectile, feature or weapon def
	int32_t def = -1;               // the subject's def id
	int32_t team = -1;              // the subject's team
	int32_t other = -1;
	int32_t arg = -1;
	int32_t arg2 = -1;
	float value = 0.0f;
	float pos[3] = {0.0f, 0.0f, 0.0f};   // the subject's position when the event was raised
	EventType type = EventType::Count;
	uint8_t flags = 0;
};

class EventLog;

// What one reader got from one Take: a window per type. Valid until the log is compacted or cleared.
class EventBatch {
public:
	EventBatch() = default;

	size_t Size(EventType t) const { return end[Index(t)] - begin[Index(t)]; }
	bool Empty() const;
	// The i-th event of a type in raise order. By value: a handler may raise events while it reads.
	Event At(EventType t, size_t i) const;

	// One type in ascending id, equal ids in raise order. `scratch` is the caller's, reused every step.
	template<class F> void ForEachById(EventType t, std::vector<uint32_t>& scratch, F&& fn) const;
	// Every type of the batch, in the order the engine raised them.
	template<class F> void ForEachRaised(F&& fn) const;

private:
	friend class EventLog;
	static size_t Index(EventType t) { return static_cast<size_t>(t); }

	const EventLog* log = nullptr;
	std::array<size_t, EVENT_TYPES> begin{};
	std::array<size_t, EVENT_TYPES> end{};
};

class EventLog {
public:
	// A new reader sees events raised from now on, of the types in its mask.
	int AddReader(EventMask mask) {
		Reader r;
		r.mask = mask;
		for (size_t t = 0; t < EVENT_TYPES; ++t) r.next[t] = lists[t].size();
		readers.push_back(r);
		wanted |= mask;
		return static_cast<int>(readers.size()) - 1;
	}

	EventMask Wanted() const { return wanted; }

	// KT1 hot
	void Push(Event e) {
		const size_t t = static_cast<size_t>(e.type);
		if (t >= EVENT_TYPES || (wanted & MaskOf(e.type)) == 0) return;
		e.seq = nextSeq++;
		lists[t].push_back(e);
	}

	EventBatch Take(int reader) {
		EventBatch b;
		b.log = this;
		Reader& r = readers[reader];
		for (size_t t = 0; t < EVENT_TYPES; ++t) {
			if ((r.mask & (EventMask(1) << t)) == 0) continue;
			b.begin[t] = r.next[t];
			b.end[t] = lists[t].size();
			r.next[t] = b.end[t];
		}
		return b;
	}

	void Compact() {
		for (size_t t = 0; t < EVENT_TYPES; ++t) {
			size_t taken = lists[t].size();
			for (const Reader& r : readers)
				if ((r.mask & (EventMask(1) << t)) != 0) taken = std::min(taken, r.next[t]);
			if (taken == 0) continue;
			lists[t].erase(lists[t].begin(), lists[t].begin() + static_cast<std::ptrdiff_t>(taken));
			for (Reader& r : readers)
				if ((r.mask & (EventMask(1) << t)) != 0) r.next[t] -= taken;
		}
	}

	void Clear() {
		for (auto& l : lists) l.clear();
		readers.clear();
		wanted = 0;
		nextSeq = 0;
	}

	size_t Held(EventType t) const { return lists[static_cast<size_t>(t)].size(); }

private:
	friend class EventBatch;
	struct Reader {
		EventMask mask = 0;
		std::array<size_t, EVENT_TYPES> next{};
	};

	std::array<std::vector<Event>, EVENT_TYPES> lists;
	std::vector<Reader> readers;
	EventMask wanted = 0;
	uint64_t nextSeq = 0;
};

inline bool EventBatch::Empty() const {
	for (size_t t = 0; t < EVENT_TYPES; ++t)
		if (end[t] != begin[t]) return false;
	return true;
}

inline Event EventBatch::At(EventType t, size_t i) const { return log->lists[Index(t)][begin[Index(t)] + i]; }

// KT1 hot
template<class F> void EventBatch::ForEachById(EventType t, std::vector<uint32_t>& scratch, F&& fn) const {
	const size_t n = Size(t);
	scratch.clear();
	for (size_t i = 0; i < n; ++i) scratch.push_back(static_cast<uint32_t>(i));
	// std::sort with the raise index as the tie-break: stable without std::stable_sort's buffer
	std::sort(scratch.begin(), scratch.end(), [&](uint32_t a, uint32_t b) {
		const int32_t ia = At(t, a).id, ib = At(t, b).id;
		return (ia != ib) ? (ia < ib) : (a < b);
	});
	for (const uint32_t i : scratch) fn(At(t, i));
}

// KT1 hot
template<class F> void EventBatch::ForEachRaised(F&& fn) const {
	std::array<size_t, EVENT_TYPES> at = begin;
	for (;;) {
		size_t pick = EVENT_TYPES;
		uint64_t best = 0;
		for (size_t t = 0; t < EVENT_TYPES; ++t) {
			if (at[t] == end[t]) continue;
			const uint64_t s = log->lists[t][at[t]].seq;
			if (pick == EVENT_TYPES || s < best) { pick = t; best = s; }
		}
		if (pick == EVENT_TYPES) return;
		const Event e = log->lists[pick][at[pick]++];
		fn(e);
	}
}

} // namespace KT1::SimHost

#endif
