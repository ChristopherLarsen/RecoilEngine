/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: combat target choice (game rule CMB-004, cmb_targeting.lua) as an engine step. Every rule value and the priority ranks
// are pushed from synced Lua; this file holds the per-frame work: the due wheel, the scan, the hold checks and applying the target.
// Mode 0 off, 1 on (the engine's own auto-target search is skipped for these weapons), 2 check (shadow: computes, applies nothing).

#ifndef SUPCOM_TARGETING_H
#define SUPCOM_TARGETING_H

struct lua_State;
class CWeapon;

namespace SupComTargeting {
	void PushSyncedCtrl(lua_State* L);
	// once per sim frame, before eventHandler.GameFrame
	void Update(int frame);
	// true when the engine step owns this weapon's automatic choice (mode 1 only)
	bool OwnsAutoTarget(const CWeapon* w);
	// true when the engine step owns this weapon (mode 1) and its gun holds no target: the game's AllowWeaponTarget
	// (cmb_targeting.lua AllowBody) then refuses every automatic target for it, so the idle command AI's own search
	// (MobileCAI::GenerateAttackCmd, AirCAI::AirAutoGenerateTarget) cannot produce an order and is skipped (SIM-RATE-001 R2:
	// a weapon looks for targets only on its own interval; SIM-CUTS-RANKED R-E, 2026-11-24)
	bool GunIdle(const CWeapon* w);

	// LUA-FREE S19: the step for a native caller (the KT1 Targeting rule): the same operations as Spring.SupComTgt*.
	// Weapon numbers are 0-based here, 1-based in Lua; -1 means none.
	struct WeaponRules {
		int scan = 1;                  // steps between scans (the Lua ScanFrames)
		double track = 1.0, rangeMin = 0.0;
		bool retarget = false, direct = false, air = false, surface = false, under = false, seafloor = false;
		bool above = false, below = false, noGround = false;
	};
	void Init(int mode, double insetElmos, int skipSteps, bool overkill);   // also clears every gun and weapon
	void SetRules(double insetElmos, int skipSteps, bool overkill);
	void SetWeapon(int weaponDefID, const WeaponRules& rules);
	void SetRank(int weaponDefID, int unitDefID, int rank);
	void AddGun(int unitID, int weaponIndex0, int weaponDefID, int firstScanFrame);
	void WakeUnit(int unitID, int frame);
	void UnitGone(int unitID, int nextFrame);
	void FireStateChanged(int unitID, int frame);
	void SetFocus(int unitID, int targetID);
	void SetInflight(int targetID, double damage);
	int TargetOf(int unitID, int weaponIndex0);
	int NextScanOf(int unitID, int weaponIndex0);
	void Stats(int& scans, int& candidates, int& holds);   // since the last call
}

#endif
