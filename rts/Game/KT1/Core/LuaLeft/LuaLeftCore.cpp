/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 LuaLeft core: the adding up (LuaLeftCore.h). No allocation, no engine include.

#include "LuaLeftCore.h"

#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace KT1 { namespace LuaLeft {

namespace {
	constexpr float NS_PER_MS = 1000000;      // DESIGN: unit conversion
	constexpr uint32_t HASH_MIX = 31u;        // DESIGN: any odd multiplier, spreads one name over the states

	// Per-unit call-ins whose name does not start with "Unit" (DESIGN: the unit is among the first arguments).
	// Draw call-ins (DrawUnit, DrawShield) run per drawn frame, not per sim step: not in the count.
	constexpr const char* PER_UNIT[] = {
		"AllowCommand", "CommandFallback", "AllowUnitCreation", "AllowUnitTransfer", "AllowUnitBuildStep",
		"AllowUnitCaptureStep", "AllowUnitTransport", "AllowUnitTransportLoad", "AllowUnitTransportUnload",
		"AllowUnitCloak", "AllowUnitDecloak", "AllowUnitKamikaze", "AllowFeatureBuildStep", "AllowWeaponTargetCheck",
		"AllowWeaponTarget", "AllowBuilderHoldFire", "AllowStockpileBuildStep", "AllowResurrect",
		"MoveCtrlNotify", "StockpileChanged", "TerraformComplete",
	};

	void CopyName(char (&to)[NAME_LEN], const char* from) {
		std::snprintf(to, NAME_LEN, "%s", from);
	}

	bool SameName(const char (&stored)[NAME_LEN], const char* name) {
		return std::strncmp(stored, name, NAME_LEN - 1) == 0;
	}

	float PerFrameMs(int64_t ns, int frames) { return (frames > 0) ? (float(ns) / NS_PER_MS) / frames : 0; }
	float PerFrame(uint64_t n, int frames) { return (frames > 0) ? float(n) / frames : 0; }
}

bool IsPerUnitCallIn(const char* name) {
	constexpr size_t PREFIX = sizeof("Unit") - 1;
	if (std::strncmp(name, "Unit", PREFIX) == 0 && std::isupper(static_cast<unsigned char>(name[PREFIX])))
		return true;
	for (const char* n : PER_UNIT)
		if (std::strcmp(name, n) == 0) return true;
	return false;
}

int Meter::State(const char* name, Side side) {
	for (int i = 0; i < stateCount; ++i)
		if (states[i].side == side && SameName(states[i].name, name)) return i;
	if (stateCount == MAX_STATES) return -1;
	CopyName(states[stateCount].name, name);
	states[stateCount].side = side;
	return stateCount++;
}

int Meter::Row(int state, uint32_t hash, const char* callIn) {
	if (state < 0) return -1;
	uint32_t at = (hash * HASH_MIX + uint32_t(state)) % HASH_SLOTS;
	// the table is half empty at its fullest, so a probe always ends
	for (;; at = (at + 1) % HASH_SLOTS) {
		const int row = index[at] - 1;
		if (row < 0) break;
		const RowSlot& s = rows[row];
		if (s.hash == hash && s.state == state && SameName(s.name, callIn)) return row;
	}
	if (rowCount == MAX_ROWS) return -1;
	RowSlot& s = rows[rowCount];
	CopyName(s.name, callIn);
	s.hash = hash;
	s.state = state;
	s.perUnit = IsPerUnitCallIn(s.name);
	index[at] = int16_t(rowCount + 1);
	return rowCount++;
}

// KT1 hot
void Meter::Push(int row, Side side, bool gc, int64_t nowNs) {
	if (depth == MAX_DEPTH) { ++overflow; return; }
	stack[depth++] = Frame{row, side, gc, nowNs, 0};
}

// KT1 hot
void Meter::Enter(int row, Side side, int64_t nowNs, int step) {
	++acc.calls[side];
	if (row < 0 || depth == MAX_DEPTH) ++acc.lostCalls;
	if (row >= 0 && rows[row].perUnit) {
		++acc.unitCalls;
		if (step != lastUnitStep) { lastUnitStep = step; ++acc.unitCallSteps; }
	}
	Push(row, side, false, nowNs);
}

// KT1 hot
void Meter::EnterGc(Side side, int64_t nowNs) {
	if (depth == MAX_DEPTH) ++acc.lostCalls;
	Push(-1, side, true, nowNs);
}

// KT1 hot
void Meter::Leave(int64_t nowNs) {
	if (overflow > 0) { --overflow; return; }
	if (depth == 0) return;
	const Frame f = stack[--depth];
	const int64_t whole = nowNs - f.startNs;
	const int64_t self = whole - f.childNs;
	if (depth > 0) stack[depth - 1].childNs += whole;
	if (f.gc) { acc.gcNs[f.side] += self; return; }
	acc.selfNs[f.side] += self;
	if (f.row >= 0) { rows[f.row].selfNs += self; ++rows[f.row].calls; }
}

bool Meter::CountsWanted(int state) const { return state >= 0 && !states[state].counted; }

void Meter::SetCounts(int state, int gadgets, int widgets) {
	if (state < 0) return;
	states[state].gadgets = gadgets;
	states[state].widgets = widgets;
	states[state].counted = true;
}

void Meter::SampleAllocs(const void* context, Side side, uint64_t total) {
	AllocSlot* empty = nullptr;
	for (AllocSlot& s : allocSlots) {
		if (s.context == context) {
			// a smaller count means a new context at an old address: it counts from its creation
			acc.allocs[side] += (total >= s.last) ? total - s.last : total;
			s.last = total;
			s.seen = true;
			return;
		}
		if (empty == nullptr && s.context == nullptr) empty = &s;
	}
	if (empty == nullptr) return;
	*empty = AllocSlot{context, total, true};
	acc.allocs[side] += total;                    // created in this window
}

void Meter::StartWindow(int step, int drawFrame) {
	acc = Report{};
	startStep = step;
	startDraw = drawFrame;
	lastUnitStep = -1;
	started = true;
	for (int i = 0; i < rowCount; ++i) { rows[i].selfNs = 0; rows[i].calls = 0; }
	for (int i = 0; i < stateCount; ++i) {
		states[i].recent = states[i].counted;
		states[i].counted = false;
	}
	for (AllocSlot& s : allocSlots) {
		if (!s.seen) s = AllocSlot{};             // its context is gone
		s.seen = false;
	}
}

void Meter::PickTop(Report& r) const {
	std::array<int, MAX_TOP> best{};
	int n = 0;
	for (int i = 0; i < rowCount; ++i) {
		if (rows[i].calls == 0) continue;
		int at = n;
		while (at > 0 && rows[best[at - 1]].selfNs < rows[i].selfNs) --at;
		if (at == MAX_TOP) continue;
		if (n < MAX_TOP) ++n;
		for (int k = n - 1; k > at; --k) best[k] = best[k - 1];
		best[at] = i;
	}
	for (int k = 0; k < n; ++k) {
		const RowSlot& s = rows[best[k]];
		r.top[k] = TopRow{states[s.state].name, s.name, states[s.state].side, s.selfNs, s.calls};
	}
	r.topCount = n;
}

Report Meter::Close(int step, int drawFrame, int periodSteps) {
	Report r = acc;
	r.simSteps = step - startStep;
	r.drawFrames = drawFrame - startDraw;
	r.partial = r.simSteps < periodSteps;
	// a state unheard of for two windows is gone (LuaUI switched off): its gadgets and widgets drop out
	for (int i = 0; i < stateCount; ++i) {
		const StateSlot& s = states[i];
		if (!s.counted && !s.recent) continue;
		if (s.side == SYNCED) r.gadgetsSynced += s.gadgets;
		else r.gadgetsUnsynced += s.gadgets;
		r.widgets += s.widgets;
	}
	PickTop(r);
	StartWindow(step, drawFrame);
	return r;
}

void FormatSummary(const Report& r, char* out, size_t size) {
	const uint64_t allocs = r.allocs[SYNCED] + r.allocs[UNSYNCED];
	std::snprintf(out, size,
		"[LUALEFT] synced_ms=%.3f unsynced_ms=%.3f gc_ms=%.3f allocs=%" PRIu64 " gadgets=%d widgets=%d"
		" gadgets_synced=%d gadgets_unsynced=%d gc_synced_ms=%.3f gc_unsynced_ms=%.3f"
		" allocs_synced=%.0f allocs_unsynced=%.0f calls_synced=%.0f calls_unsynced=%.0f unit_calls=%.1f unit_call_steps=%d"
		" sim_steps=%d draw_frames=%d window=%s lost_calls=%" PRIu64,
		PerFrameMs(r.selfNs[SYNCED], r.simSteps), PerFrameMs(r.selfNs[UNSYNCED], r.drawFrames),
		PerFrameMs(r.gcNs[SYNCED] + r.gcNs[UNSYNCED], r.drawFrames), allocs,
		r.gadgetsSynced + r.gadgetsUnsynced, r.widgets, r.gadgetsSynced, r.gadgetsUnsynced,
		PerFrameMs(r.gcNs[SYNCED], r.simSteps), PerFrameMs(r.gcNs[UNSYNCED], r.drawFrames),
		PerFrame(r.allocs[SYNCED], r.simSteps), PerFrame(r.allocs[UNSYNCED], r.drawFrames),
		PerFrame(r.calls[SYNCED], r.simSteps), PerFrame(r.calls[UNSYNCED], r.drawFrames),
		PerFrame(r.unitCalls, r.simSteps), r.unitCallSteps, r.simSteps, r.drawFrames,
		r.partial ? "partial" : "full", r.lostCalls);
}

void FormatTop(const Report& r, char* out, size_t size) {
	size_t used = size_t(std::snprintf(out, size, "[LUALEFT] top (ms and calls per sim step if S, per drawn frame if U):"));
	for (int i = 0; i < r.topCount && used < size; ++i) {
		const TopRow& t = r.top[i];
		const int frames = (t.side == SYNCED) ? r.simSteps : r.drawFrames;
		used += size_t(std::snprintf(out + used, size - used, " %c:%s:%s=%.3f(%.1f)",
			(t.side == SYNCED) ? 'S' : 'U', t.state, t.callIn, PerFrameMs(t.selfNs, frames), PerFrame(t.calls, frames)));
	}
}

}} // namespace KT1::LuaLeft
