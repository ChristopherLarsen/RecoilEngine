/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include "HoverAirMoveType.h"

struct UnitDef;

/**
 * @brief Aircraft movement with continuous-time rules.
 *
 * Selected per unit with the unit-definition custom parameter `supcom_air = "1"`, for winged and hovering
 * aircraft alike (`supcom_air_winged`). The landed state, take-off triggers, collisions and command hooks are
 * the base class's; this class replaces the flight itself:
 *
 *  - hovering aircraft close in on their top speed exponentially (`supcom_air_speed_response`, per second) and
 *    brake towards a goal the same way, never above the top speed;
 *  - winged aircraft fly forwards all the time, their speed rising monotonically to the top speed and reaching
 *    90 % of it `supcom_air_rise_90` seconds after take-off; they circle an idle goal;
 *  - the height moves towards the higher of the terrain and the water surface plus the cruise height
 *    (`cruiseAltitude`) at no more than `supcom_air_climb_rate` (elmos per second), never below that reference;
 *  - an aircraft set crashing falls ballistically (`supcom_air_fall_gravity`, elmos per second squared) with the
 *    horizontal velocity it had, tumbling at `supcom_air_tumble_rate` (degrees per second), and is destroyed
 *    when it reaches the ground;
 *  - an idle aircraft that may land waits `supcom_air_idle_land_delay` seconds, then lowers exponentially
 *    (`supcom_air_land_decay`, per second), optionally holding `supcom_air_land_hold_height` (elmos) for
 *    `supcom_air_land_hold_time` seconds, and lands under 0.1 map units above the ground;
 *  - attacking, a winged aircraft re-aims at its target's current position every frame and makes passes: it
 *    overshoots, runs out to `supcom_air_pass_swing` (elmos) and turns back; it banks with its turn rate, up to
 *    `supcom_air_max_bank` degrees (presentation);
 *  - a transport under a load or unload order lowers at no more than `supcom_air_transport_descent` (elmos per
 *    second) and loads and unloads holding `supcom_air_load_height` (elmos) above the ground (TransportHeight).
 *
 * Speeds and heights are integrated exactly over each simulation frame.
 */
class CSupComAirMoveType : public CHoverAirMoveType
{
	CR_DECLARE_DERIVED(CSupComAirMoveType)

public:
	CSupComAirMoveType(CUnit* owner);

	static bool IsWantedBy(const UnitDef* ud);

	bool Update() override;
	void StartMoving(float3 pos, float goalRadius) override;
	void StartMoving(float3 pos, float goalRadius, float speed) override { StartMoving(pos, goalRadius); }
	void StopMoving(bool callScript = false, bool hardStop = false, bool cancelRaw = false) override;
	void KeepPointingTo(float3 pos, float distance, bool aggressive) override;

	/// the height a transport loads and unloads at over pos: the reference plus the load height, or the
	/// command AI's own height when no load height is set
	float TransportHeight(const float3& pos, float engineHeight) const;
	/// true when a load height is set: the transport docks only once it holds that height
	bool HoldsLoadHeight() const { return (loadHeight > 0.0f); }

private:
	enum IdleStage {
		IDLE_NONE    = 0, // under an order
		IDLE_WAIT    = 1, // arrived, waiting for the landing delay
		IDLE_LOWER   = 2, // lowering exponentially
		IDLE_HOLD    = 3, // holding the hold height
		IDLE_LOWER2  = 4, // lowering again after the hold
	};

	void UpdateIdle();
	void UpdateFlight();
	void UpdateFall();
	/// height above the ground during the idle lowering (elmos)
	float UpdateIdleHeight();
	float ReferenceHeight(float x, float z) const;
	void ApplyStep(const float3& step);
	void UpdateBank(short oldHeading);
	const CUnit* AttackTarget() const;
	bool InTransportOrder() const;

	/// a horizontal step rounded towards zero onto the float grid around position p
	static float GridStepDown(float p, float d);

private:
	// per-unit values, engine units (elmos, frames)
	bool winged = false;
	float speedResponse = 0.0f;  ///< per frame (hovering)
	float riseRate = 0.0f;       ///< per frame (winged: ln 10 / rise_90)
	float climbRate = 0.0f;      ///< elmos per frame
	float fallGravity = 0.0f;    ///< elmos per frame squared
	float tumbleRate = 0.0f;     ///< heading units per frame
	float idleLandDelay = -1.0f; ///< frames; < 0 never lands when idle
	float landDecay = 0.0f;      ///< per frame
	float landHoldHeight = 0.0f; ///< elmos; 0 no hold
	float landHoldFrames = 0.0f;
	float cruiseHeight = 0.0f;   ///< elmos above the reference
	float passSwing = 0.0f;      ///< elmos; 0 = plain steering (winged)
	float maxBank = 0.0f;        ///< radians (winged, presentation)
	float transportDescent = 0.0f; ///< elmos per frame; 0 = the climb rate
	float loadHeight = 0.0f;     ///< elmos above the reference while loading and unloading; 0 = the command AI's

	// state
	float horSpeed = 0.0f;       ///< horizontal speed (elmos per frame)
	int idleStage = IDLE_NONE;
	int idleFrame = 0;           ///< frame the current idle stage started
	float idleHeight = 0.0f;     ///< height above the ground when the current lowering stage started
	bool falling = false;
	bool passOut = false;        ///< past the target, running out before turning back
	float lastAimDist = 0.0f;
	float3 fallVelocity;         ///< elmos per frame
};
