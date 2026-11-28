/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 LuaLeft core (LUA-FREE-WORKPACKAGES F08): how much Lua is left. Engine-free: the adapter (KT1LuaLeft.cpp) feeds it
// timestamps, step numbers and counters; it adds them up over a window and writes the [LUALEFT] lines.
//
// Time is SELF time: a call-in entered while another runs (SendToUnsynced, a Spring call that raises UnitDestroyed, a
// garbage collection) is taken out of the outer one, so synced + unsynced + gc never counts a nanosecond twice and the
// top rows add up to the side totals.

#ifndef KT1_LUALEFT_CORE_H
#define KT1_LUALEFT_CORE_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace KT1 { namespace LuaLeft {

enum Side : int { UNSYNCED = 0, SYNCED = 1, SIDES = 2 };

inline constexpr int MAX_STATES = 16;    // Lua handles told apart by (name, side); the game has under ten
inline constexpr int MAX_ROWS   = 512;   // (state, call-in) pairs; more are counted in the side totals only
inline constexpr int MAX_DEPTH  = 128;   // nested call-ins; deeper ones stay in their caller's time
inline constexpr int MAX_TOP    = 10;    // the card: "the top 10 call-ins"
inline constexpr int MAX_ALLOCS = 64;    // Lua contexts whose allocation counters are followed
inline constexpr int NAME_LEN   = 64;    // LuaHashString holds 63 characters

struct Config {
	double reportPeriodS = 10.0;   // DESIGN: perf_sprof's window (ten seconds of game time)
};

struct TopRow {
	const char* state = "";
	const char* callIn = "";
	Side side = UNSYNCED;
	int64_t selfNs = 0;
	uint64_t calls = 0;
};

struct Report {
	int simSteps = 0, drawFrames = 0;
	bool partial = false;                        // closed before the period was over (game end)
	std::array<int64_t, SIDES> selfNs{};         // call-in self time
	std::array<int64_t, SIDES> gcNs{};           // Lua garbage collection
	std::array<uint64_t, SIDES> allocs{};        // Lua allocations, from the contexts' own counters
	std::array<uint64_t, SIDES> calls{};         // calls into Lua (each costs the meter two clock reads)
	int gadgetsSynced = 0, gadgetsUnsynced = 0, widgets = 0;
	uint64_t unitCalls = 0;                      // LF-IDLE: engine -> Lua calls about one unit
	int unitCallSteps = 0;                       // LF-IDLE: sim steps with at least one such call
	uint64_t lostCalls = 0;                      // calls with no row (table full) or too deep; 0 in a sound run
	std::array<TopRow, MAX_TOP> top{};
	int topCount = 0;
};

// A call-in the engine makes about one unit (a unit event, a per-unit Allow*, a unit script function).
bool IsPerUnitCallIn(const char* name);

// The two report lines, without the log call. Rates: synced per sim step, unsynced per drawn frame.
void FormatSummary(const Report& r, char* out, size_t size);
void FormatTop(const Report& r, char* out, size_t size);

class Meter {
public:
	// ----- identity -----
	int State(const char* name, Side side);                     // find or add; -1 when full
	int Row(int state, uint32_t hash, const char* callIn);      // find or add; -1 when full or state < 0

	// ----- calls -----
	void Enter(int row, Side side, int64_t nowNs, int step);    // KT1 hot; row -1: counted in the side total only
	void EnterGc(Side side, int64_t nowNs);                     // KT1 hot
	void Leave(int64_t nowNs);                                  // KT1 hot; closes the latest Enter or EnterGc
	bool Idle() const { return depth == 0 && overflow == 0; }

	// ----- sampled values -----
	bool CountsWanted(int state) const;                         // not read yet in this window
	void SetCounts(int state, int gadgets, int widgets);
	void SampleAllocs(const void* context, Side side, uint64_t total);   // a context's cumulative allocation count

	// ----- window -----
	void StartWindow(int step, int drawFrame);                  // drops what was added up so far
	void Stop() { started = false; }                            // switched off: the next window starts fresh
	// No window yet, or the step counter went back (a new game in the same process).
	bool NeedsStart(int step) const { return !started || step < startStep; }
	bool Due(int step, int periodSteps) const { return started && (step - startStep) >= periodSteps; }
	int StepsInWindow(int step) const { return started ? step - startStep : 0; }
	// Adds the window up, then starts the next one. Call only when Idle().
	Report Close(int step, int drawFrame, int periodSteps);

private:
	struct StateSlot {
		char name[NAME_LEN] = {};
		Side side = UNSYNCED;
		int gadgets = 0, widgets = 0;
		bool counted = false;
	};
	struct RowSlot {
		char name[NAME_LEN] = {};
		uint32_t hash = 0;
		int state = -1;
		bool perUnit = false;
		int64_t selfNs = 0;
		uint64_t calls = 0;
	};
	struct Frame {
		int row = -1;
		Side side = UNSYNCED;
		bool gc = false;
		int64_t startNs = 0, childNs = 0;
	};
	struct AllocSlot {
		const void* context = nullptr;
		uint64_t last = 0;
		bool seen = false;
	};

	void Push(int row, Side side, bool gc, int64_t nowNs);
	void PickTop(Report& r) const;

	static constexpr int HASH_SLOTS = MAX_ROWS * 2;

	std::array<StateSlot, MAX_STATES> states{};
	std::array<RowSlot, MAX_ROWS> rows{};
	std::array<int16_t, HASH_SLOTS> index{};     // row + 1; 0 empty
	std::array<Frame, MAX_DEPTH> stack{};
	std::array<AllocSlot, MAX_ALLOCS> allocSlots{};
	int stateCount = 0, rowCount = 0, depth = 0, overflow = 0;

	bool started = false;
	int startStep = 0, startDraw = 0, lastUnitStep = -1;
	Report acc{};                                // the window's running totals
};

}} // namespace KT1::LuaLeft

#endif
