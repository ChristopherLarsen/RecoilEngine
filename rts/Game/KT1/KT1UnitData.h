/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: per-unit data for rules. The engine never names a unit: a rule reads a unit def's values by key.
//
// In a unit def the game writes either
//     kt1 = { census = { weight = 3 } }                  (a table: gamedata/unitdefs_post.lua flattens it), or
//     customParams = { kt1_census_weight = "3" }          (the flat form the engine sees)
// and the rule reads   KT1::DefNum(def, "census", "weight", cfgDefault).
// Read per def once in Rule::Start into a vector indexed by def id; never per unit per frame.

#ifndef KT1_UNIT_DATA_H
#define KT1_UNIT_DATA_H

#include <cstdlib>
#include <string>

#include "Sim/Units/UnitDef.h"

namespace KT1 {
	inline const std::string* DefParam(const UnitDef* d, const std::string& rule, const char* key) {
		if (d == nullptr) return nullptr;
		const auto it = d->customParams.find("kt1_" + rule + "_" + key);
		return (it == d->customParams.end()) ? nullptr : &it->second;
	}
	inline float DefNum(const UnitDef* d, const std::string& rule, const char* key, float def) {
		const std::string* v = DefParam(d, rule, key);
		if (v == nullptr || v->empty()) return def;
		char* end = nullptr;
		const float f = std::strtof(v->c_str(), &end);
		return (end == v->c_str()) ? def : f;
	}
	inline bool DefBool(const UnitDef* d, const std::string& rule, const char* key, bool def) {
		const std::string* v = DefParam(d, rule, key);
		if (v == nullptr || v->empty()) return def;
		return !(*v == "0" || *v == "false");
	}
}

#endif
