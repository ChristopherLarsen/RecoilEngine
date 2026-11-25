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
}

#endif
