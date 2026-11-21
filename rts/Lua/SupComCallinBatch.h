/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: fight call-in batching for synced LuaRules (Kardashev PERF-IMPLEMENTATION-ORDER item 3, docs/engine/CALLIN-BATCH.md).
// UnitDamaged, ProjectileCreated, ProjectileDestroyed and Explosion for the synced LuaRules handle are queued as flat records and
// handed to Lua in one call, SupComFightBatch(n, records), in their original order. The queue is flushed before any other Lua call
// into that handle (RunCallInTraceback), so Lua sees every event in today's order relative to every other call-in; the one
// optional exception is UnitPreDamaged (mask bit 16). Flushed again at the end of each sim frame. Off (no SupComFightBatchMask
// global in synced LuaRules, or 0) changes nothing: no event is queued, every call-in runs as stock.

#ifndef SUPCOM_CALLIN_BATCH_H
#define SUPCOM_CALLIN_BATCH_H

class CLuaHandle;
class CUnit;
class CProjectile;
struct CExplosionParams;

namespace SupComCib {
	enum {
		UNIT_DAMAGED = 1, PROJ_CREATED = 2, PROJ_DESTROYED = 4, EXPLOSION = 8,
		PRE_NO_FLUSH = 16,   // UnitPreDamaged does not flush the queue (relaxed order: pre-damage handlers run before queued events)
		CHECK = 64,          // check mode: every event still runs at once; the queued copy goes to SupComFightBatchCheck at flush
	};
	constexpr int STRIDE = 11;   // kind + 10 numbers per record

	extern int pending;       // records queued (0: nothing to flush)

	// true: queued, the caller returns (Explosion: returns false, no noGfx)
	bool UnitDamaged(CLuaHandle* h, const CUnit* unit, const CUnit* attacker, float damage, int weaponDefID, int projectileID, bool paralyzer);
	bool ProjectileCreated(CLuaHandle* h, int proID, int ownerID, int weaponDefID);
	bool ProjectileDestroyed(CLuaHandle* h, int proID, int ownerID, int weaponDefID);
	bool Explosion(CLuaHandle* h, int weaponDefID, const CExplosionParams& params);

	// at the top of CLuaHandle::RunCallInTraceback: flush before any other Lua call into the batch handle
	void BeforeCallIn(CLuaHandle* h);
	// CSyncedLuaHandle::UnitPreDamaged, right before its RunCallInTraceback: skip that one flush if mask bit 16 is set
	void PreDamagedNext(CLuaHandle* h);
	// Game.cpp, before eventHandler.GameFramePost: flush, then re-read the mask
	void FrameEnd();
}

#endif
