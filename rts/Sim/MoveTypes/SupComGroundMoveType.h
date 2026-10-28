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
 *  - under water the top speed follows the move class's depthModParams, as the engine's own ground movement;
 *    on the sea floor (amphibious) or on water (hover) it is also scaled by supcom_water_speed_factor (MOV-009 R7);
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
 *    upright and sink_lower units are lowered by upright_sink times the spread;
 *  - an idle allied unit of this move type that a moving one touches in front makes way: it moves to the
 *    nearest free spot off the mover's route (PTH-002 R3); a unit holding position does not;
 *  - ships (Ship move class) ride on the water surface; submarines (`supcom_submarine`) cruise
 *    `supcom_dive_depth` below it and dive or surface over `supcom_dive_time` along a smooth curve,
 *    ordered from Lua with MoveCtrl.SetGroundMoveTypeData(unitID, "submerged", bool); in water shallower than the
 *    dive depth a dive stops supcom_floor_clearance above the floor (MOV-011 R7).
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
	bool Update() override;

	/// handles "submerged" (bool: dive or surface a submarine) and "supcomHeadingFree" (bool, hover units: follow the
	/// route without turning, Lua sets the heading; MOV-005 R7), everything else as the base class
	bool SetMemberValue(unsigned int memberHash, void* memberValue) override;

	/// movement layer (MOV-009): "land", "seabed", "water" or "sub"
	const char* GetLayerName() const;
	/// the direction a step is taken in: the unit's front, or for a heading-free hover unit its waypoint direction
	float3 MotionDir() const;
	/// amphibious unit on the sea floor (MOV-009 R8): at the water line, or with a switch depth the kept layer
	bool OnSeabed() const;
	float GetContactLift() const override { return contactLift; }
	bool HeadingLocked() const override;
	bool GetHeadingFree() const { return headingFree; }
	/// the footprint this move type drives with (elmos)
	float GetFootprintX() const { return footprintX; }
	float GetFootprintZ() const { return footprintZ; }
	/// diagnosis (Lua GetUnitMoveTypeData): speed (elmos per frame), phase, braking, frames blocked
	float GetStepSpeed() const { return scSpeed; }
	int GetStepPhase() const { return scPhase; }
	bool GetStepBraking() const { return scBraking; }
	int GetBlockedFrames() const { return scBlockedFrames; }
	/// a submarine's current depth below the water surface (elmos)
	float GetDepth() const { return CurrentDepth(); }
	bool IsSubmarine() const { return submarine; }
	bool IsSubmergedOrdered() const { return subOrdered; }
	bool IsDiving() const;

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
	/// the waypoint, or the nearest point where the footprint fits; round a corner, a point to go to first
	float3 FitWaypoint(const float3& wp) const;
	/// whether the footprint fits all along the straight line from a to b
	bool LineFits(const float3& a, const float3& b) const;
	bool DirectLineClear();
	void SetWaypointDir(const float3& cwp, const float3& opos) override;
	void UpdateTurnRate();

	/// PTH-002 R3 (mover side): an idle allied unit in contact in front for a few frames is asked to make way
	void CheckYield();
	/// PTH-002 R3 (idle side): move to the nearest free spot off the mover's route; false if there is none
	bool MakeWay(const CSupComGroundMoveType* mover);
	/// whether an idle unit of this move type may be asked to make way
	bool CanYield() const;
	float HalfSize() const { return 0.5f * std::max(footprintX, footprintZ); }

	/// a step rounded to the float grid around position p
	static float GridStep(float p, float d);

	void StartDive(bool submerge);
	float CurrentDepth() const;

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
	float hoverHeight = 0.0f;    ///< elmos above the corner references (hover units, MOV-010)
	float waterSpeedFactor = 1.0f; ///< top speed multiplier on the sea floor or on water (MOV-009 R7)
	float floorClearance = 0.0f; ///< elmos a submarine stays above the floor (MOV-011 R7)
	float seabedSwitchDepth = 0.0f; ///< elmos of water under the unit at which Land becomes Seabed (MOV-009 R8; 0 = water line)
	bool onSeabed = false;       ///< the Seabed layer, with seabedSwitchDepth > 0 (kept between frames for the hysteresis)

	// state
	float scSpeed = 0.0f;        ///< instantaneous speed along the motion direction (elmos per frame, >= 0)
	int scPhase = PHASE_REST;
	int scDir = 1;               ///< +1 forwards, -1 backwards
	bool scBraking = false;      ///< braking committed for the current order
	bool scStopNow = false;      ///< stop order: brake at once
	bool scRestart = false;      ///< the last step was blocked
	float contactLift = 0.0f;    ///< elmos above the surface under the centre, set by AdjustPosToWaterLine
	int scBlockedFrames = 0;     ///< consecutive frames with the wanted step refused
	int scStructFrames = 0;      ///< consecutive frames with the wanted step refused by a structure (Q100)
	int scHandoverFrames = 0;    ///< frames left of the engine's own ground movement after such a block
	float3 scHandoverFrom;       ///< where that handover began
	int scYieldFrames = 0;       ///< consecutive frames in contact with the same idle unit in front
	int scYieldUnit = -1;        ///< that unit's id
	bool scYielding = false;     ///< moving out of the way (no order of its own)
	bool headingFree = false;    ///< hover unit following its route without turning (MOV-005 R7, set from Lua)

	// submarine (MOV-011): depth below the water surface, moving between depthFrom and depthTo along a smooth curve
	bool submarine = false;
	bool subOrdered = false;     ///< the last dive order: true = submerged, false = surfaced
	float diveDepth = 0.0f;      ///< elmos below the surface when submerged
	float diveFrames = 0.0f;     ///< frames for a full dive or surfacing
	float depthFrom = 0.0f;
	float depthTo = 0.0f;
	float depthFrames = 0.0f;    ///< frames for the current transition
	int depthStartFrame = 0;

	// direct steering cache
	float3 directGoal;
	int directCheckFrame = 0;
	bool directClear = false;
};
