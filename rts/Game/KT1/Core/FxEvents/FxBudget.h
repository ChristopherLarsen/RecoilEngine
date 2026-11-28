/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS family: the effect budget (replaces code/gamedata/fx_budget.lua). No engine include.
//
// One instance in the synced system (option: mod option fx_scale, the same on every machine) and one on the draw side
// (setting KardashevFxScale; Draw/FxCommon/FxView.h). Admit per request, NewStep once per sim step; the second turns over
// every Core::Rate::Every(1) steps. Hit confirmation is never dropped while the option is on.

#ifndef KT1_FX_BUDGET_H
#define KT1_FX_BUDGET_H

#include <array>
#include <cstdint>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxTypes.h"

namespace KT1::Core::Fx {

struct BudgetPoint {
	float scale = 0.0f;                 // PLACEHOLDER: the option value (docs/vfx/PLAN.md section 4)
	float per_second = 0.0f;            // PLACEHOLDER: effect systems per second at that value
	template<class S> void Describe(S& s) {
		s.Field("scale", scale);
		s.Field("per_second", per_second);
	}
};

struct BudgetConfig {
	// PLACEHOLDER: budget points (option value -> effect systems per second), linear in between, flat above the last
	std::vector<BudgetPoint> points = {{0.0f, 0.0f}, {0.5f, 300.0f}, {1.0f, 1200.0f}, {2.0f, 3000.0f}};
	// DESIGN: by priority code, a request is admitted while the second's spending is below this share ([0] hit: unused)
	std::array<float, PRIORITIES> share = {1.0f, 0.95f, 0.85f, 0.75f, 0.6f, 0.5f};
	float step_burst = 1.6f;            // DESIGN: per-step limit = per second / steps per second x this (not for hit)
	int step_min = 4;                   // DESIGN: the least per-step limit while the budget is on
	template<class S> void Describe(S& s) {
		s.List("points", points, [](auto& o, BudgetPoint& p) { p.Describe(o); });
		s.Field("share", share);
		s.Field("step_burst", step_burst);
		s.Field("step_min", step_min);
	}
};

enum class Refusal : uint8_t { None, Off, Second, Step };

struct BudgetStats {                    // by priority code - 1
	std::array<uint32_t, PRIORITIES> requested{}, spawned{}, dropped{};
	uint32_t off = 0, second = 0, step = 0;
};

float BudgetPerSecond(const BudgetConfig& c, float scale);
int32_t BudgetPerStep(const BudgetConfig& c, float scale);

class Budget {
public:
	void Init(const BudgetConfig& config, float scale);   // after Rate is set; keeps its own copy
	void SetScale(float scale);
	void NewStep(int32_t step);
	bool Admit(Priority priority, Refusal* why = nullptr);
	float ScaleValue() const { return scale; }
	float PerSecond() const { return perSecond; }
	const BudgetStats& Stats() const { return stats; }

private:
	BudgetConfig config;
	BudgetStats stats;
	float scale = 1.0f, perSecond = 0.0f;
	int32_t perStep = 0, secondUsed = 0, stepUsed = 0;
};

} // namespace KT1::Core::Fx

#endif
