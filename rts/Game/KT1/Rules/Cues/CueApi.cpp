/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: the bodies of Audio/Cues/CueApi.h (LUA-FREE D10; they read units, so they live with the sim rule).

#include "Game/KT1/Audio/Cues/CueApi.h"   // KT1 lint: allow sim-draw-include cue API

#include <string>

#include "CueSim.h"
#include "Game/KT1/Core/Cues/CueEvents.h"
#include "Game/KT1/Host/DisplayEvents.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1::Cues {
namespace {

const CUnit* Live(int id) {
	return (id >= 0 && id < static_cast<int>(unitHandler.MaxUnits())) ? unitHandler.GetUnit(id) : nullptr;
}

void SendAlert(std::string_view kind, std::string_view severity, float x, float z, int allyTeam) {
	if (ActiveSim() == nullptr) return;
	Core::Cues::CueAlertEvent e;
	e.allyTeam = allyTeam;
	e.x = x;
	e.z = z;
	Core::Cues::SetName(e.kind, kind);
	Core::Cues::SetName(e.severity, severity);
	DisplayEvents::Send(e);
}

} // namespace

bool Raise(std::string_view cue, int unitId, std::string_view variant) {
	CueSim* sim = ActiveSim();
	const CUnit* u = Live(unitId);
	if (sim == nullptr || cue.empty() || u == nullptr) return false;
	Subject s = Of(u);
	s.variant = variant;
	return sim->Send(cue, s);
}

bool RaiseAt(std::string_view cue, const float3& pos, int team, std::string_view variant) {
	CueSim* sim = ActiveSim();
	if (sim == nullptr || cue.empty()) return false;
	Subject s;
	s.team = team;
	s.pos = pos;
	s.variant = variant;
	return sim->Send(cue, s);
}

void Alert(std::string_view kind, int allyTeam) { SendAlert(kind, {}, 0.0f, 0.0f, allyTeam); }

void AlertAt(std::string_view kind, std::string_view severity, float x, float z, int allyTeam) {
	SendAlert(kind, severity, x, z, allyTeam);
}

void ShieldHit(int unitId, const float3& pos) {
	CueSim* sim = ActiveSim();
	const CUnit* u = Live(unitId);
	if (sim == nullptr || u == nullptr || !Core::Cues::ShieldHitAdmit(sim->State(), sim->Params(), unitId)) return;
	Subject s = Of(u);
	s.pos = pos;
	sim->Emit("shield_hit", s);
}

void ShieldState(int unitId, int up, std::string_view reason, float height) {
	CueSim* sim = ActiveSim();
	if (sim == nullptr) return;
	using Core::Cues::ShieldCue;
	const ShieldCue cue = Core::Cues::ShieldStateStep(sim->State(), unitId, up, reason == "collapsed");
	const CUnit* u = Live(unitId);
	if (cue == ShieldCue::None || u == nullptr) return;
	Subject s = Of(u);
	s.pos.y += height;
	sim->Emit(cue == ShieldCue::Up ? "shield_up" : cue == ShieldCue::Break ? "shield_break" : "shield_fall", s);
}

void CaptureProgress(int targetId, int captorId, float progress) {
	CueSim* sim = ActiveSim();
	if (sim == nullptr) return;
	using Core::Cues::CaptureCue;
	const CaptureCue cue = Core::Cues::CaptureStep(sim->State(), sim->Params(), targetId, captorId, progress);
	if (cue == CaptureCue::None) return;
	// the capturing side hears these: the captor is the subject while it lives
	Raise(cue == CaptureCue::Start ? "capture_start" : "capture_stall", Live(captorId) != nullptr ? captorId : targetId);
}

void CaptureDone(int oldUnitId, int newUnitId) {
	CueSim* sim = ActiveSim();
	if (sim == nullptr) return;
	Core::Cues::CaptureEnd(sim->State(), oldUnitId);
	Raise("capture_complete", newUnitId);
}

} // namespace KT1::Cues
