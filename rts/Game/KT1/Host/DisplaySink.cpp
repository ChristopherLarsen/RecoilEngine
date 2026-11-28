/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: playing the sim's display events (see DisplaySink.h). The one place that turns them into engine sound,
// effect, log, debug line and ping calls.
//
// It runs outside the sim step, so it must create nothing synced: every class an explosion generator can spawn is
// unsynced (ExpGenSpawnable.cpp), geometric debug lines are unsynced projectiles, and map points are local.
// An effect gets no owner: by now the unit may be dead and its id reused.

#include "DisplaySink.h"

#if __has_include("Game/KT1/Host/DisplayEvents.h")

#include "Game/Game.h"
#include "Game/InMapDrawModel.h"
#include "Game/KT1/Host/DisplayEvents.h"
#include "Sim/Misc/GeometricObjects.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Projectiles/ExplosionGenerator.h"
#include "System/Log/ILog.h"
#include "System/Sound/ISoundChannels.h"
#include "System/float3.h"

namespace KT1 {
namespace DisplaySink {
namespace {

using Core::SimHost::DisplayEvent;
using Core::SimHost::DisplayKind;

float3 ToFloat3(const float (&v)[3]) { return float3(v[0], v[1], v[2]); }

void Play(const DisplayEvent& e) {
	const bool skipping = game != nullptr && game->skipping;   // fast-forwarding: no sounds, no pings
	switch (e.kind) {
		case DisplayKind::Sound: {
			if (!skipping) Channels::Battle->PlaySample(e.id, ToFloat3(e.a), e.value);
		} break;
		case DisplayKind::Effect: {
			explGenHandler.GenExplosion(e.id, ToFloat3(e.a), ToFloat3(e.b), e.value, e.size, 1.0f, nullptr, ExplosionHitObject());
		} break;
		case DisplayKind::Log: {
			LOG("%s", e.text);
		} break;
		case DisplayKind::DebugLine: {
			const int steps = static_cast<int>(e.value * GAME_SPEED + 0.5f);   // seconds to sim steps, rounded
			geometricObjects->AddLine(ToFloat3(e.a), ToFloat3(e.b), e.size, 0, steps);
		} break;
		case DisplayKind::Ping: {
			if (!skipping) inMapDrawerModel->AddPoint(ToFloat3(e.a), e.text, e.unit);
		} break;
	}
}

} // namespace

bool Present() { return true; }
void Replay() { DisplayEvents::Replay(&Play); }

} // namespace DisplaySink
} // namespace KT1

#else

namespace KT1 {
namespace DisplaySink {

bool Present() { return false; }
void Replay() {}

} // namespace DisplaySink
} // namespace KT1

#endif
