/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS family: the family's spine, one plain struct per display event (game-build
// docs/engine/families/EFFECTS.md 4.1). They replace the SendToUnsynced messages of the family's gadgets and the messages
// other gadgets send to them; the name in brackets is the Lua message. No engine include.
//
// Send: DisplayEvents::Send(event) from the sim (Rules/FxEvents for the family, any family for the ones it owns the
// happening of); receive: DisplaySink::Listen<T>(fn) (Draw/Common/DisplayListen.h), draw or audio side.
// Positions are elmos. `seen` is who had the event's point in line of sight when it was raised (FxTypes.h Sees).
// Effect spawns themselves (the engine's explosion generators) are played by DisplayEvents::Effect; BuildSet and DeathSet
// say in addition what was spawned.
// The damage-over-time tick ("dot_tick") and the charge messages are COMBAT's events and the order mark ("order_ack")
// is ORDERS'.

#ifndef KT1_FX_EVENT_H
#define KT1_FX_EVENT_H

#include <cstdint>

#include "Game/KT1/Core/Common/DisplayTypes.h"
#include "Game/KT1/Core/FxEvents/FxTypes.h"
#include "Game/KT1/Core/SimHost/DisplayQueue.h"

namespace KT1::Core::Fx {

constexpr int32_t EVENT_BASE = DisplayRange::EFFECTS;

// ---------------------------------------------------------------- raised by the family's synced system

struct BuildSet {                       // one on-target build set made at a unit under construction (fx_build_sets)
	static constexpr int32_t TYPE = EVENT_BASE + 0;
	int32_t unit = -1, defId = -1, slot = 0, effect = NO_EFFECT;
	Vec3 pos;
};

struct DeathSet {                       // one death or damage-state set made (fx_death_sets)
	static constexpr int32_t TYPE = EVENT_BASE + 1;
	int32_t unit = -1, defId = -1, effect = NO_EFFECT;
	Vec3 pos;
	Priority priority = Priority::Death;
};

struct WreckImpulse {                   // an explosion pushed a wreck (fx_wreck_physics)
	static constexpr int32_t TYPE = EVENT_BASE + 2;
	int32_t feature = -1, weaponDef = -1;
	Vec3 pos;
	float vx = 0.0f, vz = 0.0f, up = 0.0f, spin = 0.0f;
};

struct WreckThud {                      // a wreck landed or bounced, rate limited (the sound cue "wreck_thud")
	static constexpr int32_t TYPE = EVENT_BASE + 3;
	int32_t feature = -1;
	Vec3 pos;
	float speed = 0.0f;                 // downward, elmos per step
};

struct ScriptBlast {                    // [fx_script_blast] an impact whose look has a script tree
	static constexpr int32_t TYPE = EVENT_BASE + 4;
	ScriptTree tree = ScriptTree::None;
	int32_t weaponDef = -1;
	Vec3 pos;
};

struct DepthHit {                       // [fx_depth_hit] a depth-class explosion
	static constexpr int32_t TYPE = EVENT_BASE + 5;
	Vec3 pos;
	int32_t tier = 1;                   // 1 s .. 4 xl
	SeenMask seen = 0;
};

struct DepthIntercept {                 // [fx_depth_intercept] a charge or torpedo met by a counter-shot
	static constexpr int32_t TYPE = EVENT_BASE + 6;
	Vec3 pos, from;
	uint8_t hasFrom = 0;
	SeenMask seen = 0;
};

struct DepthLaunch {                    // [fx_depth_launch] a depth-class projectile created
	static constexpr int32_t TYPE = EVENT_BASE + 7;
	int32_t projectile = -1, weaponDef = -1;
	Vec3 pos;
	SeenMask seen = 0;
};

enum class ScorchSource : uint8_t {
	Explosion,      // [scorch] an explosion: size and life from the weapon's tier (gfx_scorch.lua)
	Parity,         // [fx_parity_scorch] a look's own mark: size and life are given
	Death           // [scorch] a scripted mark: a crash, the ring round an experimental's death
};

struct Scorch {                         // COMBAT raises this one too (death_blast's crash mark); decals take every one,
                                        // as the Lua's handler did (seen is for other readers and may be left 0)
	static constexpr int32_t TYPE = EVENT_BASE + 8;
	int32_t weaponDef = -1;
	Vec3 pos;
	float size = 0.0f;                  // diameter, elmos; Explosion and Death: 0 (from the weapon)
	float lifeS = 0.0f;                 // seconds; 0: the reader's default
	ScorchSource source = ScorchSource::Explosion;
	SeenMask seen = 0;
};

struct Scale {                          // [fx_scale] the shared effect budget's option, once a second
	static constexpr int32_t TYPE = EVENT_BASE + 9;
	float scale = 1.0f;
};

struct ShotStart {                      // a followed projectile was created
	static constexpr int32_t TYPE = EVENT_BASE + 10;
	int32_t projectile = -1, weaponDef = -1, owner = -1;
	Vec3 pos, vel;                      // vel: elmos per step
	SeenMask seen = 0;
};

struct ShotEnd {                        // a followed projectile is gone from the map
	static constexpr int32_t TYPE = EVENT_BASE + 11;
	int32_t projectile = -1, weaponDef = -1, owner = -1;
	Vec3 pos, vel;                      // as last known
	SeenMask seen = 0;
};

struct Impact {                         // an explosion a draw system watches (ground shade, lights)
	static constexpr int32_t TYPE = EVENT_BASE + 12;
	int32_t weaponDef = -1, owner = -1;
	Vec3 pos;
	SeenMask seen = 0;
};

struct Death {                          // a unit was destroyed: what the draw side needs after it is gone
	static constexpr int32_t TYPE = EVENT_BASE + 13;
	int32_t unit = -1, defId = -1, team = -1, allyTeam = -1;
	Vec3 pos;
	float radius = 0.0f, height = 0.0f;
	uint8_t finished = 1;               // 0: an unfinished site was removed
	SeenMask seen = 0;
};

struct Blast {                          // [cfx_blast] a classified explosion (death_blast raises it too)
	static constexpr int32_t TYPE = EVENT_BASE + 14;
	BlastClass cls = BlastClass::None;
	BlastFamily family = BlastFamily::None;
	Faction faction = Faction::Human;
	int32_t weaponDef = -1;
	Vec3 pos;
	SeenMask seen = 0;
};

struct CritDeath {                      // [crit_death] a finished unit died (the criteria effects)
	static constexpr int32_t TYPE = EVENT_BASE + 15;
	int32_t defId = -1, unit = -1, team = 0;
	Vec3 pos;
	DeathMedium medium = DeathMedium::None;
	int32_t heading = 0;
	SeenMask seen = 0;
};

struct FogDeath {                       // [vis_fog_puff] a finished unit died (drawn only for a viewer without sight)
	static constexpr int32_t TYPE = EVENT_BASE + 16;
	Vec3 pos;
	float radius = 0.0f, height = 0.0f;
	int32_t allyTeam = -1;
	SeenMask seen = 0;
};

// ---------------------------------------------------------------- abilities (relayed from the ability rules)

struct Teleport {                       // [fxev_tele]
	static constexpr int32_t TYPE = EVENT_BASE + 17;
	TeleportKind kind = TeleportKind::Start;
	int32_t unit = -1, allyTeam = -1;
	Vec3 from, to;                      // y: ground height
	float value = 0.0f;                 // Start: charge seconds; Jump: arrival reach, elmos
	SeenMask seenFrom = 0, seenTo = 0;
};

struct Crash {                          // [fxev_crash] an aircraft killed in flight starts to fall
	static constexpr int32_t TYPE = EVENT_BASE + 18;
	int32_t unit = -1, defId = -1;
};

struct Remnant {                        // [fxev_rem] a death remnant appears
	static constexpr int32_t TYPE = EVENT_BASE + 19;
	int32_t unit = -1;
	Vec3 pos;
	SeenMask seen = 0;
};

struct Sacrifice {                      // [fxev_sac]
	static constexpr int32_t TYPE = EVENT_BASE + 20;
	int32_t engineer = -1, site = -1;
	Vec3 pos, sitePos;                  // the engineer's, the site's
	float before = 0.0f, after = 0.0f;  // the site's build progress
	float siteRadius = 0.0f, siteHeight = 0.0f;
	SeenMask seen = 0;
};

// ---------------------------------------------------------------- raised by the shield rule (PROJ), which sends these two
// Charging weapons: COMBAT's Core::TimedRules::ChargeProgress and ChargeFired (Core/Damage/CombatEvents.h); the barrel
// tip they are drawn at is FxSamples.h MuzzleSample.

enum class ShieldKind : uint8_t { Bubble, Personal, Transport, AntiArtillery };
enum class ShieldWhy : uint8_t { Up, Off, Collapsed, NoEnergy, Gone, Other };

struct ShieldState {                    // [shield_state]
	static constexpr int32_t TYPE = EVENT_BASE + 21;
	int32_t unit = -1;
	int8_t up = 0;                      // 1 up, 0 down, -1 gone
	float fraction = 0.0f;              // health share
	ShieldKind kind = ShieldKind::Bubble;
	float radius = 0.0f, height = 0.0f;
	ShieldWhy why = ShieldWhy::Up;
	Faction faction = Faction::Human;   // the unit rules param fx_faction, else the def's
};

struct ShieldHit {                      // [shield_hit]
	static constexpr int32_t TYPE = EVENT_BASE + 22;
	int32_t unit = -1, weaponDef = -1;
	Vec3 pos;
	float amount = 0.0f;
};

struct CommanderBlast {                 // [cmd_blast, cmd_noblast] a command unit's death
	static constexpr int32_t TYPE = EVENT_BASE + 23;
	int32_t unit = -1, defId = -1, team = -1, allyTeam = -1;
	Vec3 pos;
	int32_t killStep = 0;
	uint8_t noBlast = 0;                // 1: removed by a script without dying
};

// ---------------------------------------------------------------- strategic and tactical missiles (fx_strategic)
// Coordinates are ground coordinates; steps are sim step numbers; ids are the relay's, not unit ids.

struct StrategicLaunch {                // [strat_launch]
	static constexpr int32_t TYPE = EVENT_BASE + 24;
	int32_t id = 0, team = 0, allyTeam = 0, owner = 0;
	float sx = 0.0f, sz = 0.0f, x = 0.0f, z = 0.0f;
	int32_t orderStep = 0, launchStep = 0, hitStep = 0, interceptStep = 0;
	int32_t defenceUnit = 0;
	float dx = 0.0f, dz = 0.0f;
	int32_t defenceAlly = -1;
	float ringElmos = 0.0f;
};

struct StrategicEnd {                   // [strat_end]
	static constexpr int32_t TYPE = EVENT_BASE + 25;
	int32_t id = 0;
	uint8_t result = 1;                 // 1 hit, 2 intercepted
	int32_t step = 0;
	float x = 0.0f, z = 0.0f;
	float ringElmos = 0.0f;
	int32_t bands = 1;                  // the outer zone's steps
	float spreadS = 0.0f;               // 0: the zone comes at once
};

struct StrategicTactical {              // [strat_tac] a tactical missile's hit
	static constexpr int32_t TYPE = EVENT_BASE + 26;
	float x = 0.0f, z = 0.0f;
};

struct StrategicInterceptor {           // [strat_icpt]
	static constexpr int32_t TYPE = EVENT_BASE + 27;
	int32_t id = 0, missile = 0, defenceAlly = -1;
	float sx = 0.0f, sz = 0.0f;
	int32_t launchStep = 0;
};

struct StrategicInterceptorPos {        // [strat_ipos]
	static constexpr int32_t TYPE = EVENT_BASE + 28;
	int32_t id = 0;
	float x = 0.0f, z = 0.0f;
	int32_t step = 0;
};

struct StrategicInterceptorEnd {        // [strat_iend]
	static constexpr int32_t TYPE = EVENT_BASE + 29;
	int32_t id = 0;
	uint8_t result = 1;                 // 1 hit, 2 its missile ended first
	float x = 0.0f, z = 0.0f;
	int32_t step = 0;
};

struct TacticalLaunch {                 // [tac_launch]
	static constexpr int32_t TYPE = EVENT_BASE + 30;   // KT1 lint: allow frame-literal the 31st event number, not a cadence
	int32_t id = 0, allyTeam = 0;
	float sx = 0.0f, sz = 0.0f, x = 0.0f, z = 0.0f;
	SeenMask seenLaunch = 0;
};

struct TacticalPos {                    // [tac_pos]
	static constexpr int32_t TYPE = EVENT_BASE + 31;
	int32_t id = 0, allyTeam = 0;
	float gx = 0.0f, gz = 0.0f;
	float progress = 0.0f, pathLength = 0.0f;
	uint8_t shot = 0;                   // 1: a defence shot is after it
	float shotX = 0.0f, shotZ = 0.0f, shotFromX = 0.0f, shotFromZ = 0.0f;
	SeenMask seenMissile = 0, seenShot = 0;
};

struct TacticalEnd {                    // [tac_end]
	static constexpr int32_t TYPE = EVENT_BASE + 32;
	int32_t id = 0;
	uint8_t result = 1;                 // 1 hit, 2 shot down
	float x = 0.0f, z = 0.0f, shotFromX = 0.0f, shotFromZ = 0.0f;
	SeenMask seen = 0;
};

struct MissileRedirected {              // [spc_redirect]
	static constexpr int32_t TYPE = EVENT_BASE + 33;
	int32_t id = 0;
	float redirectorX = 0.0f, redirectorZ = 0.0f, x = 0.0f, z = 0.0f;
	float fromX = 0.0f, fromZ = 0.0f, toX = 0.0f, toZ = 0.0f;
	SeenMask seenRedirector = 0, seenPoint = 0;
};

constexpr int32_t EVENT_TYPES = 34;
static_assert(EVENT_TYPES <= DisplayRange::SPAN, "EFFECTS events fit in the family's range");
static_assert(sizeof(BuildSet) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(DeathSet) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(WreckImpulse) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(WreckThud) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(ScriptBlast) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(DepthHit) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(DepthIntercept) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(DepthLaunch) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Scorch) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Scale) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(ShotStart) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(ShotEnd) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Impact) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Death) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Blast) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(CritDeath) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(FogDeath) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Teleport) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Crash) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Remnant) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(Sacrifice) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(ShieldState) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(ShieldHit) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(CommanderBlast) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(StrategicLaunch) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(StrategicEnd) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(StrategicTactical) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(StrategicInterceptor) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(StrategicInterceptorPos) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(StrategicInterceptorEnd) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(TacticalLaunch) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(TacticalPos) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(TacticalEnd) <= SimHost::DISPLAY_TEXT);
static_assert(sizeof(MissileRedirected) <= SimHost::DISPLAY_TEXT);

} // namespace KT1::Core::Fx

#endif
