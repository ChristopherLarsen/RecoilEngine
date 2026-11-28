/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the generated registrar call (kt1_systems.gen.cpp, written by rts/Game/KT1/sources.cmake). Registry only.

#ifndef KT1_SYSTEMS_H
#define KT1_SYSTEMS_H

namespace KT1 {
	// calls RegisterSystem_<Name>() for every kt1_system() fragment entry, and SetSystemFamily for those with a FAMILY
	void RegisterAllSystems();
	void SetSystemFamily(const char* name, const char* family);
}

#endif
