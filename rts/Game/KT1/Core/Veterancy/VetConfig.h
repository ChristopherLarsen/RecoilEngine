/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Veterancy core: the system's config, game/data/kt1/veterancy.json (LUA-FREE H01, step S26). No engine include.
//
// Replaces the values of gamedata/vet_values.lua. The defaults are the design layer's (design/vet.lua); the development
// roster's values, every unit code and the requirements' test fixtures come from the data file only. Describe is a
// template over the config schema (Data/Schema.h).

#ifndef KT1_CORE_VET_CONFIG_H
#define KT1_CORE_VET_CONFIG_H

#include <array>
#include <string>
#include <vector>

namespace KT1::Core::Veterancy {

constexpr int TIERS = 4;

// the class factor of VET-001 R4: "t1", "t2", "t3", "support_command", "experimental", "command" or "other"
struct ClassFactorConfig {
	std::string name;
	float factor = 0;                      // DESIGN: unset
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("factor", factor);
	}
};

// REQ VET-001 R3: a unit's own factor, by unit code
struct UnitFactorConfig {
	std::string code;
	float factor = 0;                      // DESIGN: unset
	template<class S> void Describe(S& s) {
		s.Field("code", code);
		s.Field("factor", factor);
	}
};

struct LevelValue {
	float value = 0;                       // DESIGN: unset
	template<class S> void Describe(S& s) { s.Field("value", value); }
};

// a named list of one value per level: a regeneration row, a command unit's thresholds or heal shares
struct LevelList {
	std::string name;                      // the row, or the unit code ("default": every other command unit)
	std::vector<LevelValue> levels;
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.List("levels", levels, [](auto& o, LevelValue& item) { item.Describe(o); });
	}
};

// the requirements' synthetic values, laid over the config by a test (GG.Vet.UseFixture); below 0 or empty: unset
struct VetFixture {
	std::string name;
	float factor = -1;                     // DESIGN: unset
	float own_mass = -1;                   // DESIGN: unset
	float max_level = -1;                  // DESIGN: unset
	float health_step = -1;                // DESIGN: unset
	float heal_factor = -1;                // DESIGN: unset
	float victim_cost = -1;                // DESIGN: unset
	float victim_health = -1;              // DESIGN: unset
	std::vector<ClassFactorConfig> factor_class;
	std::vector<LevelValue> command_vet_mass;
	std::vector<LevelValue> regen_row;
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("factor", factor);
		s.Field("own_mass", own_mass);
		s.Field("max_level", max_level);
		s.Field("health_step", health_step);
		s.Field("heal_factor", heal_factor);
		s.Field("victim_cost", victim_cost);
		s.Field("victim_health", victim_health);
		s.List("factor_class", factor_class, [](auto& o, ClassFactorConfig& item) { item.Describe(o); });
		s.List("command_vet_mass", command_vet_mass, [](auto& o, LevelValue& item) { item.Describe(o); });
		s.List("regen_row", regen_row, [](auto& o, LevelValue& item) { item.Describe(o); });
	}
};

struct VetConfig {
	int max_level = 4;                     // PLACEHOLDER (design/vet.lua; REQ VET-002 R2)
	float health_step = 0.07f;             // PLACEHOLDER REQ VET-002 R3: of the base maximum health, per level
	float heal_factor = 0.10f;             // REQ VET-002 v3 R4: a level-up heals this share of the base maximum
	bool heal_on_level_up = false;         // DESIGN: with no heal share, a level-up heals to full
	float factor_default = 1.8f;           // PLACEHOLDER (design/vet.lua)
	std::array<float, TIERS> factor_tier = {1.8f, 2.9f, 3.7f, 4.6f};   // PLACEHOLDER (design/vet.lua), tiers 1 to 4
	std::vector<ClassFactorConfig> factor_class;   // REQ VET-001 R4; DESIGN: empty (then the tier factor)
	std::vector<UnitFactorConfig> factor_of;       // REQ VET-001 R3; DESIGN: empty
	std::vector<LevelList> regen_rows;     // REQ VET-002 R5: t1, t2, t3_and_command, support_command, experimental
	std::vector<LevelList> command_vet_mass = {    // PLACEHOLDER REQ VET-003 R2: steps per level (design/vet.lua)
		{"default", {{700}, {1600}, {2800}, {4400}}}};
	std::vector<LevelList> command_heal;   // REQ VET-002 R4: command units' own heal shares; DESIGN: empty
	std::string naval_outlines = "ship,sub,carrier";   // DESIGN: icon outlines that take the regeneration one row up
	std::string lost_share = "redistribute";   // PLACEHOLDER REQ VET-003 R1: "drop" or "redistribute"
	float level_epsilon = 1e-9f;           // DESIGN: experience short of a threshold that still reaches it
	std::vector<VetFixture> fixtures;      // REQ VET-001 to VET-003 test values; DESIGN: empty

	template<class S> void Describe(S& s) {
		s.Field("max_level", max_level);
		s.Field("health_step", health_step);
		s.Field("heal_factor", heal_factor);
		s.Field("heal_on_level_up", heal_on_level_up);
		s.Field("factor_default", factor_default);
		s.Field("factor_tier", factor_tier);
		s.List("factor_class", factor_class, [](auto& o, ClassFactorConfig& item) { item.Describe(o); });
		s.List("factor_of", factor_of, [](auto& o, UnitFactorConfig& item) { item.Describe(o); });
		s.List("regen_rows", regen_rows, [](auto& o, LevelList& item) { item.Describe(o); });
		s.List("command_vet_mass", command_vet_mass, [](auto& o, LevelList& item) { item.Describe(o); });
		s.List("command_heal", command_heal, [](auto& o, LevelList& item) { item.Describe(o); });
		s.Field("naval_outlines", naval_outlines);
		s.Field("lost_share", lost_share);
		s.Field("level_epsilon", level_epsilon);
		s.List("fixtures", fixtures, [](auto& o, VetFixture& item) { item.Describe(o); });
	}
};

} // namespace KT1::Core::Veterancy

#endif
