/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage: DamageApi.h before step S21 (LUA-FREE H01): the system is not live, every answer is neutral.

#include "DamageApi.h"

namespace KT1::Damage {

bool Active() { return false; }

bool LastKill(int32_t, Core::Damage::KillRecord&) { return false; }

void MarkNoBlast(int32_t) {}
bool NoBlastMarked(int32_t) { return false; }

int32_t CrashingSince(int32_t) { return -1; }
bool CrashKillOf(int32_t, Core::Damage::CrashKill&) { return false; }
bool CrashContactOf(int32_t, Core::Damage::CrashContact&) { return false; }
const Core::Damage::BlastDef* EffectOf(int32_t) { return nullptr; }

bool Impact(int32_t, const float (&)[3], int32_t) { return false; }
int32_t Splits() { return 0; }

float ArmourMult(int32_t, const char*) { return 1; }

float DeathThreshold() { return 0; }
float DeathDelaySeconds() { return -1; }
float LandDeathToWreckSeconds() { return 0; }
float ShipDeathToWreckSeconds() { return 0; }

} // namespace KT1::Damage
