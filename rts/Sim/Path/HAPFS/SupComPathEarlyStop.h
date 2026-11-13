/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_PATH_EARLY_STOP_H
#define SUPCOM_PATH_EARLY_STOP_H

#include "System/float3.h"

struct MoveDef;
class CSolidObject;

// supcom: springsetting PathEarlyStop (default 1; 0 is stock). A HAPFS request whose start is walled in (the start square and
// every square within two of it impassable to the move type: a structure, or speed 0) returns Error at once (PTH-005-H). Stock
// returns the same Error only after the start-block checks of every resolution level have failed: each of up to 9 small
// searches can leave the start through no square, so none succeeds. Requests with the goal near the start (where the start
// itself can count as reached) and directional move types are left to the stock search.
namespace SupComPathEarlyStop {
	bool On();
	bool StartWalledIn(const MoveDef& moveDef, const float3& startPos, const float3& goalPos, float goalRadius, const CSolidObject* owner);
}

#endif
