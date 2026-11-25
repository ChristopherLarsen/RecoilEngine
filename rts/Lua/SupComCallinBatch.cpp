/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: fight call-in batching for synced LuaRules. See SupComCallinBatch.h.

#include "SupComCallinBatch.h"

#include "LuaHandle.h"
#include "LuaHashString.h"
#include "LuaRules.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Weapons/WeaponDef.h"
#include "Sim/Misc/DamageArray.h"
#include "Game/GameHelper.h"

#include <vector>

namespace SupComCib {
	int pending = 0;

	static std::vector<lua_Number> buf;
	static std::vector<lua_Number> out;
	static CLuaHandle* batchH = nullptr;   // the synced LuaRules handle, set at FrameEnd
	static int mask = 0;
	static bool flushing = false;
	static bool skipNext = false;

	static inline int Accept(CLuaHandle* h, int bit) {
		// 0: run as stock; 1: queue only; 2: queue and run (check mode)
		if (h != batchH || flushing || (mask & bit) == 0)
			return 0;
		return (mask & CHECK) ? 2 : 1;
	}

	static inline void Put(int kind, lua_Number a, lua_Number b, lua_Number c, lua_Number d, lua_Number e,
	                       lua_Number f, lua_Number g, lua_Number i, lua_Number j, lua_Number k) {
		const size_t o = buf.size();
		buf.resize(o + STRIDE);
		lua_Number* p = &buf[o];
		p[0] = kind; p[1] = a; p[2] = b; p[3] = c; p[4] = d; p[5] = e; p[6] = f; p[7] = g; p[8] = i; p[9] = j; p[10] = k;
		pending += 1;
	}

	bool UnitDamaged(CLuaHandle* h, const CUnit* unit, const CUnit* attacker, float damage, int weaponDefID, int projectileID, bool paralyzer) {
		const int a = Accept(h, UNIT_DAMAGED);
		if (a == 0)
			return false;
		// the synced handle has full read: the attacker is always visible and typed (LuaUtils::PushAttackerInfo)
		Put(UNIT_DAMAGED, unit->id, unit->unitDef->id, unit->team, damage, paralyzer ? 1 : 0, weaponDefID, projectileID,
		    attacker ? attacker->id : -1, attacker ? attacker->unitDef->id : -1, attacker ? attacker->team : -1);
		return a == 1;
	}

	bool ProjectileCreated(CLuaHandle* h, int proID, int ownerID, int weaponDefID) {
		const int a = Accept(h, PROJ_CREATED);
		if (a == 0)
			return false;
		Put(PROJ_CREATED, proID, ownerID, weaponDefID, 0, 0, 0, 0, 0, 0, 0);
		return a == 1;
	}

	bool ProjectileDestroyed(CLuaHandle* h, int proID, int ownerID, int weaponDefID) {
		const int a = Accept(h, PROJ_DESTROYED);
		if (a == 0)
			return false;
		Put(PROJ_DESTROYED, proID, ownerID, weaponDefID, 0, 0, 0, 0, 0, 0, 0);
		return a == 1;
	}

	bool Explosion(CLuaHandle* h, int weaponDefID, const CExplosionParams& params) {
		const int a = Accept(h, EXPLOSION);
		if (a == 0)
			return false;
		Put(EXPLOSION, weaponDefID, params.pos.x, params.pos.y, params.pos.z, (params.owner != nullptr) ? params.owner->id : -1,
		    params.projectileID, 0, 0, 0, 0);
		return a == 1;
	}

	void BeforeCallIn(CLuaHandle* h) {
		if (h != batchH)
			return;
		const bool skip = skipNext;
		skipNext = false;
		if (pending == 0 || flushing || skip || (mask & CHECK) != 0)   // check mode: one hand-over per frame, at FrameEnd
			return;
		h->SupComCibCall(0);
	}

	void PreDamagedNext(CLuaHandle* h) {
		if (h == batchH && (mask & PRE_NO_FLUSH) != 0 && (mask & CHECK) == 0)
			skipNext = true;
	}

	void FrameEnd() {
		CLuaHandle* h = (luaRules != nullptr) ? static_cast<CLuaHandle*>(&luaRules->syncedLuaHandle) : nullptr;
		if (h != batchH) {
			buf.clear();
			pending = 0;
			batchH = h;
		}
		skipNext = false;
		if (h == nullptr) {
			mask = 0;
			return;
		}
		if (pending > 0)
			h->SupComCibCall(0);
		mask = h->SupComCibCall(1);
	}
}

// op 0: hand the queue to Lua (SupComFightBatch, or SupComFightBatchCheck in check mode); op 1: read SupComFightBatchMask
int CLuaHandle::SupComCibCall(int op)
{
	using namespace SupComCib;
	if (op == 1) {
		lua_getglobal(L, "SupComFightBatchMask");
		const int m = lua_isnumber(L, -1) ? static_cast<int>(lua_tonumber(L, -1)) : 0;
		lua_pop(L, 1);
		return m;
	}

	out.clear();
	out.swap(buf);
	const int n = pending;
	pending = 0;
	if (n == 0)
		return 0;

	flushing = true;
	luaL_checkstack(L, 4, __func__);
	static const LuaHashString runStr("SupComFightBatch");
	static const LuaHashString chkStr("SupComFightBatchCheck");
	const LuaHashString& hs = (mask & CHECK) ? chkStr : runStr;
	if (hs.GetGlobalFunc(L)) {
		lua_pushnumber(L, n);
		lua_createtable(L, static_cast<int>(out.size()), 0);
		for (size_t i = 0; i < out.size(); ++i) {
			lua_pushnumber(L, out[i]);
			lua_rawseti(L, -2, static_cast<int>(i + 1));
		}
		RunCallIn(L, hs, 2, 0);
	}
	flushing = false;
	return n;
}
