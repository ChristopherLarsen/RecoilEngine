/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: the rule class (KT1Targeting.cpp), seen by TargetingApi.cpp and TargetingBridge.cpp only.

#ifndef KT1_TARGETING_RULE_H
#define KT1_TARGETING_RULE_H

#include "KT1TargetingWorld.h"

#include "Game/KT1/Core/Targeting/TargetState.h"
#include "Game/KT1/Host/SimContext.h"
#include "Game/KT1/KT1Rule.h"

class CUnit;
struct Command;

namespace KT1::Targeting {

class TargetingRule : public Rule {
public:
	static constexpr const char* NAME = "Targeting";

	TargetingRule();
	// the rule after its LoadConfig (on or check), else nullptr
	static TargetingRule* Live();
	bool On() const { return Enabled() && SwitchMode() == Mode::On; }

	void LoadConfig(const LuaTable& cfg) override;
	void ConfigValues(Data::Values& values) const override;
	void Start() override;
	void PushLua(lua_State* L) override;

	Core::Targeting::TargetState state;
	Core::Targeting::TargetingDefs defs;
	Core::Targeting::TargetingParams params;
	Core::Targeting::TargetingConfig config;
	EngineWorld world;
	Core::Targeting::TargetingContext Context() const { return {defs, params}; }
	int32_t CurrentFrame() const;

private:
	void RegisterPhases();
	void RegisterAllow();
	void RegisterSync();
	void CommandGuns(const SimContext& c);
	void Events(const SimContext& c);
	void Created(const SimContext& c, int32_t id, int32_t def);
	void Destroyed(const SimContext& c, int32_t id);
	void ShotCreated(const SimContext& c, int32_t projectile, int32_t weaponDef);
	void ShotGone(const SimContext& c, int32_t projectile);
	bool AllowCommand(const CUnit* u, const Command& cmd);
};

} // namespace KT1::Targeting

#endif
