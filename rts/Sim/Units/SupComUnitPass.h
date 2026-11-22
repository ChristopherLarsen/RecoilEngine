/* SupCom: one engine walk over the units per sim frame that runs the absorbed per-unit systems in turn
 * (docs/engine/UNIT-PASS-DESIGN.md, owner-approved 2026-11-20).
 * Order: ascending unit id, every client the same (no sort: ids are walked in order). Each step has a filter, kept per
 * unit def as a bit mask, so it never visits units it has no work for; an optional rate runs it where id % rate == frame % rate (int_coverage F.pm slicing).
 * Each step is behind mod option unitpass_<name> ("1" on; default off until its Lua-parity proof lands).
 * Results reach Lua as one batch per frame: Spring.SupComUnitPassEvents() -> frame, ids, kinds, a, b (flat arrays).
 * With no step on, Update returns at once (the zero-cost check of the skeleton). */
#ifndef SUPCOM_UNIT_PASS_H
#define SUPCOM_UNIT_PASS_H

#include <cstdint>

struct lua_State;

class CUnit;

namespace SupComUnitPass {
	struct Facts { int frame; };
	using StepFn = void (*)(CUnit* u, const Facts& facts);
	using FilterFn = bool (*)(const CUnit* u);

	/// registers a step at static-init time; name is the mod option suffix
	int Register(const char* name, FilterFn filter, StepFn fn, int rate);
	/// a step's event for Lua (kind: the step's own code)
	void Emit(int unitID, int kind, float a, float b);
	/// whether step `name` is on (its mod option); steps run by Lua (intel) ask this
	bool IsOn(const char* name);

	/// one-line hook in CUnitHandler::Update, after the move types
	void Update();
	/// one-line hook in LuaSyncedRead
	void PushSyncedRead(lua_State* L);

	// steps (registered explicitly at the first Update, so no translation unit can be dropped by the linker)
	void RegisterMoveStep();
	void RegisterIntelStep();
	void PushIntel(lua_State* L);   // SupComIntRules/Defs/Group/UnitFlags, SupComIntState (SupComUnitPassIntel.cpp)
	int SetMoveSpeed(lua_State* L);
}

#endif
