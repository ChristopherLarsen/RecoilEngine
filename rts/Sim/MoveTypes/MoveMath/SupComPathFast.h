/* supcom (PTH-005-H, docs/engine/PTH-005-H-PERF-DESIGN.md): a faster CMoveMath::IsBlockedNoSpeedModCheckDiff with the same result */
#pragma once

#include "MoveMath.h"

namespace SupComPathFast {
	// springsetting SupComPathFast: 0 = stock, 1 = fast (default), 2 = fast and cross-checked against stock (logs every mismatch)
	int Mode();
	// true while the cross-check runs the stock code (the hook in MoveMath.cpp lets it through)
	bool InStockCheck();
	CMoveMath::BlockType IsBlockedDiff(const MoveDef& moveDef, int2 prevSqr, int2 newSqr, const CSolidObject* collider, int thread);
}
