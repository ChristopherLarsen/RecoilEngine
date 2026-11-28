/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: the sim half of the sound cues (LUA-FREE D10): the facts it reads from the engine and the one way a cue
// leaves the sim. Shared by the rule (KT1Cues.cpp), the cue API (CueApi.cpp) and the bridge. Outputs only: nothing here
// is read by the simulation.

#ifndef KT1_RULES_CUE_SIM_H
#define KT1_RULES_CUE_SIM_H

#include <string_view>
#include <vector>

#include "Game/KT1/Core/Cues/CueState.h"
#include "Game/KT1/Host/SimContext.h"
#include "System/float3.h"

class CUnit;

namespace KT1::Cues {

constexpr const char* NAME = "Cues";

// what a cue is about
struct Subject {
	int unit = 0;                     // 0: a point
	int def = 0;
	int team = 0;
	int weapon = 0;                   // the weapon def of a fire or impact cue
	float3 pos;
	std::string_view variant;
};

Subject Of(const CUnit* unit);

class CueSim {
public:
	void Init(const Core::Cues::CueConfig& config, int maxUnits, int weaponDefs);
	void Step(const SimContext& c);        // FrameStart: units from the events, this step's budget, movement, build loops
	void Events(const SimContext& c);      // FrameEnd: shots and impacts, as the engine raised them

	// a cue under the step's limit; false: cut. check: reported to Host/Check instead of sent.
	bool Send(std::string_view cue, const Subject& subject);
	void Emit(std::string_view cue, const Subject& subject);      // outside the limit (shields, as before)
	void Loop(int unit, bool on);                                 // a builder's loop: outside the limit
	void Trigger(std::string_view on, const Subject& subject, std::string_view unitClass, std::string_view weapon);

	Core::Cues::CueState& State() { return state; }
	const Core::Cues::CueParams& Params() const { return params; }

private:
	void Track(const SimContext& c);
	void Moves(int step);
	void Loops();
	void Fired(const Core::SimHost::Event& e);
	void Hit(const Core::SimHost::Event& e);

	Core::Cues::CueConfig config;
	Core::Cues::CueParams params;
	Core::Cues::CueState state;
	std::vector<int> ids;             // scratch: the tracked units, ascending
	bool check = false;
};

CueSim* ActiveSim();                  // the running rule's sim; nullptr: the rule is off
void SetActiveSim(CueSim* sim);

} // namespace KT1::Cues

#endif
