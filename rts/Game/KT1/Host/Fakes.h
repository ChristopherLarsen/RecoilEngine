/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: intel's fake contacts. A fake is a unit whose def has the custom parameter spc_blip "1", the one test the
// game's gamedata/fake_units.lua uses. As Lua gadgets do not hear a fake's unit call-ins, a native system does not see a
// fake's unit events and is not asked UnitPreDamaged about it, unless its RuleInfo.seesFakes is true. The other Allow
// call-ins are asked about every unit, as Lua is. A system that walks engine units itself leaves fakes out with Is().

#ifndef KT1_HOST_FAKES_H
#define KT1_HOST_FAKES_H

class CUnit;
struct UnitDef;

namespace KT1 {

class Fakes {
public:
	static bool IsDef(const UnitDef* def);   // every def read once, at the first question
	static bool Is(const CUnit* unit);
	static bool Sees(const char* system);    // the registered system's RuleInfo.seesFakes
	static void Reset();                     // game shutdown: the next game's defs are read again
};

} // namespace KT1

#endif
