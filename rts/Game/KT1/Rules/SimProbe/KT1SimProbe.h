/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: SimProbe, the sim host's test system (KT1SimProbe.cpp has the header). Shared by its two .cpp files.

#ifndef KT1_SIM_PROBE_H
#define KT1_SIM_PROBE_H

#include <array>
#include <cstdint>
#include <vector>

#include "Game/KT1/KT1Rule.h"
#include "Game/KT1/Core/SimHost/Phase.h"
#include "Game/KT1/Core/SimProbe/SimProbeCore.h"

namespace KT1 {

struct SimContext;

namespace SimProbe {

// The native Allow* validators, in AllowChain's order.
enum class Veto : uint8_t { Command, Creation, Transfer, BuildStep, CaptureStep, FeatureStep, Target, PreDamage, Count };

constexpr size_t VETOES = static_cast<size_t>(Veto::Count);

class SimProbeRule : public Rule {
public:
	SimProbeRule();

	void LoadConfig(const LuaTable& cfg) override;   // game setup: registers phases, validators and synced state
	void PushLua(lua_State* L) override;             // KT1SimProbeLua.cpp
	bool CheckMode() const { return SwitchMode() == Mode::Check; }

	Core::SimProbe::State state;
	// check mode only, until the Lua twin takes them
	std::vector<Core::SimHost::Event> stream;        // every event, as raised
	std::vector<int32_t> commands;                   // (unit id, command id) the command validator saw
	std::array<uint64_t, Core::SimHost::PHASES> seen{};   // events each phase's reader has had
	std::array<uint64_t, VETOES> asked{};            // calls each validator has had
	int denyCommand = 0;                             // the one command id the probe refuses (0: none)
	uint64_t dropped = 0;                            // check-mode records nobody took

private:
	void RegisterPhases();
	void RegisterValidators();
	void FrameStart(const SimContext& c);
	void TryWrites(const SimContext& c, int unitID);
	void Count(const SimContext& c);
	void Report(int frame);

	int reportEvery = 1;
	bool tried = false;   // the check-mode order and display event
};

} // namespace SimProbe
} // namespace KT1

#endif
