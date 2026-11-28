/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: replay of the sim's display events. See DisplayEvents.h. This is the one file on the sim's side that may
// include sound and effect headers.
//
// Replay runs inside SimFrame, after the step and still in synced scope, as Spring.SpawnCEG does today: an explosion
// generator may hold synced parts, and the queue is the same on every client. When the sim gets its own thread the
// replay moves to the draw side, and such generators must be settled then (NEW-ENGINE-PLAN Phase 5).

#include "DisplayEvents.h"

#include "Game/GameHelper.h"
#include "Game/InMapDrawModel.h"
#include "Sim/Misc/GeometricObjects.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Projectiles/ExplosionGenerator.h"
#include "Sim/Units/UnitHandler.h"
#include "System/Log/ILog.h"
#include "System/Sound/ISoundChannels.h"

namespace KT1 {
namespace {

using Core::SimHost::DisplayEvent;
using Core::SimHost::DisplayKind;

Core::SimHost::DisplayQueue queue;

float3 ToFloat3(const float (&v)[3]) { return float3(v[0], v[1], v[2]); }

void Set(float (&to)[3], const float3& v) {
	to[0] = v.x;
	to[1] = v.y;
	to[2] = v.z;
}

void Play(const DisplayEvent& e, bool skipping) {
	switch (e.kind) {
		case DisplayKind::Sound: {
			if (!skipping) Channels::Battle->PlaySample(e.id, ToFloat3(e.a), e.value);
		} break;
		case DisplayKind::Effect: {
			CUnit* owner = (e.unit >= 0) ? unitHandler.GetUnit(e.unit) : nullptr;   // dead by now: no owner
			explGenHandler.GenExplosion(e.id, ToFloat3(e.a), ToFloat3(e.b), e.value, e.size, 1.0f, owner, ExplosionHitObject());
		} break;
		case DisplayKind::Log: {
			LOG("%s", e.text);
		} break;
		case DisplayKind::DebugLine: {
			const int steps = static_cast<int>(e.value * GAME_SPEED + 0.5f);
			geometricObjects->AddLine(ToFloat3(e.a), ToFloat3(e.b), e.size, 0, steps);
		} break;
		case DisplayKind::Ping: {
			if (!skipping) inMapDrawerModel->AddPoint(ToFloat3(e.a), e.text, e.unit);
		} break;
	}
}

} // namespace

void DisplayEvents::Emit(const DisplayEvent& e) { queue.Emit(e); }

void DisplayEvents::Sound(int soundId, const float3& pos, float volume) {
	DisplayEvent e;
	e.kind = DisplayKind::Sound;
	e.id = soundId;
	Set(e.a, pos);
	e.value = volume;
	queue.Emit(e);
}

void DisplayEvents::Effect(int generatorId, const float3& pos, const float3& dir, float damage, float radius, int ownerId) {
	DisplayEvent e;
	e.kind = DisplayKind::Effect;
	e.id = generatorId;
	e.unit = ownerId;
	Set(e.a, pos);
	Set(e.b, dir);
	e.value = damage;
	e.size = radius;
	queue.Emit(e);
}

void DisplayEvents::Log(const char* text) {
	DisplayEvent e;
	e.kind = DisplayKind::Log;
	e.SetText(text);
	queue.Emit(e);
}

void DisplayEvents::DebugLine(const float3& from, const float3& to, float width, float seconds) {
	DisplayEvent e;
	e.kind = DisplayKind::DebugLine;
	Set(e.a, from);
	Set(e.b, to);
	e.size = width;
	e.value = seconds;
	queue.Emit(e);
}

void DisplayEvents::Ping(const float3& pos, const char* label, int playerId) {
	DisplayEvent e;
	e.kind = DisplayKind::Ping;
	e.unit = playerId;
	Set(e.a, pos);
	e.SetText(label);
	queue.Emit(e);
}

void DisplayEvents::Publish() { queue.Publish(); }

void DisplayEvents::Replay(bool skipping) {
	queue.Replay([skipping](const DisplayEvent& e) { Play(e, skipping); });
}

void DisplayEvents::Reset() { queue.Clear(); }

} // namespace KT1
