/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage core: the records of the damage pipeline (LUA-FREE H01, COMBAT §4.1; step S21). No engine include.
//
// Replaces the data of LuaRules/Gadgets/cmb_damage.lua, cmb_impact_patterns.lua and death_blast.lua. Units: unit,
// projectile, weapon def and unit def ids as the engine gives them; -1 is "none"; times are sim steps; positions and
// radii are elmos unless a name says map units (Mu).

#ifndef KT1_CORE_DAMAGE_TYPES_H
#define KT1_CORE_DAMAGE_TYPES_H

#include <cstdint>

namespace KT1::Core::Damage {

constexpr int32_t NONE = -1;

// customParams.impact_pattern (REQ CMB-022). "burst_fragments" and "single" are look only: None.
enum class Pattern : uint8_t { None, Incendiary, TwoStage, SplitOnWater };

// which rules changed a hit: the check column rule_hit (bits, both layers of the chain together)
enum RuleBit : uint8_t {
	RULE_NONE         = 0,
	RULE_UNDAMAGEABLE = 1 << 0,   // REQ CMB-008 R1
	RULE_TRUCE        = 1 << 1,   // the setup's opening truce
	RULE_SPLASH_DROP  = 1 << 2,   // the engine's area hit of an area-hit bomb is dropped
	RULE_FRIENDLY     = 1 << 3,   // REQ CMB-006 R2
	RULE_INITIAL      = 1 << 4,   // REQ CMB-006 R5: the initial damage was added
	RULE_NO_DAMAGE    = 1 << 5,   // REQ CMB-006 R4: nothing left
	RULE_KILL         = 1 << 6,   // REQ CMB-008 R2: the killing hit
	RULE_PATTERN      = 1 << 7    // REQ CMB-022: a split child's share, or the first of two stages
};

// one UnitPreDamaged call
struct Hit {
	int32_t unit = NONE;
	int32_t unitDef = NONE;
	int32_t unitTeam = NONE;
	int32_t attacker = NONE;
	int32_t attackerTeam = NONE;   // NONE when the attacker is gone
	int32_t weaponDef = NONE;      // negative: an engine damage source
	int32_t projectile = NONE;
	float damage = 0;              // the raw damage the chain hands in
	bool paralyzer = false;
};

// what the call answers; the check column hit_result
struct HitResult {
	float damage = 0;
	float impulse = 1;             // factor on the engine's impulse; 0: no push (REQ CMB-006 R7)
	bool paralyzer = false;
	uint8_t rules = RULE_NONE;     // RuleBit: the check column rule_hit
};

// a pending damage-over-time pulse of cmb_damage (REQ CMB-006 R6); kept in schedule order
struct DotPulse {
	int32_t atStep = 0;
	int32_t unit = NONE;           // single target
	int32_t weaponDef = NONE;
	int32_t attacker = NONE;
	bool area = false;             // true: everyone in the weapon's radius at pos
	float pos[3] = {};
	float amount = 0;
};

// a pending area pulse of cmb_impact_patterns (incendiary pulses, the second of two stages); kept in schedule order
struct PatternPulse {
	int32_t atStep = 0;
	int32_t weaponDef = NONE;
	int32_t attacker = NONE;
	float pos[3] = {};
	float radius = 0;
	float amount = 0;
};

// the killing hit on a unit (GG.Cmb.lastKill; REQ CMB-008 R3: the wreck rules read the overkill)
struct KillRecord {
	bool set = false;
	float amount = 0;
	float before = 0;              // health before the hit
	float max = 0;
};

} // namespace KT1::Core::Damage

#endif
