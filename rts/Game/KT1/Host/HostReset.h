/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the sim host's game shutdown. Registry::Reset (CGame::CGame, before a new game) calls it BEFORE the rules go
// (the phase functions, validators and sync columns point into them). Afterwards the host is as in a fresh process, so a
// second game in the same process starts like the first. A new host part with state of its own adds its Reset here.

#ifndef KT1_HOST_RESET_H
#define KT1_HOST_RESET_H

namespace KT1 {

void ResetSimHost();

} // namespace KT1

#endif
