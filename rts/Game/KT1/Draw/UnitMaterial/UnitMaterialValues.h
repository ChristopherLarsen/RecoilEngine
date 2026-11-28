/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the per-unit values of system "unitmaterial", where each goes in the engine's per-object uniform block, its
// config and state (LUA-FREE H01, UNITDRAW §4.5; replaces gfx_select_bright and the value side of gfx_unit_shader; step
// D06 writes the bodies). Engine-free: standard and Core headers only. Config data/kt1/unitmaterial.json.
//
// The block (Rendering/Models/ModelsMemStorageDefs.h ModelUniformData, 128 bytes, binding 1). The engine keeps teamID,
// maxHealth, health, drawPos (w: heading), speed and drawFlag itself, so team tint and damage need no write; userDefined
// slots 12..15 are the engine model shader's tint (KT1 hook "model tint"): an rgb multiplier on the final colour while
// slot 15 is over one half. The selection brighten is that multiplier. No field is missing.
// Gap: a unit drawn through gfx_unit_shader's Lua material ignores the tint. The art_unit shading itself (normal map,
// reflection) has to be in the engine's model shader before that gadget can go: a stock shader and drawer change.
//
// Check column "uniforms": the values written, on change only.

#ifndef KT1_DRAW_UNIT_MATERIAL_VALUES_H
#define KT1_DRAW_UNIT_MATERIAL_VALUES_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"

namespace KT1::UnitMaterial {

constexpr int TINT_SLOT = 12;    // DESIGN: ModelUniformData::userDefined[12..15], the engine's model tint

// written at TINT_SLOT, four floats
struct UnitMaterialValues {
	float tint[3] = {1.0f, 1.0f, 1.0f};
	float tintOn = 0.0f;         // 1: the multiplier applies
};

static_assert(sizeof(UnitMaterialValues) == 16, "one vec4 of the per-object uniforms");

struct UnitMaterialConfig {
	float select_gain = 1.05f;                   // DESIGN (owner 2026-11-14): a selected model's lit colour times this
	int select_max_level = 3;                    // DESIGN: brightened up to this zoom level
	std::string shader_param = "art_shader";     // DESIGN: defs whose custom parameter is "1" take our unit shading
	float emissive_gain = 2.0f;                  // DESIGN: art_unit's emissive gain
	float env_amount = 1.0f;                     // DESIGN: art_unit's reflection amount (option art_env)
	bool plain_shadow = false;                   // DESIGN: their shadows in the engine's instanced pass (option art_shadow_mat=0)
	template<class S> void Describe(S& s) {
		s.Field("select_gain", select_gain);
		s.Field("select_max_level", select_max_level);
		s.Field("shader_param", shader_param);
		s.Field("emissive_gain", emissive_gain);
		s.Field("env_amount", env_amount);
		s.Field("plain_shadow", plain_shadow);
	}
};

struct UnitMaterialFacts {
	bool selected = false;
	bool asIcon = false;         // the engine draws its icon instead of the model
	bool masked = false;         // its draw mask is 0
};

struct UnitMaterialWrite {
	int32_t id = -1;
	UnitMaterialValues values;
};

struct UnitMaterialState {
	Core::DenseIndex units;
	Core::Column<UnitMaterialValues> written;    // what the block holds
	std::vector<UnitMaterialWrite> writes;       // this frame's changes, for the adapter to put into the block
	uint32_t selectionSeen = 0;                  // FrameFacts::SelectionChanges at the last pass
	int levelSeen = -1;

	void Init(int32_t maxIds);
	void Remove(int32_t id);
};

UnitMaterialValues ValuesFor(const UnitMaterialConfig& config, const UnitMaterialFacts& facts, int zoomLevel);
bool Set(UnitMaterialState& state, int32_t id, const UnitMaterialValues& values);   // true: it differs, a write is queued

const UnitMaterialState& State();   // the running system's

} // namespace KT1::UnitMaterial

#endif
