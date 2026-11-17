/* supcom (SEL-004-H, SEL-002 R3), see SupComSelect.h */

#include "SupComSelect.h"

#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"

bool SupComSelect::CarriedSelectable(const CUnit* unit)
{
	return (unit != nullptr && unit->GetTransporter() != nullptr && unit->unitDef->selectable);
}

void SupComSelect::AfterAttach(CUnit* unit)
{
	if (unit != nullptr && unit->GetTransporter() != nullptr)
		unit->noSelect = !unit->unitDef->selectable;
}
