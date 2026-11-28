/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxDecals: the unit tracks. Stub (LUA-FREE H01): step D16 writes the bodies.

#include "Game/KT1/Draw/Decals/TrackMarks.h"

namespace KT1::FxDecals {

TrackBudget BudgetOf(const TrackConfig&, float) { return {}; }
TrackKind KindOf(const Core::Fx::UnitDefFx&) { return TrackKind::None; }
void InitTracks(TrackState&, const TrackConfig&, int32_t) {}
void AddUnit(TrackState&, const TrackConfig&, int32_t, int32_t, const Core::Fx::UnitDefFx&, float, float) {}
void RemoveUnit(TrackState&, int32_t) {}
void StepTracks(TrackState&, const TrackConfig&, std::span<const WalkerIn>, int32_t, float, std::vector<Contact>&,
	DecalOutput&) {}
void SwitchTreads(TrackState&, std::span<TreadCandidate>, bool) {}
void ExpireTracks(TrackState&, float, DecalOutput&) {}
void ForgetMarks(TrackState&) {}

} // namespace KT1::FxDecals
