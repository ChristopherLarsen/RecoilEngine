/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include "GroundMoveType.h"

/**
 * @brief Ground movement with continuous-time speed rules.
 *
 * A variant of CGroundMoveType selected per unit with the unit-definition custom
 * parameter `supcom_move = "1"`. Path following, steering, collisions and the
 * order handling are the base class's; this class replaces the speed integration,
 * the arrival, blocked steps and the vertical placement:
 *
 *  - speed rises at the unit's acceleration up to its top speed, horizontally;
 *  - braking starts where constant braking plus the settle brings the unit to
 *    rest at its stopping point, and is not undone for the same order;
 *  - below brake / settle_rate the speed decays exponentially, and the unit is at
 *    rest (and the order complete) once it falls under stop_speed;
 *  - the turn rate limit grows with speed when a turn radius is given;
 *  - a step whose footprint would leave the map or enter impassable terrain is
 *    not taken, the speed drops to zero, and the next step restarts at
 *    blocked_restart times the top speed;
 *  - the unit sits at the mean terrain height under its footprint corners and,
 *    unless upright, tilts with the plane spanned by the corner diagonals;
 *    upright and sink_lower units are lowered by upright_sink times the spread.
 *
 * All rates are continuous (per second) and are integrated exactly over each
 * simulation frame, including phase changes inside a frame.
 */
class CSupComGroundMoveType : public CGroundMoveType
{
	CR_DECLARE_DERIVED(CSupComGroundMoveType)

public:
	CSupComGroundMoveType(CUnit* owner);

	static bool IsWantedBy(const UnitDef* ud);

	void StartMovingRaw(const float3 moveGoalPos, float moveGoalRadius) override;
	void StartMoving(float3 pos, float moveGoalRadius) override;
	void StartMoving(float3 pos, float moveGoalRadius, float speed) override { StartMoving(pos, moveGoalRadius); }
	void StopMoving(bool callScript = false, bool hardStop = false, bool cancelRaw = false) override;

	void UpdateUnitPosition() override;

protected:
	bool ArrivalAllowed() const override;
	void AdjustPosToWaterLine() override;

private:
	enum Phase {
		PHASE_DRIVE  = 0, // accelerating or at top speed
		PHASE_BRAKE  = 1, // constant braking
		PHASE_SETTLE = 2, // exponential settle
		PHASE_REST   = 3,
	};

	void ResetForNewOrder();

	/// distance needed to come to rest from speed v (elmos, v in elmos per frame)
	float StoppingDistance(float v) const;
	/// advances the speed state over one frame; returns the distance travelled
	float Advance(float topSpeed, float stopDist, bool stop);
	float AdvanceBraking(float dt, float& dist);
	void AdvanceDrive(float dt, float topSpeed, float& v, float& dist) const;

	bool FootprintBlocked(const float3& pos) const;
	bool DirectLineClear();
	void SetWaypointDir(const float3& cwp, const float3& opos) override;
	void UpdateTurnRate();

	/// a step rounded to the float grid around position p
	static float GridStep(float p, float d);

private:
	// per-unit values, engine units (elmos, frames, heading units)
	float baseTurnRate = 0.0f;   ///< turn rate limit at low speed (heading units per frame)
	float turnAccelFactor = 0.333f; ///< turn acceleration per unit of turn rate (the base class's ratio)
	float turnRadius = 0.0f;     ///< elmos; 0 means none
	float settleRate = 1.0f;     ///< per frame
	float stopSpeed = 0.0f;      ///< elmos per frame
	float blockedRestart = 0.0f; ///< fraction of the top speed
	float uprightSink = 0.0f;
	float footprintX = 0.0f;     ///< elmos, along the unit's right axis
	float footprintZ = 0.0f;     ///< elmos, along the unit's forward axis
	bool sinkLower = false;

	// state
	float scSpeed = 0.0f;        ///< instantaneous speed along the motion direction (elmos per frame, >= 0)
	int scPhase = PHASE_REST;
	int scDir = 1;               ///< +1 forwards, -1 backwards
	bool scBraking = false;      ///< braking committed for the current order
	bool scStopNow = false;      ///< stop order: brake at once
	bool scRestart = false;      ///< the last step was blocked

	// direct steering cache
	float3 directGoal;
	int directCheckFrame = 0;
	bool directClear = false;
};
