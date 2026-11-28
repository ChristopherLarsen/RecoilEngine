/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the build work rules (gamedata/fx_build_logic.lua L.Task, L.InReach, L.NanoPiece), which the synced
// side needs to publish BuildSample. No engine include.
//
// LoopsDue and LoopNext (the construction sound loops, L.LoopDue and L.LoopNext) stay stubs: they are the sound side's
// and are written with the step that moves the construction loops.

#include "Game/KT1/Core/FxEvents/FxBuildWork.h"

#include <algorithm>
#include <cctype>

namespace KT1::Core::Fx {

namespace {

constexpr int32_t ENGINE_CMD_REPAIR = 40;        // DESIGN: the engine's CMD_REPAIR (Sim/Units/CommandAI/Command.h)
constexpr int32_t ENGINE_CMD_RECLAIM = 90;       // DESIGN: the engine's CMD_RECLAIM
constexpr int32_t ENGINE_CMD_CAPTURE = 130;      // DESIGN: the engine's CMD_CAPTURE

char Lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

bool Contains(std::string_view name, std::string_view part) {
	return std::search(name.begin(), name.end(), part.begin(), part.end(),
		[](char a, char b) { return Lower(a) == b; }) != name.end();
}

bool Before(std::string_view a, std::string_view b) {
	return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
		[](char x, char y) { return static_cast<unsigned char>(Lower(x)) < static_cast<unsigned char>(Lower(y)); });
}

// the rank of the first of the "+" separated name parts that the name holds; -1: none
int32_t RankOf(std::string_view parts, std::string_view name) {
	for (int32_t rank = 0; !parts.empty(); ++rank) {
		const size_t plus = parts.find('+');
		const std::string_view part = parts.substr(0, plus);
		if (!part.empty() && Contains(name, part)) return rank;
		if (plus == std::string_view::npos) break;
		parts.remove_prefix(plus + 1);
	}
	return -1;
}

} // namespace

BuildTarget TargetOf(const BuilderOrder& order, int32_t maxUnits) {
	if (order.building >= 0)
		return {true, order.command == ENGINE_CMD_REPAIR ? BuildWork::Repair : BuildWork::Build, order.building, false};
	// no command, no target, or an area order (no single target yet)
	if (order.command == 0 || order.parameter < 0 || order.parameters > 1) return {};
	if (order.command == ENGINE_CMD_RECLAIM) {
		if (order.parameter >= maxUnits) return {true, BuildWork::Reclaim, order.parameter - maxUnits, true};
		return {true, BuildWork::Reclaim, order.parameter, false};
	}
	if (order.command == ENGINE_CMD_REPAIR) return {true, BuildWork::Repair, order.parameter, false};
	if (order.command == ENGINE_CMD_CAPTURE) return {true, BuildWork::Capture, order.parameter, false};
	return {};
}

bool InReach(const BuildWorkConfig& c, float distance, float buildDistance, float targetRadius) {
	return distance <= buildDistance + targetRadius + c.range_slack;
}

int32_t NanoPiece(const BuildWorkConfig& c, std::span<const std::string_view> pieces) {
	int32_t best = -1, bestRank = -1;
	for (size_t i = 0; i < pieces.size(); ++i) {
		const int32_t rank = RankOf(c.nano_names, pieces[i]);
		if (rank < 0) continue;
		if (best < 0 || rank < bestRank || (rank == bestRank && Before(pieces[i], pieces[best]))) {
			best = static_cast<int32_t>(i);
			bestRank = rank;
		}
	}
	return best;
}

void LoopsDue(const BuildWorkConfig&, std::vector<LoopState>&, std::span<const WorkingBuilder>, float, std::vector<int32_t>&) {}
void LoopNext(const BuildWorkConfig&, std::vector<LoopState>&, int32_t, float, float) {}

} // namespace KT1::Core::Fx
