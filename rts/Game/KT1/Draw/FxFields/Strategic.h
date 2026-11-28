/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxFields: the look rules of strategic missiles, interceptors and long-range shells (replaces
// LuaUI/fx/strategic.lua). The widgets that draw and warn (gfx_strategic, gfx_tactical: step D17) stay Lua until then and
// keep their own copy; their native ports call these. Fed by the Strategic* events. Times are seconds here (the Lua
// counted frames). Engine-free.

#ifndef KT1_FIELD_STRATEGIC_H
#define KT1_FIELD_STRATEGIC_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxEvent.h"

namespace KT1::FxFields {

struct StrategicLookConfig {          // "strategic"
	std::string warn_own = "Strategic missile launched";          // DESIGN
	std::string warn_enemy = "Nuclear launch detected";           // DESIGN
	std::string warn_spectator = "Nuclear launch detected: %s";   // DESIGN
	std::string warn_intercepted = "Strategic missile intercepted";   // DESIGN
	std::string alert_kind = "nuclear_launch";                    // DESIGN: the sound cue alert kinds
	std::string alert_kind_own = "strategic_launch";              // DESIGN
	std::string alert_kind_intercept = "nuclear_intercept";       // DESIGN
	float ping_life = 10.0f;          // DESIGN: MISSILE_DEFENCE brief 2026-11-10 s5
	float ping_merge_dist = 80.0f;    // DESIGN: elmos (10 map units)
	float ping_merge_s = 8.0f;        // DESIGN
	float edge_life = 4.0f;           // DESIGN
	float climb_s = 2.0f;             // DESIGN: straight up before the arc
	float climb_height = 700.0f;      // DESIGN
	float apex_factor = 0.22f;        // DESIGN
	float apex_max = 2400.0f;         // DESIGN
	float rise_end = 0.47f;           // DESIGN: shares of the flight
	float turn_end = 0.60f;           // DESIGN
	float cruise_end = 0.72f;         // DESIGN
	float rise_top = 0.9f;            // DESIGN: share of the cruise height the climb reaches
	float cruise_factor = 1.4f;       // DESIGN
	float cruise_min = 600.0f;        // DESIGN
	float cruise_max = 2400.0f;       // DESIGN
	float turn_share = 0.12f;         // DESIGN
	float dive_share = 0.10f;         // DESIGN
	float trail_stages[3] = {0.0f, 2.6f, 5.2f};   // DESIGN: seconds after liftoff
	float intercept_lead_s = 1.5f;    // DESIGN
	float interceptor_climb = 220.0f; // DESIGN
	float interceptor_climb_s = 0.667f;   // DESIGN
	float min_liftoff_s = 0.5f;       // DESIGN
	bool prep_glow = true;            // DESIGN
	float trail_life = 3.0f;          // DESIGN
	float flash_time = 0.9f;          // DESIGN
	float flash_radius = 1400.0f;     // DESIGN
	float ring_time = 2.6f;           // DESIGN
	float ring_radius = 1600.0f;      // DESIGN: a silo's outer ring wins
	float cloud_time = 9.0f;          // DESIGN
	float cloud_height = 900.0f;      // DESIGN
	float cloud_width = 420.0f;       // DESIGN
	float crater_life = 150.0f;       // DESIGN
	float crater_fade = 20.0f;        // DESIGN
	float crater_radius_share = 0.45f;    // DESIGN
	float shake_time = 1.6f;          // DESIGN
	float shake_amp = 28.0f;          // DESIGN
	float shake_reach = 4500.0f;      // DESIGN
	float burst_time = 1.2f;          // DESIGN
	float burst_radius = 320.0f;      // DESIGN
	float shell_min_range = 1200.0f;  // DESIGN
	int shell_min_tier = 3;           // DESIGN
	float shell_impact_time = 1.1f;   // DESIGN
	float shell_ring_radius = 360.0f; // DESIGN
	float shell_crater_life = 60.0f;  // DESIGN
	float shell_crater_fade = 10.0f;  // DESIGN
	float shell_shake_amp = 6.0f;     // DESIGN
	float shell_shake_reach = 1600.0f;    // DESIGN
	float landing_max_s = 30.0f;      // KT1 lint: allow frame-literal DESIGN: seconds a shell's landing is looked ahead
	template<class S> void Describe(S& s) {
		s.Field("warn_own", warn_own);
		s.Field("warn_enemy", warn_enemy);
		s.Field("warn_spectator", warn_spectator);
		s.Field("warn_intercepted", warn_intercepted);
		s.Field("alert_kind", alert_kind);
		s.Field("alert_kind_own", alert_kind_own);
		s.Field("alert_kind_intercept", alert_kind_intercept);
		s.Field("ping_life", ping_life);
		s.Field("ping_merge_dist", ping_merge_dist);
		s.Field("ping_merge_s", ping_merge_s);
		s.Field("edge_life", edge_life);
		s.Field("climb_s", climb_s);
		s.Field("climb_height", climb_height);
		s.Field("apex_factor", apex_factor);
		s.Field("apex_max", apex_max);
		s.Field("rise_end", rise_end);
		s.Field("turn_end", turn_end);
		s.Field("cruise_end", cruise_end);
		s.Field("rise_top", rise_top);
		s.Field("cruise_factor", cruise_factor);
		s.Field("cruise_min", cruise_min);
		s.Field("cruise_max", cruise_max);
		s.Field("turn_share", turn_share);
		s.Field("dive_share", dive_share);
		s.Field("trail_stages", trail_stages);
		s.Field("intercept_lead_s", intercept_lead_s);
		s.Field("interceptor_climb", interceptor_climb);
		s.Field("interceptor_climb_s", interceptor_climb_s);
		s.Field("min_liftoff_s", min_liftoff_s);
		s.Field("prep_glow", prep_glow);
		s.Field("trail_life", trail_life);
		s.Field("flash_time", flash_time);
		s.Field("flash_radius", flash_radius);
		s.Field("ring_time", ring_time);
		s.Field("ring_radius", ring_radius);
		s.Field("cloud_time", cloud_time);
		s.Field("cloud_height", cloud_height);
		s.Field("cloud_width", cloud_width);
		s.Field("crater_life", crater_life);
		s.Field("crater_fade", crater_fade);
		s.Field("crater_radius_share", crater_radius_share);
		s.Field("shake_time", shake_time);
		s.Field("shake_amp", shake_amp);
		s.Field("shake_reach", shake_reach);
		s.Field("burst_time", burst_time);
		s.Field("burst_radius", burst_radius);
		s.Field("shell_min_range", shell_min_range);
		s.Field("shell_min_tier", shell_min_tier);
		s.Field("shell_impact_time", shell_impact_time);
		s.Field("shell_ring_radius", shell_ring_radius);
		s.Field("shell_crater_life", shell_crater_life);
		s.Field("shell_crater_fade", shell_crater_fade);
		s.Field("shell_shake_amp", shell_shake_amp);
		s.Field("shell_shake_reach", shell_shake_reach);
		s.Field("landing_max_s", landing_max_s);
	}
};

enum class StrategicEffect : uint8_t { Warning, Marker, Missile, Prep, Launch, Trail, Flash, Ring, Cloud, Crater, Shake, Burst };
enum class Viewer : uint8_t { Own, Enemy, Spectator };

struct EffectSwitches { bool particles = true, lights = true, decals = true, shake = true, reduceMotion = false; };

struct MapPing { float x = 0.0f, z = 0.0f; const char* label = ""; };
struct Warning {
	bool on = false;                  // false: none (a tactical missile has no global warning)
	Viewer relation = Viewer::Enemy;
	std::string text, sound;
	float at = 0.0f;                  // seconds
	MapPing pings[2];
	int32_t pingCount = 0;
	bool edge = false;                // flash the minimap border: no position to show
};
struct RecentPing { float x = 0.0f, z = 0.0f, time = 0.0f; int32_t team = 0; };

struct FlightPlan {                   // seconds
	float order = 0.0f, liftoff = 0.0f, hit = 0.0f, ends = 0.0f;
	bool intercepted = false;
	float intercept = 0.0f, interceptorStart = 0.0f;
};
enum class FlightPhase : uint8_t { None, Climb, Arc };
struct FlightPoint { FlightPhase phase = FlightPhase::None; Core::Fx::Vec3 pos; float share = 0.0f; };

struct ImpactLook {
	float flash = 0.0f, flashRadius = 0.0f, ring = 0.0f, ringRadius = 0.0f, cloud = 0.0f, cloudHeight = 0.0f, capWidth = 0.0f,
		stemWidth = 0.0f, crater = 0.0f, craterRadius = 0.0f;
	bool done = false;
};

struct GroundAt { void* ctx = nullptr; float (*height)(void* ctx, float x, float z) = nullptr; };

struct ShellInfo { int override_ = -1; bool cannon = false; float range = 0.0f; int tier = 0; };   // override_: -1 none, 0, 1

bool Allowed(StrategicEffect effect, const EffectSwitches& switches);
Viewer ViewerOf(int32_t allyTeam, int32_t viewerAllyTeam, bool spectatorFullView);
Warning LaunchWarning(const StrategicLookConfig& c, const Core::Fx::StrategicLaunch& e, Viewer viewer,
	const char* teamName, float stepSeconds);
Warning InterceptWarning(const StrategicLookConfig& c, const Core::Fx::StrategicLaunch& e, Viewer viewer,
	bool involvedAlly, bool seesIntercept, float x, float z, float stepSeconds);
bool MergePing(const StrategicLookConfig& c, std::vector<RecentPing>& recent, float x, float z, int32_t team, float now);
FlightPlan PlanFlight(const StrategicLookConfig& c, const Core::Fx::StrategicLaunch& e, float stepSeconds);
FlightPoint MissileAt(const StrategicLookConfig& c, const Core::Fx::StrategicLaunch& e, const FlightPlan& plan, float now,
	const GroundAt& ground);
int32_t TrailStage(const StrategicLookConfig& c, const FlightPlan& plan, float now);
void SimMissileAt(const Core::Fx::StrategicLaunch& e, float now, float stepSeconds, float& x, float& z);
// where a real interceptor is drawn: from its simulated place toward the drawn missile as it closes
Core::Fx::Vec3 InterceptorDrawn(const StrategicLookConfig& c, const Core::Fx::StrategicLaunch& e, const FlightPlan& plan,
	float ix, float iz, float startDistance, float age, float now, float stepSeconds, const GroundAt& ground);
FlightPoint InterceptorAt(const FlightPlan& plan, const Core::Fx::Vec3& from, const Core::Fx::Vec3& meet, float now);
ImpactLook ImpactAt(const StrategicLookConfig& c, float age, float ringRadius);       // ringRadius 0: the default
ImpactLook ShellImpactAt(const StrategicLookConfig& c, float age);
float ShakeAt(float distance, float age, float amplitude, float reach, float time);
bool IsStrategicShell(const StrategicLookConfig& c, const ShellInfo& info);
// where a shell comes down: velocity per step, gravity per step squared; false: not within landing_max_s
bool Landing(const StrategicLookConfig& c, const Core::Fx::Vec3& pos, const Core::Fx::Vec3& vel, float gravity,
	float stepSeconds, const GroundAt& ground, Core::Fx::Vec3& at, int32_t& steps);

} // namespace KT1::FxFields

#endif
