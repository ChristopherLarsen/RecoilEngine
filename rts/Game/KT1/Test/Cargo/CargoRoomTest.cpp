/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/Cargo/CargoRoom, engine-free: spc_cargo.lua's SelectLoad and Full (REQ ORD-009 v2 R1-R3, ORD-011 v3 R1, R2), with the
// roster's room model (G0-PFD [cargo]) and the SF-ORD-11 transport (4 small, 2 medium, 1 large).

#include "../KT1Test.h"

#include <vector>

#include "Game/KT1/Core/Cargo/CargoRoom.h"

using namespace KT1::Core::Cargo;

namespace {

constexpr int32_t S = 0, M = 1, L = 2;

RoomModel Roster() {
	RoomModel m;
	m.need = {Room{1, 0, 0}, Room{2, 1, 0}, Room{4, 2, 1}};
	m.use = {Room{1, 0.5f, 0.25f}, Room{2, 1, 0.5f}, Room{4, 2, 1}};
	return m;
}

Transport Sf11(std::vector<int32_t> aboard = {}) {
	Transport t;
	t.hasSlots = true;
	t.slots = {4, 2, 1};
	t.aboard = std::move(aboard);
	return t;
}

std::vector<int32_t> Taken(const Transport& t, std::vector<int32_t> sizes) {
	std::vector<int32_t> taken, left;
	SelectLoad(t, sizes, Roster(), taken, left);
	return taken;
}

} // namespace

KT1_CASE("CARGO-SMALLS", "ORD-011 R2: four small units fill the small room; the fifth is left") {
	KT1_CHECK(Taken(Sf11(), {S, S, S, S, S}) == std::vector<int32_t>({0, 1, 2, 3}));
}

KT1_CASE("CARGO-LARGE-FIRST", "a large unit takes all the room: a small one after it is left, in the given order") {
	KT1_CHECK(Taken(Sf11(), {L, S}) == std::vector<int32_t>({0}));
	KT1_CHECK(Taken(Sf11(), {S, L}) == std::vector<int32_t>({0}));   // a small first: the large no longer fits (3 < 4, 1.5 < 2, 0.75 < 1)
}

KT1_CASE("CARGO-MEDIUMS", "two medium units fit by the small room; a third is left") {
	KT1_CHECK(Taken(Sf11(), {M, M, M}) == std::vector<int32_t>({0, 1}));
}

KT1_CASE("CARGO-ABOARD", "the cargo aboard counts: with two small aboard, two more small fit") {
	KT1_CHECK(Taken(Sf11({S, S}), {S, S, S}) == std::vector<int32_t>({0, 1}));
	KT1_CHECK(Taken(Sf11({NO_SIZE}), {S, S, S, S}) == std::vector<int32_t>({0, 1, 2}));   // no class aboard: counted small
}

KT1_CASE("CARGO-NO-CLASS", "a candidate with no carry_class never fits a transport with slots") {
	KT1_CHECK(Taken(Sf11(), {NO_SIZE, S}) == std::vector<int32_t>({1}));
}

KT1_CASE("CARGO-FULL", "full: no size class fits any more") {
	KT1_CHECK(Full(Sf11({S, S, S, S}), Roster()));
	KT1_CHECK(!Full(Sf11({S, S, S}), Roster()));
	KT1_CHECK(Full(Sf11({L}), Roster()));
	KT1_CHECK(!Full(Sf11(), Roster()));
}

KT1_CASE("CARGO-NO-SLOTS", "a transport without slots takes up to its engine capacity, full at it") {
	Transport t;
	t.capacity = 2;
	t.aboard = {S};
	std::vector<int32_t> taken, left;
	const std::vector<int32_t> sizes = {S, M, L};
	SelectLoad(t, sizes, Roster(), taken, left);
	KT1_CHECK(taken == std::vector<int32_t>({0}));
	KT1_CHECK(left == std::vector<int32_t>({1, 2}));
	KT1_CHECK(!Full(t, Roster()));
	t.aboard.push_back(L);
	KT1_CHECK(Full(t, Roster()));
}
