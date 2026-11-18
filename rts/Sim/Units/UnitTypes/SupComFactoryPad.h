/* supcom (ECO-012 / FAC: factory pad-free delay): a factory unit def with customParams.supcom_pad_clear (elmos) starts its next
   build once every mobile unit standing on the build square has its centre at least that far from the build position (the pack's
   clear distance); structures and features block as before. Without the key: the stock test (any other object on the build
   square blocks). Unit defs are synced, so the rule is the same on every client. */
#pragma once

#include "System/float3.h"

class CFactory;

namespace SupComFactoryPad {
	bool Blocked(const CFactory* factory, const float3& buildPos);
}
