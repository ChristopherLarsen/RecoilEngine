/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: display effects on their way from the sim to the draw side (SIM-DRAW-PARALLEL 13.4, 13.8).
// No engine include.
//
// The sim Emits during a step; Publish(step), at the step's end, moves the step's events to a second list, stamped with
// the step; the draw side Replays that list, in step order and emit order, and forgets it. Events are plain values taken
// at emit time: the unit may be gone at replay. Nothing here is synced: no sim code may read the queue back.
//
// With no draw side to replay them (or one that stalls) the published list holds at most DISPLAY_PUBLISHED_MAX events;
// the oldest go first and are counted. No allocation once the lists have grown.

#ifndef KT1_DISPLAY_QUEUE_H
#define KT1_DISPLAY_QUEUE_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace KT1::Core::SimHost {

enum class DisplayKind : uint8_t {
	Sound,      // id = sound id; a = position; value = volume
	Effect,     // id = explosion generator id; a = position; b = direction; value = damage; size = radius; unit = owner
	Log,        // text
	DebugLine,  // a, b = ends; size = width; value = lifetime in seconds
	Ping        // a = position; text = label; unit = player
};

constexpr size_t DISPLAY_TEXT = 120;             // DESIGN: longer text is cut, so an event stays a plain value
constexpr size_t DISPLAY_PUBLISHED_MAX = 4096;   // DESIGN: events held for a draw side that has not replayed them

struct DisplayEvent {
	DisplayKind kind = DisplayKind::Log;
	int32_t step = -1;      // the sim step it was emitted in; set by Publish
	int32_t id = -1;
	int32_t unit = -1;
	float a[3] = {0.0f, 0.0f, 0.0f};
	float b[3] = {0.0f, 0.0f, 0.0f};
	float value = 0.0f;
	float size = 0.0f;
	char text[DISPLAY_TEXT] = {0};

	void SetText(const char* s) {
		size_t n = 0;
		if (s != nullptr)
			while (n < DISPLAY_TEXT - 1 && s[n] != 0) ++n;
		if (n > 0) std::memcpy(text, s, n);
		text[n] = 0;
	}
};

class DisplayQueue {
public:
	// KT1 hot
	void Emit(const DisplayEvent& e) { writing.push_back(e); }

	// Copies rather than swaps, so both lists keep their size and a warm queue does not allocate.
	void Publish(int32_t step) {
		for (DisplayEvent& e : writing) e.step = step;
		published.insert(published.end(), writing.begin(), writing.end());
		writing.clear();
		if (published.size() <= DISPLAY_PUBLISHED_MAX) return;
		const size_t over = published.size() - DISPLAY_PUBLISHED_MAX;
		published.erase(published.begin(), published.begin() + static_cast<std::ptrdiff_t>(over));
		dropped += over;
	}

	// `sink` may not Emit or Publish: replay is draw-side work.
	template<class F> void Replay(F&& sink) {
		for (const DisplayEvent& e : published) sink(e);
		published.clear();
	}

	size_t Pending() const { return published.size(); }
	uint64_t Dropped() const { return dropped; }

	void Clear() {
		writing.clear();
		published.clear();
		dropped = 0;
	}

private:
	std::vector<DisplayEvent> writing;     // the step in progress: the sim's
	std::vector<DisplayEvent> published;   // finished steps: the draw side's
	uint64_t dropped = 0;
};

} // namespace KT1::Core::SimHost

#endif
