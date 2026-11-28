/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the effect budget (gamedata/fx_budget.lua B.PerSecond, B.PerFrame, B.New). Same arithmetic in the
// same order, in float (the Lua number). No engine include.

#include "Game/KT1/Core/FxEvents/FxBudget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "Game/KT1/Core/Common/Rate.h"

namespace KT1::Core::Fx {

float BudgetPerSecond(const BudgetConfig& c, float scale) {
	if (scale <= 0 || c.points.empty()) return 0;
	for (size_t i = 1; i < c.points.size(); ++i) {
		if (scale > c.points[i].scale) continue;
		const BudgetPoint& a = c.points[i - 1];
		const BudgetPoint& b = c.points[i];
		return a.per_second + (b.per_second - a.per_second) * (scale - a.scale) / (b.scale - a.scale);
	}
	return c.points.back().per_second;
}

int32_t BudgetPerStep(const BudgetConfig& c, float scale) {
	const float s = BudgetPerSecond(c, scale);
	if (s <= 0) return 0;
	const float burst = std::ceil(s / static_cast<float>(Rate::StepsPerSecond()) * c.step_burst);
	return std::max<int32_t>(c.step_min, static_cast<int32_t>(burst));
}

void Budget::Init(const BudgetConfig& newConfig, float newScale) {
	config = newConfig;
	stats = {};
	secondUsed = 0;
	stepUsed = 0;
	SetScale(newScale);
}

void Budget::SetScale(float newScale) {
	scale = newScale;
	perSecond = BudgetPerSecond(config, newScale);
	perStep = BudgetPerStep(config, newScale);
}

void Budget::NewStep(int32_t step) {
	stepUsed = 0;
	if (step % Rate::Every(1) == 0) secondUsed = 0;
}

// KT1 hot
bool Budget::Admit(Priority priority, Refusal* why) {
	int32_t code = static_cast<int32_t>(priority);
	if (code < 1 || code > PRIORITIES) code = 1;     // the Lua: an unknown priority name is "hit"
	const size_t at = static_cast<size_t>(code - 1);
	++stats.requested[at];
	Refusal refusal = Refusal::None;
	if (perSecond <= 0) {
		refusal = Refusal::Off;
		++stats.off;
	} else if (priority != Priority::Hit) {          // hit confirmation is never dropped while the option is on
		if (static_cast<float>(secondUsed) >= perSecond * config.share[at]) {
			refusal = Refusal::Second;
			++stats.second;
		} else if (stepUsed >= perStep) {
			refusal = Refusal::Step;
			++stats.step;
		}
	}
	if (why != nullptr) *why = refusal;
	if (refusal != Refusal::None) {
		++stats.dropped[at];
		return false;
	}
	++secondUsed;
	++stepUsed;
	++stats.spawned[at];
	return true;
}

} // namespace KT1::Core::Fx
