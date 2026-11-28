/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 UnitFx: the unit effect logic. Stub (LUA-FREE H01): step D03 writes the bodies.

#include "Game/KT1/Draw/UnitFx/UnitFxState.h"

namespace KT1::UnitFx {

void InitState(UnitFxState&, const UnitFxConfig&, int32_t) {}
void PollDamage(UnitFxState&, const Frame&, std::span<const SeenUnit>) {}
void PollAmbient(UnitFxState&, const Frame&, std::span<const SeenUnit>) {}
void EmitDamage(UnitFxState&, const Frame&) {}
void EmitAmbient(UnitFxState&, const Frame&, std::span<const SeenUnit>) {}
void EmitTimed(UnitFxState&, const Frame&) {}
void EmitPieceTrails(UnitFxState&, const Frame&, std::span<const Core::Fx::ShotSample>) {}
void EmitWreckTrails(UnitFxState&, const Frame&, std::span<const Core::Fx::FeatureFxSample>) {}
void EmitContrails(UnitFxState&, const Frame&, std::span<const SeenUnit>) {}

void Footfall(UnitFxState&, const Frame&, const Vec3&, float, Core::Fx::SizeBucket, int32_t, int32_t, const float[3]) {}
void OnTeleport(UnitFxState&, const Frame&, const Core::Fx::Teleport&, Core::Fx::Faction, bool) {}
void OnSacrifice(UnitFxState&, const Frame&, const Core::Fx::Sacrifice&, bool) {}
void OnRemnant(UnitFxState&, const Frame&, const Core::Fx::Remnant&) {}
void OnCrash(UnitFxState&, const Frame&, const Core::Fx::Crash&) {}
void OnCharge(UnitFxState&, const Frame&, const Core::TimedRules::ChargeProgress&, const Vec3&) {}
void OnChargeFired(UnitFxState&, const Frame&, const Core::TimedRules::ChargeFired&) {}
void OnDotTick(UnitFxState&, const Frame&, const Core::Damage::DotTick&, const Vec3&, float) {}
void OnBlast(UnitFxState&, const Frame&, const BlastIn&, std::span<const BurnCandidate>, std::vector<MarkAsk>&,
	std::vector<int32_t>&) {}

int32_t CapAt(const std::vector<CapPoint>&, float) { return 0; }

} // namespace KT1::UnitFx
