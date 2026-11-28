/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: the engine as Core/Targeting reads and writes it (LUA-FREE S19). Each read is the engine call the Lua's
// Spring.* call made, so the native rule sees what the gadget saw; each write asks Check::MayWrite (check mode writes
// nothing).

#ifndef KT1_TARGETING_WORLD_H
#define KT1_TARGETING_WORLD_H

#include "Game/KT1/Core/Targeting/TargetState.h"

namespace KT1::Targeting {

class EngineWorld : public Core::Targeting::TargetingWorld {
public:
	bool ValidUnit(int32_t unit) override;
	bool DeadUnit(int32_t unit) override;
	int32_t UnitDef(int32_t unit) override;
	int32_t UnitTeam(int32_t unit) override;
	int32_t AllyTeam(int32_t unit) override;
	bool Allied(int32_t teamA, int32_t teamB) override;
	bool Arriving(int32_t unit) override;
	bool ScriptAI(int32_t team) override;
	int32_t LosBits(int32_t unit, int32_t allyTeam) override;
	bool InLos(int32_t unit, int32_t allyTeam) override;
	bool Position(int32_t unit, float (&pos)[3]) override;
	bool MidPosition(int32_t unit, float (&pos)[3]) override;
	float Height(int32_t unit) override;
	float Radius(int32_t unit) override;
	Core::Targeting::Layer MovementLayer(int32_t unit) override;
	float GroundHeight(float x, float z) override;
	void UnitsInCylinder(float x, float z, float radius, std::vector<int32_t>& out) override;
	void UnitsInRectangle(float x1, float z1, float x2, float z2, std::vector<int32_t>& out) override;
	int32_t FireState(int32_t unit) override;
	Core::Targeting::CommandView CurrentCommand(int32_t unit) override;
	bool WeaponRange(int32_t unit, int32_t weapon, float& range) override;
	int32_t WeaponTarget(int32_t unit, int32_t weapon) override;
	int32_t ChosenTarget(int32_t unit, int32_t weapon) override;
	void SetWeaponTarget(int32_t unit, int32_t weapon, int32_t target) override;
	void WriteRefusal(int32_t unit, Core::Targeting::Refusal reason, int32_t step) override;
};

} // namespace KT1::Targeting

#endif
