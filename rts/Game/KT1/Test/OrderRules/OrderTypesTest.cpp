/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core cases (LUA-FREE S11): command names, the game's command ids, accepted-order sets, default fire states
// and the accepted-set test of OrderRules that uses them.

#include <string>

#include "Game/KT1/Core/OrderRules/GuardLogic.h"
#include "Game/KT1/Core/Orders/OrdersConfig.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::Orders;
using KT1::Core::OrderRules::PackAccepts;

KT1_CASE("ORDT-KINDS", "every command kind has one name that parses back to it") {
	for (int i = 1; i < static_cast<int>(CommandKind::Count); ++i) {
		const CommandKind kind = static_cast<CommandKind>(i);
		KT1_CHECK_MSG(ParseCommandKind(CommandKindName(kind)) == kind, CommandKindName(kind));
	}
	KT1_CHECK(ParseCommandKind("Move") == CommandKind::Other && ParseCommandKind("") == CommandKind::Other);
}

KT1_CASE("ORDT-LIST", "a command list is a mask; an unknown name fails and the known ones stay") {
	CommandMask mask = 0;
	KT1_CHECK(ParseCommandList("move, patrol", mask));
	KT1_CHECK(mask == (MaskOf(CommandKind::Move) | MaskOf(CommandKind::Patrol)));
	KT1_CHECK(!ParseCommandList("move,fly", mask));
	KT1_CHECK(mask == MaskOf(CommandKind::Move));
	KT1_CHECK(ParseCommandList("", mask) && mask == 0);
}

KT1_CASE("ORDT-CUSTOM", "the game's own command ids map both ways") {
	KT1_CHECK(CustomKind(39401) == CommandKind::Waypoint && CustomKind(39406) == CommandKind::Pickup);
	KT1_CHECK(CustomKind(39400) == CommandKind::Other && CustomKind(10) == CommandKind::Other);
	KT1_CHECK_EQ(CustomId(CommandKind::AMove), 39404);
	KT1_CHECK_EQ(CustomId(CommandKind::Move), 0);
	KT1_CHECK_EQ(std::string(FailReasonName(FailReason::BoxedIn)), std::string("boxed in"));
	KT1_CHECK_EQ(std::string(WorkKindName(WorkKind::None)), std::string(""));
}

KT1_CASE("ORDT-ACCEPTS", "accepted words are read as written; unknown words are left out") {
	KT1_CHECK_EQ(ParseAccepts("move,attack_move"), BitOf(AcceptWord::Move) | BitOf(AcceptWord::AttackMove));
	KT1_CHECK_EQ(ParseAccepts("move, attack,fly"), BitOf(AcceptWord::Move));
	KT1_CHECK_EQ(ParseAccepts(""), 0u);
}

KT1_CASE("ORDT-PACK", "a pack set refuses an order it lacks, except self-destruct, a factory rally and attack-move with attack") {
	DefFacts d;
	KT1_CHECK(PackAccepts(d, CommandKind::Move, false));
	d.Set(DefFlag::HasAccepts, true);
	d.accepts = BitOf(AcceptWord::Attack);
	KT1_CHECK(!PackAccepts(d, CommandKind::Move, false) && !PackAccepts(d, CommandKind::Wait, false));
	KT1_CHECK(PackAccepts(d, CommandKind::Attack, false) && PackAccepts(d, CommandKind::SelfDestruct, false));
	KT1_CHECK(PackAccepts(d, CommandKind::Fight, false) && PackAccepts(d, CommandKind::AMove, false));
	KT1_CHECK(PackAccepts(d, CommandKind::Stop, false) && PackAccepts(d, CommandKind::Build, false));
	d.accepts = BitOf(AcceptWord::Move);
	KT1_CHECK(!PackAccepts(d, CommandKind::Fight, false) && !PackAccepts(d, CommandKind::GameFireState, false));
	KT1_CHECK(PackAccepts(d, CommandKind::Fight, true));
}

KT1_CASE("ORDT-FIRE", "a new unit's fire state: cannot move, then tier, then command unit, then the rest") {
	FireConfig config;
	config.structure = "ground";
	config.experimental = "hold";
	config.command_unit = "ground";
	config.other = "free";
	DefFacts d;
	d.tier = 4;
	KT1_CHECK(DefaultFireState(d, config) == FireState::Ground);
	d.Set(DefFlag::CanMove, true);
	KT1_CHECK(DefaultFireState(d, config) == FireState::Hold);
	d.tier = 3;
	KT1_CHECK(DefaultFireState(d, config) == FireState::Free);
	d.Set(DefFlag::CommandUnit, true);
	KT1_CHECK(DefaultFireState(d, config) == FireState::Ground);
	config.command_unit = "nothing";
	KT1_CHECK(DefaultFireState(d, config) == FireState::Free);
}
