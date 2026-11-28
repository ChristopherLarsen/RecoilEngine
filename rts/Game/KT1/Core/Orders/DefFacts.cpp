/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: a def's accepted-order words and its default fire state (DefFacts.h). No engine include.

#include "DefFacts.h"

#include <array>

#include "OrdersConfig.h"

namespace KT1::Core::Orders {

namespace {

// the packs' words, in the order of AcceptWord
constexpr std::array ACCEPT_WORDS = {"move", "attack", "patrol", "guard", "fire_state", "reclaim", "repair", "capture",
	"pause", "self_destruct", "attack_move"};
static_assert(ACCEPT_WORDS.size() == static_cast<size_t>(AcceptWord::Count));

} // namespace

uint32_t ParseAccepts(std::string_view commaList) {
	uint32_t bits = 0;
	while (!commaList.empty()) {
		const size_t comma = commaList.find(',');
		const std::string_view word = commaList.substr(0, comma);   // as written: the Lua does not trim
		commaList = (comma == std::string_view::npos) ? std::string_view() : commaList.substr(comma + 1);
		for (size_t i = 0; i < ACCEPT_WORDS.size(); ++i) {
			if (word == ACCEPT_WORDS[i]) bits |= BitOf(static_cast<AcceptWord>(i));
		}
	}
	return bits;
}

bool AcceptWordOf(CommandKind kind, AcceptWord& word) {
	switch (kind) {
		case CommandKind::Move: word = AcceptWord::Move; return true;
		case CommandKind::Attack: word = AcceptWord::Attack; return true;
		case CommandKind::Patrol: word = AcceptWord::Patrol; return true;
		case CommandKind::Guard: word = AcceptWord::Guard; return true;
		case CommandKind::FireState:
		case CommandKind::GameFireState: word = AcceptWord::FireState; return true;
		case CommandKind::Reclaim: word = AcceptWord::Reclaim; return true;
		case CommandKind::Repair: word = AcceptWord::Repair; return true;
		case CommandKind::Capture: word = AcceptWord::Capture; return true;
		case CommandKind::Wait: word = AcceptWord::Pause; return true;
		case CommandKind::SelfDestruct: word = AcceptWord::SelfDestruct; return true;
		case CommandKind::Fight: word = AcceptWord::AttackMove; return true;
		default: return false;
	}
}

FireState DefaultFireState(const DefFacts& def, const FireConfig& config) {
	const std::string& name = !def.Has(DefFlag::CanMove) ? config.structure
		: (def.tier >= config.experimental_tier)         ? config.experimental
		: def.Has(DefFlag::CommandUnit)                  ? config.command_unit
		                                                 : config.other;
	FireState state = FireState::Free;   // DESIGN: a name no state has gives the engine's free fire
	ParseFireState(name, state);
	return state;
}

} // namespace KT1::Core::Orders
