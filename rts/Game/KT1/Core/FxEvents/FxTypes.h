/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS family: the small types the sim side (Rules/FxEvents) and the draw side (Draw/Fx*) share. No engine include.

#ifndef KT1_FX_TYPES_H
#define KT1_FX_TYPES_H

#include <cstdint>

namespace KT1::Core::Fx {

struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };

// who saw a point when it was raised: bit n set = ally team n had it in line of sight
using SeenMask = uint32_t;
constexpr int32_t SEEN_ALLY_TEAMS = 32;   // DESIGN: ally teams past this have no bit (only a full view sees for them)
constexpr bool Sees(SeenMask mask, int32_t allyTeam, bool fullView) {
	return fullView || (allyTeam >= 0 && allyTeam < SEEN_ALLY_TEAMS && ((mask >> allyTeam) & 1u) != 0u);
}

// A look in the roster (Draw/FxCommon/FxRoster.h gives the draw side its data).
using RosterKey = uint16_t;
constexpr RosterKey NO_KEY = 0xFFFF;

// an engine explosion generator id (the effect library's definitions); -1: unknown
constexpr int32_t NO_EFFECT = -1;

// the budget's priority codes, highest first (gamedata/fx_budget.lua B.code)
enum class Priority : uint8_t { Hit = 1, Death = 2, Shield = 3, Trail = 4, State = 5, Ambient = 6 };
constexpr int32_t PRIORITIES = 6;

// the numbers of the next four are the Lua's wire values (1-based indexes): keep them
enum class BlastClass : uint8_t { None = 0, SmallArms = 1, TankShell = 2, Artillery = 3, BuildingDeath = 4, ExperimentalDeath = 5 };
enum class BlastFamily : uint8_t { None = 0, Cannon = 1, Beam = 2, Missile = 3, Flak = 4, Torpedo = 5, Plasma = 6, Flame = 7 };
enum class TeleportKind : uint8_t { Start = 1, Jump = 2, Cancel = 3 };
enum class DeathMedium : uint8_t { None = 0, Land = 1, Water = 2, Air = 3 };   // gfx_criteria_fx.lua MEDIUM

// where a blast or effect is (gamedata/fx_blast.lua B.Medium)
enum class Medium : uint8_t { Ground, Water, Underwater, Air };
// the faction looks (gamedata/fx_factions.lua F.order)
enum class Faction : uint8_t { Human, Arachnid, Grey, Hadean, Count };
// weapon effect tier (customParams fx_scale)
enum class Tier : uint8_t { S, M, L, Xl, Count };
// unit death and damage-state layer and class (fx_death_sets.lua)
enum class DeathLayer : uint8_t { Land, Water, Sub, Air };
enum class DeathKind : uint8_t { Mobile, Air, Structure, Command, Sea, Sub };
// movement effects (gamedata/fx_move.lua)
enum class MoveKind : uint8_t { Dust, Spray, Wake, Bubbles, Silt, Downwash, Count };
enum class SizeBucket : uint8_t { S, M, L, X, Count };
// the script trees of the third-tier bombs, the cluster parent and the interceptor (fx_script_relay.lua TREE_OF_CLASS)
enum class ScriptTree : uint8_t { None, NukDq, NukDp, NukDz, ClsA, StmDh };

} // namespace KT1::Core::Fx

#endif
