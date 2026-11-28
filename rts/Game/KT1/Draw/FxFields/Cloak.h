/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxFields: the shells of cloaked units (replaces LuaUI/fx/shield_fx.lua's cloak rules and
// LuaRules/Gadgets/fx_cloak.lua): a haze with screen refraction for own units, an outline for allies, a flash on
// decloak for whoever sees it. Cloak state from Snapshot<Core::Fx::UnitFxSample>. Engine-free.

#ifndef KT1_FIELD_CLOAK_H
#define KT1_FIELD_CLOAK_H

#include <cstdint>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/FxEvents/FxTypes.h"
#include "Game/KT1/Draw/FxCommon/FxShapes.h"

namespace KT1::FxFields {

// one instance, 12 floats (shader locations 1 .. 3)
struct CloakInstance {
	float posRad[4];             // centre, horizontal radius
	float shape[4];              // vertical radius, mode (1 haze, 2 outline, 3 flash), amount 0 .. 1, seed
	float color[4];              // team colour, unused
};
static_assert(sizeof(CloakInstance) == 48);

enum class CloakMode : uint8_t { None, Haze, Outline, Flash };
enum class Relation : uint8_t { Own, Ally, Enemy };
enum class CloakChange : uint8_t { None, Cloak, Decloak };

struct CloakConfig {             // "cloak"
	int max = 512;               // DESIGN: shells drawn
	int rings = 12;              // DESIGN
	int sectors = 24;            // DESIGN
	float scan_s = 0.1f;         // DESIGN: the cloak state is read this often
	float decloak_life_s = 0.35f;    // DESIGN
	float cloak_life_s = 0.25f;      // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("max", max);
		s.Field("rings", rings);
		s.Field("sectors", sectors);
		s.Field("scan_s", scan_s);
		s.Field("decloak_life_s", decloak_life_s);
		s.Field("cloak_life_s", cloak_life_s);
	}
};

struct Cloak {
	bool cloaked = false;
	bool flash = true;
	float since = -100.0f, decloakAt = -100.0f;
};

struct CloakStats { uint32_t drawn = 0, haze = 0, outline = 0, flashes = 0, skipped = 0, refract = 0; };

struct CloakState {
	Core::DenseIndex units;                    // units that may cloak, by unit id
	Core::Column<Cloak> cloaks;
	KT1::FxDraw::InstanceList<CloakInstance> instances;
	float nextScan = 0.0f;
	CloakStats stats;
};

struct CloakDrawing { CloakMode mode = CloakMode::None; float amount = 0.0f; };

void InitCloaks(CloakState& s, const CloakConfig& c, int32_t maxUnits);
void TrackCloak(CloakState& s, int32_t unit, bool cloaked, float now, const CloakConfig& c);
void ForgetCloak(CloakState& s, int32_t unit);
CloakChange SetCloaked(Cloak& cloak, float now, bool cloaked);
Relation RelationOf(int32_t team, int32_t allyTeam, int32_t myTeam, int32_t myAllyTeam, bool fullView);
CloakDrawing DrawingOf(const CloakConfig& c, const Cloak& cloak, float now, Relation relation, bool seen);

} // namespace KT1::FxFields

#endif
