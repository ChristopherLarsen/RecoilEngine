/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: every call the family makes into other families' Lua beside target choice (OrdersTargetPick.h), in one
// place. Until the owning family is native the bodies ask its GG table through the bridge; afterwards its Api header.
// A missing gadget answers as the Lua does without it (stated per call). Unit ids; -1 is no unit.

#ifndef KT1_RULES_ORDERS_FOREIGN_H
#define KT1_RULES_ORDERS_FOREIGN_H

#include <span>
#include <vector>

namespace KT1 {

struct SimContext;

namespace Orders {

// ---- INTEL (GG.Int.state[ally][enemy] == "seen"; without it: the engine's line of sight) ----------------------------
bool SeenByArmy(const SimContext& context, int allyTeam, int enemy);

// ---- shields (GG.Shields.Room(unit) ~= nil; without it: false) -----------------------------------------------------
bool ShieldHasRoom(const SimContext& context, int unit);

// ---- GLUE: the group guard's ring places (GG.FrmGuard.place[unit]) -------------------------------------------------
struct RingPlace {
	int guarded = -1;
	float dx = 0, dz = 0;            // the place beside the guarded unit
	bool settled = false;
};
bool RingPlaceOf(const SimContext& context, int unit, RingPlace& place);   // false: no place
void SetRingSettled(const SimContext& context, int unit, bool settled);

// ---- GLUE: cargo (GG.SpcCargo) and transport pickup (GG.OrdPickup) -------------------------------------------------
// SelectLoad: which of the waiting units the transport takes, in order, and which are left. Without the gadget: the first
// one when nothing is carried.
void SelectLoad(const SimContext& context, int transport, std::span<const int> waiting, int carried,
                std::vector<int>& taken, std::vector<int>& left);
bool CargoFull(const SimContext& context, int transport);                      // without the gadget: false
void NoteLeftBehind(const SimContext& context, int transport, int count);      // GG.SpcCargo.NoteRefused
bool PickupInReach(const SimContext& context, int transport, int cargo);       // without the gadget: true

// ---- COMBAT: a ferry marker leaves without its death blast (GG.RemoveWithoutBlast[unit]) ---------------------------
void RemoveWithoutBlast(const SimContext& context, int unit);

} // namespace Orders
} // namespace KT1

#endif
