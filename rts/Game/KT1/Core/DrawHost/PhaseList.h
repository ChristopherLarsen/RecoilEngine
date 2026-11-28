/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw host (Core, no engine include): the draw phases and the ordered list of the systems in them.
// Systems are called by (order, name), so the result does not depend on the order they registered in.

#ifndef KT1_DRAWHOST_PHASELIST_H
#define KT1_DRAWHOST_PHASELIST_H

#include <algorithm>
#include <string>
#include <vector>

namespace KT1 {

// In frame order. Most stand beside a Lua draw call-in; WorldAlpha is the engine's alpha pass, after its particles;
// PreDraw follows every Update (the last moment to set the frame's camera); FrameEnd is after everything drawn, before
// the buffers are swapped.
enum class DrawPhase : int {
	Update = 0,
	PreDraw,
	Genesis,
	WorldPreUnit,
	World,
	WorldAlpha,
	ScreenEffects,
	Screen,
	MiniMap,
	FrameEnd,
	Count
};

constexpr unsigned PhaseBit(DrawPhase p) { return 1u << static_cast<int>(p); }

inline const char* PhaseName(DrawPhase p) {
	static const char* const names[] = {"Update", "PreDraw", "Genesis", "WorldPreUnit", "World", "WorldAlpha", "ScreenEffects",
	                                    "Screen", "MiniMap", "FrameEnd"};
	static_assert(sizeof(names) / sizeof(names[0]) == static_cast<int>(DrawPhase::Count), "one name per phase");
	const int i = static_cast<int>(p);
	return (i >= 0 && i < static_cast<int>(DrawPhase::Count)) ? names[i] : "?";
}

template<typename System>
class PhaseList {
public:
	struct Entry {
		System* system = nullptr;
		std::string name;
		int order = 0;
		unsigned phases = 0;
		bool enabled = true;
	};

	// false when the system or its name is in the list already
	bool Add(System* system, const std::string& name, int order, unsigned phases) {
		if (system == nullptr || Find(name) != nullptr) return false;
		for (const Entry& e : entries) {
			if (e.system == system) return false;
		}
		const Entry entry{system, name, order, phases, true};
		const auto before = [](const Entry& a, const Entry& b) { return (a.order != b.order) ? a.order < b.order : a.name < b.name; };
		entries.insert(std::upper_bound(entries.begin(), entries.end(), entry, before), entry);
		return true;
	}

	bool SetEnabled(const std::string& name, bool on) {
		Entry* e = Find(name);
		if (e == nullptr) return false;
		e->enabled = on;
		return true;
	}

	// an enabled system takes part in the phase
	bool Wants(DrawPhase p) const {
		return std::any_of(entries.begin(), entries.end(), [p](const Entry& e) { return e.enabled && (e.phases & PhaseBit(p)) != 0; });
	}

	bool AnyEnabled() const {
		return std::any_of(entries.begin(), entries.end(), [](const Entry& e) { return e.enabled; });
	}

	// fn(entry) for every enabled system of the phase, in order; fn must not add or remove systems
	template<typename Fn> void ForPhase(DrawPhase p, Fn&& fn) const {
		for (const Entry& e : entries) {
			if (e.enabled && (e.phases & PhaseBit(p)) != 0) fn(e);
		}
	}

	const std::vector<Entry>& Entries() const { return entries; }
	void Clear() { entries.clear(); }

private:
	Entry* Find(const std::string& name) {
		for (Entry& e : entries) {
			if (e.name == name) return &e;
		}
		return nullptr;
	}

	std::vector<Entry> entries;
};

} // namespace KT1

#endif
