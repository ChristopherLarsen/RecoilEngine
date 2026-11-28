/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the sim's display events. See DisplayEvents.h. No draw, sound or GUI include: playing them is the draw
// side's work.

#include "DisplayEvents.h"

#include "System/float3.h"

namespace KT1 {
namespace {

using Core::SimHost::DisplayEvent;
using Core::SimHost::DisplayKind;

Core::SimHost::DisplayQueue queue;

void Set(float (&to)[3], const float3& v) {
	to[0] = v.x;
	to[1] = v.y;
	to[2] = v.z;
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

void DisplayEvents::Publish(int step) { queue.Publish(step); }

void DisplayEvents::Replay(void (*sink)(const DisplayEvent&)) { queue.Replay(sink); }

void DisplayEvents::Reset() { queue.Clear(); }

} // namespace KT1
