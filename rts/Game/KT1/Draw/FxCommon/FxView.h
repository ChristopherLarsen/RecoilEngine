/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS draw: what every effect system asks about the viewer: the player's switches and the frame rate guard
// (replaces LuaRules/cfx_switches.lua), the local effect budget (GG.FxUnsynced) and who the viewer may see for.
// One per process; whichever effect system draws first in a frame polls it.

#ifndef KT1_FX_VIEW_H
#define KT1_FX_VIEW_H

#include <cstdint>
#include <string>

#include "Game/KT1/Core/FxEvents/FxBudget.h"
#include "Game/KT1/Core/FxEvents/FxTypes.h"

namespace KT1 {

class FrameFacts;

namespace FxDraw {

enum class Switch : uint8_t {
	Lights, Blasts, BuildFx, Shields, Cloak, Shake, Shots, Damage, Tracks, Abilities, Ambient, Decals, Count
};

struct FxViewConfig {
	float poll_s = 0.5f;                       // DESIGN: LuaUI/fx/config.lua poll: settings are read this often
	std::string scale_key = "KardashevFxScale";           // DESIGN: the effect option (0 off .. 2 high)
	std::string scale_alias = "SplinterFxScale";          // DESIGN: the old name, read when the new one is unset
	std::string limiting_key = "KardashevFxGuardLimiting";   // DESIGN: set by the frame rate guard
	std::string guard_key = "KardashevFxGuard";           // DESIGN: the guard's own switch
	Core::Fx::BudgetConfig budget;
	template<class S> void Describe(S& s) {
		s.Field("poll_s", poll_s);
		s.Field("scale_key", scale_key);
		s.Field("scale_alias", scale_alias);
		s.Field("limiting_key", limiting_key);
		s.Field("guard_key", guard_key);
		s.Object("budget", [&](auto& o) { budget.Describe(o); });
	}
};

struct SwitchState {
	bool effects = true;        // the effect option is above 0
	bool on = true;             // the player's switch for this effect
	bool limiting = false;      // the frame rate guard is limiting
	bool guard = true;          // the player allows the guard
};

class FxView {
public:
	void Init(const FxViewConfig& config);
	// once per drawn frame is enough: later calls for the same frame do nothing
	void Frame(const FrameFacts& facts);
	SwitchState State(Switch s) const;
	bool Admit(Core::Fx::Priority priority);            // the local budget
	float Scale() const;
	bool Sees(Core::Fx::SeenMask seen) const;           // the local ally team, or a full view
	bool SeesAlly(int32_t allyTeam) const;              // own or allied, or a full view
	bool FullView() const;
	int32_t LocalTeam() const;
	int32_t LocalAllyTeam() const;
	void Reset();                                       // the game ends

private:
	FxViewConfig config;
	Core::Fx::Budget budget;
	SwitchState states[static_cast<int>(Switch::Count)];
	float nextPoll = 0.0f;
	uint32_t lastFrame = 0;
};

FxView& View();

} // namespace FxDraw
} // namespace KT1

#endif
