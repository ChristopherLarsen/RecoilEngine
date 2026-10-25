/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComAirMoveType.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "Map/Ground.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "System/SpringMath.h"

#include "System/Misc/TracyDefs.h"

CR_BIND_DERIVED(CSupComAirMoveType, CHoverAirMoveType, (nullptr))
CR_REG_METADATA(CSupComAirMoveType, (
	CR_MEMBER(winged),
	CR_MEMBER(speedResponse),
	CR_MEMBER(riseRate),
	CR_MEMBER(climbRate),
	CR_MEMBER(fallGravity),
	CR_MEMBER(tumbleRate),
	CR_MEMBER(idleLandDelay),
	CR_MEMBER(landDecay),
	CR_MEMBER(landHoldHeight),
	CR_MEMBER(landHoldFrames),
	CR_MEMBER(cruiseHeight),

	CR_MEMBER(horSpeed),
	CR_MEMBER(idleStage),
	CR_MEMBER(idleFrame),
	CR_MEMBER(idleHeight),
	CR_MEMBER(falling),
	CR_MEMBER(fallVelocity),

	CR_PREALLOC(GetPreallocContainer)
))

// an idle aircraft lands once it is lower than this above the ground (0.1 map units)
static constexpr float LAND_HEIGHT = 0.1f * SQUARE_SIZE;

static float CustomParam(const UnitDef* ud, const char* key, float def)
{
	const auto it = ud->customParams.find(key);

	if (it == ud->customParams.end() || it->second.empty())
		return def;

	return std::strtof(it->second.c_str(), nullptr);
}


bool CSupComAirMoveType::IsWantedBy(const UnitDef* ud)
{
	const auto it = ud->customParams.find("supcom_air");
	return (it != ud->customParams.end() && it->second == "1");
}

CSupComAirMoveType::CSupComAirMoveType(CUnit* owner): CHoverAirMoveType(owner)
{
	// creg
	if (owner == nullptr)
		return;

	const UnitDef* ud = owner->unitDef;

	winged = (CustomParam(ud, "supcom_air_winged", 0.0f) != 0.0f);

	// per second -> per frame
	speedResponse = std::max(0.0f, CustomParam(ud, "supcom_air_speed_response", 0.0f)) / GAME_SPEED;
	riseRate = math::log(10.0f) / std::max(1.0f / GAME_SPEED, CustomParam(ud, "supcom_air_rise_90", 1.0f)) / GAME_SPEED;
	climbRate = std::max(0.0f, CustomParam(ud, "supcom_air_climb_rate", 0.0f)) / GAME_SPEED;
	fallGravity = std::max(0.0f, CustomParam(ud, "supcom_air_fall_gravity", 0.0f)) / (GAME_SPEED * GAME_SPEED);
	tumbleRate = CustomParam(ud, "supcom_air_tumble_rate", 0.0f) / GAME_SPEED * SPRING_CIRCLE_DIVS / 360.0f;

	const float delay = CustomParam(ud, "supcom_air_idle_land_delay", -1.0f);
	idleLandDelay = (delay < 0.0f)? -1.0f: (delay * GAME_SPEED);
	landDecay = std::max(0.0f, CustomParam(ud, "supcom_air_land_decay", 0.0f)) / GAME_SPEED;
	landHoldHeight = std::max(0.0f, CustomParam(ud, "supcom_air_land_hold_height", 0.0f));
	landHoldFrames = std::max(0.0f, CustomParam(ud, "supcom_air_land_hold_time", 0.0f)) * GAME_SPEED;

	// the cruise height exactly as defined (the base class adds a random offset)
	cruiseHeight = ud->wantedHeight;
	wantedHeight = cruiseHeight;
	orgWantedHeight = cruiseHeight;
}


float CSupComAirMoveType::ReferenceHeight(float x, float z) const
{
	// the higher of the terrain and the water surface (height 0)
	return std::max(CGround::GetHeightReal(x, z), 0.0f);
}

float CSupComAirMoveType::GridStepDown(float p, float d)
{
	// the step to the representable position nearest p + d, shortened by one float step if rounding
	// lengthened it, so that a capped speed is never exceeded once applied (q - p is exact: q and p are close)
	float q = p + d;

	if (math::fabs(q - p) > math::fabs(d))
		q = std::nextafter(q, p);

	return (q - p);
}

void CSupComAirMoveType::ApplyStep(const float3& step)
{
	float3 s = step;
	s.x = GridStepDown(owner->pos.x, s.x);
	s.z = GridStepDown(owner->pos.z, s.z);

	owner->SetVelocityAndSpeed(s);
	owner->Move(s, true);
}


void CSupComAirMoveType::StartMoving(float3 pos, float goalRadius)
{
	CHoverAirMoveType::StartMoving(pos, goalRadius);

	// an order in the queue cancels waiting, lowering and holding (UpdateIdle); the command AI also re-issues
	// the finished goal once after its stop, which must not cancel the idle wait
	if (!owner->commandAI->commandQue.empty())
		idleStage = IDLE_NONE;

	wantedHeight = cruiseHeight;
}

void CSupComAirMoveType::StopMoving(bool callScript, bool hardStop, bool cancelRaw)
{
	CHoverAirMoveType::StopMoving(callScript, hardStop, cancelRaw);

	// the stop is handled here, not by the base class's stop state changes; arrival is the moment the
	// command queue empties (UpdateIdle)
	wantToStop = false;
}

void CSupComAirMoveType::UpdateIdle()
{
	if (aircraftState == AIRCRAFT_LANDED || aircraftState == AIRCRAFT_CRASHING)
		return;

	if (!owner->commandAI->commandQue.empty()) {
		// a new order cancels waiting, lowering and holding
		if (idleStage != IDLE_NONE && aircraftState == AIRCRAFT_LANDING)
			SetState(AIRCRAFT_FLYING);

		idleStage = IDLE_NONE;
		return;
	}

	if (idleStage != IDLE_NONE)
		return;

	// arrived: hover in place, or circle the point (winged); the queue emptied during the last frame
	idleStage = IDLE_WAIT;
	idleFrame = gs->frameNum - 1;
	goalPos = owner->pos * XZVector;
}


bool CSupComAirMoveType::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;

	// attached units take off when released
	AAirMoveType::Update();

	if (owner->beingBuilt)
		return false;

	switch (aircraftState) {
		case AIRCRAFT_LANDED: {
			// no air movement on the ground
			horSpeed = 0.0f;
			UpdateLanded();
			return false;
		}
		case AIRCRAFT_CRASHING: {
			UpdateFall();
			return true;
		}
		case AIRCRAFT_TAKEOFF: {
			SetState(AIRCRAFT_FLYING);
			UpdateIdle();
			UpdateFlight();
		} break;
		default: {
			UpdateIdle();
			UpdateFlight();
		} break;
	}

	if (aircraftState == AIRCRAFT_LANDED)
		return true;

	UpdateHeading();
	return HandleCollisions(collide);
}

void CSupComAirMoveType::UpdateFlight()
{
	const float3& pos = owner->pos;
	const bool idle = (idleStage != IDLE_NONE);

	float topSpeed = maxSpeed;

	if (maxWantedSpeed > 0.0f)
		topSpeed = std::min(topSpeed, maxWantedSpeed);

	const float3 toGoal = (goalPos - pos) * XZVector;
	const float goalDist = toGoal.Length2D();

	float3 dir;

	if (winged) {
		// always forwards along the heading, turning towards the goal (round it when idle)
		if (goalDist > 1.0f)
			wantedHeading = GetHeadingFromVector(toGoal.x, toGoal.z);

		dir = GetVectorFromHeading(owner->heading);
		horSpeed = topSpeed - (topSpeed - horSpeed) * math::exp(-riseRate);
	} else {
		// exponential approach to the top speed, and to rest at the goal
		const float target = idle? 0.0f: topSpeed;
		horSpeed = target - (target - horSpeed) * math::exp(-speedResponse);

		if (idle) {
			float3 flatSpeed = owner->speed * XZVector;
			dir = (flatSpeed.SqLength2D() > 0.0f)? flatSpeed.SafeNormalize2D(): ZeroVector;
		} else {
			horSpeed = std::min(horSpeed, speedResponse * goalDist);
			dir = (goalDist > 0.0f)? (toGoal / goalDist): ZeroVector;

			if (goalDist > 1.0f)
				wantedHeading = GetHeadingFromVector(dir.x, dir.z);
		}
	}

	horSpeed = std::min(horSpeed, topSpeed);

	float3 step = dir * horSpeed;

	const float3 next = pos + step;
	const float ref = ReferenceHeight(next.x, next.z);
	float newY = pos.y;

	// idle: wait, then lower towards the ground and land
	if (idle && idleStage == IDLE_WAIT && idleLandDelay >= 0.0f && !dontLand && autoLand && owner->commandAI->commandQue.empty()) {
		if ((gs->frameNum - idleFrame) >= idleLandDelay) {
			// the lowering starts exactly at the end of the delay
			idleStage = IDLE_LOWER;
			idleFrame = idleFrame + int(idleLandDelay);
			idleHeight = std::max(0.0f, pos.y - ref);
			SetState(AIRCRAFT_LANDING);
		}
	}

	if (idleStage >= IDLE_LOWER) {
		const float h = UpdateIdleHeight();

		if (h < LAND_HEIGHT) {
			step.y = ref - pos.y;
			ApplyStep(step * UpVector);
			owner->SetVelocityAndSpeed(ZeroVector);

			horSpeed = 0.0f;
			idleStage = IDLE_NONE;
			SetState(AIRCRAFT_LANDED);
			return;
		}

		newY = ref + h;
	} else {
		// towards the cruise height at no more than the climb rate
		newY = pos.y + std::clamp((ref + wantedHeight) - pos.y, -climbRate, climbRate);
	}

	// never below the reference height
	step.y = std::max(newY, ref) - pos.y;
	ApplyStep(step);
}

float CSupComAirMoveType::UpdateIdleHeight()
{
	// the whole lowering is one curve in time from its start: exponential, optionally interrupted by a hold
	const float t = gs->frameNum - idleFrame;

	if (landHoldHeight <= 0.0f || idleHeight <= landHoldHeight)
		return (idleHeight * math::exp(-landDecay * t));

	const float tHold = math::log(idleHeight / landHoldHeight) / std::max(landDecay, 1e-6f);

	if (t < tHold) {
		idleStage = IDLE_LOWER;
		return (idleHeight * math::exp(-landDecay * t));
	}
	if (t < tHold + landHoldFrames) {
		idleStage = IDLE_HOLD;
		return landHoldHeight;
	}

	idleStage = IDLE_LOWER2;
	return (landHoldHeight * math::exp(-landDecay * (t - tHold - landHoldFrames)));
}

void CSupComAirMoveType::UpdateFall()
{
	// ballistic: the horizontal velocity at death, constant downward acceleration
	if (!falling) {
		falling = true;
		fallVelocity = owner->speed;
	}

	const float3& pos = owner->pos;

	float3 step = fallVelocity;
	step.y = fallVelocity.y - 0.5f * fallGravity;
	fallVelocity.y -= fallGravity;

	const float groundHeight = CGround::GetHeightReal(pos.x + step.x, pos.z + step.z);

	if (pos.y + step.y <= groundHeight) {
		// ground contact: the unit crashes where it hits
		step.y = groundHeight - pos.y;
		ApplyStep(step);
		owner->ForcedKillUnit(nullptr, true, false, -CSolidObject::DAMAGE_AIRCRAFT_CRASHED);
		return;
	}

	ApplyStep(step);

	if (tumbleRate != 0.0f)
		owner->AddHeading(short(tumbleRate), false, false, 0.0f);
}
