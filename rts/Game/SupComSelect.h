/* supcom (SEL-004-H, SEL-002 R3): a unit carried in a transport can be selected (ctrl-click map-wide includes carried units).
   Selection only: the carried unit's commands and simulation are unchanged. Selection is unsynced, so this needs no sync switch. */
#pragma once

class CUnit;

namespace SupComSelect {
	// true: the stock rule that a carried unit cannot be selected does not apply to <unit>
	bool CarriedSelectable(const CUnit* unit);
	// after a transport hides a passenger (void state sets noSelect), give back the unit's own selectability
	void AfterAttach(CUnit* unit);
}
