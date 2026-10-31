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
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/CommandAI/Command.h"
#include "System/SpringMath.h"

#include "System/Misc/TracyDefs.h"
#include "System/SpringHash.h"

// the per-frame approach rate to the top speed that reaches 90 % of it in rise90 seconds
static float RiseRate(float rise90)
{
	return (math::log(10.0f) / std::max(1.0f / GAME_SPEED, rise90) / GAME_SPEED);
}

CR_BIND_DERIVED(CSupComAirMoveType, CHoverAirMoveType, (nullptr))
CR_REG_METADATA(CSupComAirMoveType, (
	CR_MEMBER(winged),
	CR_MEMBER(speedResponse),
	CR_MEMBER(luaAttackGoal),
	CR_MEMBER(riseRate),
	CR_MEMBER(climbRate),
	CR_MEMBER(fallGravity),
	CR_MEMBER(tumbleRate),
	CR_MEMBER(idleLandDelay),
	CR_MEMBER(landDecay),
	CR_MEMBER(landHoldHeight),
	CR_MEMBER(landHoldFrames),
	CR_MEMBER(cruiseHeight),
	CR_MEMBER(passSwing),
	CR_MEMBER(maxBank),
	CR_MEMBER(transportDescent),
	CR_MEMBER(loadHeight),

	CR_MEMBER(horSpeed),
	CR_MEMBER(idleStage),
	CR_MEMBER(idleFrame),
	CR_MEMBER(idleHeight),
	CR_MEMBER(falling),
	CR_MEMBER(fallVelocity),
	CR_MEMBER(passOut),
	CR_MEMBER(lastAimDist),

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
	riseRate = RiseRate(CustomParam(ud, "supcom_air_rise_90", 1.0f));
	climbRate = std::max(0.0f, CustomParam(ud, "supcom_air_climb_rate", 0.0f)) / GAME_SPEED;
	fallGravity = std::max(0.0f, CustomParam(ud, "supcom_air_fall_gravity", 0.0f)) / (GAME_SPEED * GAME_SPEED);
	tumbleRate = CustomParam(ud, "supcom_air_tumble_rate", 0.0f) / GAME_SPEED * SPRING_CIRCLE_DIVS / 360.0f;

	const float delay = CustomParam(ud, "supcom_air_idle_land_delay", -1.0f);
	idleLandDelay = (delay < 0.0f)? -1.0f: (delay * GAME_SPEED);
	landDecay = std::max(0.0f, CustomParam(ud, "supcom_air_land_decay", 0.0f)) / GAME_SPEED;
	landHoldHeight = std::max(0.0f, CustomParam(ud, "supcom_air_land_hold_height", 0.0f));
	landHoldFrames = std::max(0.0f, CustomParam(ud, "supcom_air_land_hold_time", 0.0f)) * GAME_SPEED;

	passSwing = std::max(0.0f, CustomParam(ud, "supcom_air_pass_swing", 0.0f));
	maxBank = std::max(0.0f, CustomParam(ud, "supcom_air_max_bank", 0.0f)) * math::DEG_TO_RAD;
	transportDescent = std::max(0.0f, CustomParam(ud, "supcom_air_transport_descent", 0.0f)) / GAME_SPEED;
	loadHeight = std::max(0.0f, CustomParam(ud, "supcom_air_load_height", 0.0f));

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

	// attacking a unit, the steering target is the unit itself, not the command AI's snapshot of its position
	// (unless the game steers the attack itself: supcomLuaAttackGoal)
	if (const CUnit* target = AttackTarget(); target != nullptr && !luaAttackGoal)
		goalPos = target->pos * XZVector;

	wantedHeight = cruiseHeight;
}

void CSupComAirMoveType::KeepPointingTo(float3 pos, float distance, bool aggressive)
{
	// attack: keep flying at the target (the base class would hold position instead)
	wantToStop = false;

	// the game steering the attack itself (supcomLuaAttackGoal) keeps its own goal
	if (!luaAttackGoal) {
		goalPos = pos * XZVector;

		if (const CUnit* target = AttackTarget())
			goalPos = target->pos * XZVector;
	}

	wantedHeight = cruiseHeight;
	progressState = AMoveType::Active;

	if (aircraftState == AIRCRAFT_LANDED || aircraftState == AIRCRAFT_LANDING)
		SetState(AIRCRAFT_TAKEOFF);
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

	const short oldHeading = owner->heading;

	UpdateHeading();

	if (winged)
		UpdateBank(oldHeading);

	return HandleCollisions(collide);
}

float CSupComAirMoveType::GetRise90() const
{
	return (math::log(10.0f) / std::max(1e-6f, riseRate * GAME_SPEED));
}

bool CSupComAirMoveType::SetMemberValue(unsigned int memberHash, void* memberValue)
{
	static const unsigned int rise90Hash = spring::LiteHash("supcomRise90", 12, 0);
	static const unsigned int climbRateHash = spring::LiteHash("supcomClimbRate", 15, 0);
	static const unsigned int speedResponseHash = spring::LiteHash("supcomSpeedResponse", 19, 0);
	static const unsigned int luaAttackGoalHash = spring::LiteHash("supcomLuaAttackGoal", 19, 0);

	if (memberHash == luaAttackGoalHash) {
		// Lua passes a boolean as bool*, a number as float*; the key is boolean
		luaAttackGoal = *reinterpret_cast<bool*>(memberValue);
		return true;
	}
	if (memberHash == speedResponseHash) {
		speedResponse = std::max(0.0f, *reinterpret_cast<float*>(memberValue)) / GAME_SPEED;
		return true;
	}
	if (memberHash == rise90Hash) {
		riseRate = RiseRate(*reinterpret_cast<float*>(memberValue));
		return true;
	}
	if (memberHash == climbRateHash) {
		climbRate = std::max(0.0f, *reinterpret_cast<float*>(memberValue)) / GAME_SPEED;
		return true;
	}

	return CHoverAirMoveType::SetMemberValue(memberHash, memberValue);
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
		// attacking a unit: aim at its current position, refreshed every frame
		if (const CUnit* target = AttackTarget())
			goalPos = target->pos * XZVector;

		const float3 toAim = (goalPos - pos) * XZVector;
		const float aimDist = toAim.Length2D();

		// passes: overshoot the aim point, run straight out to the swing distance, then turn back
		if (passSwing > 0.0f) {
			if (passOut) {
				passOut = (aimDist < passSwing);
			} else if (aimDist < passSwing * 0.25f && aimDist > lastAimDist) {
				passOut = true;
			}
		}

		lastAimDist = aimDist;

		// always forwards along the heading, turning towards the goal (round it when idle)
		if (!passOut && aimDist > 1.0f)
			wantedHeading = GetHeadingFromVector(toAim.x, toAim.z);

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
		// towards the wanted height (cruise, or the command AI's load height) at no more than the climb rate;
		// a transport under a load or unload order lowers at no more than its transport descent
		const float downRate = (transportDescent > 0.0f && InTransportOrder())? transportDescent: climbRate;
		newY = pos.y + std::clamp((ref + wantedHeight) - pos.y, -downRate, climbRate);
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

const CUnit* CSupComAirMoveType::AttackTarget() const
{
	// the unit named by the attack order at the front of the queue (not the command AI's last snapshot of it)
	const auto& queue = owner->commandAI->commandQue;

	if (queue.empty())
		return nullptr;

	const Command& c = queue.front();

	if (c.GetID() != CMD_ATTACK || c.GetNumParams() != 1)
		return nullptr;

	// a command parameter is a float from the network: check the range before converting it to a unit id
	const float targetParam = c.GetParam(0);
	const CUnit* target = (targetParam >= 0.0f && targetParam < float(MAX_UNITS))? unitHandler.GetUnit(int(targetParam)): nullptr;

	if (target == nullptr || target == owner)
		return nullptr;

	return target;
}

bool CSupComAirMoveType::InTransportOrder() const
{
	const auto& queue = owner->commandAI->commandQue;

	if (queue.empty())
		return false;

	switch (queue.front().GetID()) {
		case CMD_LOAD_UNITS:
		case CMD_LOAD_ONTO:
		case CMD_UNLOAD_UNIT:
		case CMD_UNLOAD_UNITS:
			return true;
		default:
			break;
	}

	return false;
}

float CSupComAirMoveType::TransportHeight(const float3& pos, float engineHeight) const
{
	if (loadHeight <= 0.0f)
		return engineHeight;

	return (ReferenceHeight(pos.x, pos.z) + loadHeight);
}

void CSupComAirMoveType::UpdateBank(short oldHeading)
{
	// bank follows the turn rate, up to the maximum bank at the full turn rate (presentation)
	const short delta = owner->heading - oldHeading;
	const float rate = std::clamp(float(delta) / std::max(1.0f, turnRate), -1.0f, 1.0f);
	const float bank = maxBank * rate;

	const float3 front = GetVectorFromHeading(owner->heading);
	const float3 right = float3{-front.z, 0.0f, front.x};
	const float c = math::cos(bank);
	const float s = math::sin(bank);

	owner->frontdir = front;
	owner->rightdir = right * c + UpVector * s;
	owner->updir = UpVector * c - right * s;
	owner->UpdateMidAndAimPos();
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
		// float to int first, then to short: a direct float-to-short conversion is undefined outside the short range
		// and x86 and arm64 compilers give different results there (cross-architecture desync, Q120)
		owner->AddHeading(short(int(tumbleRate)), false, false, 0.0f);
}
