/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComAirMoveType.h"
#include "SupComHalfRate.h"

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
	CR_MEMBER(combatTurnRate),
	CR_MEMBER(idleLandDelay),
	CR_MEMBER(landDecay),
	CR_MEMBER(landHoldHeight),
	CR_MEMBER(landHoldFrames),
	CR_MEMBER(cruiseHeight),
	CR_MEMBER(passSwing),
	CR_MEMBER(maxBank),
	CR_MEMBER(transportDescent),
	CR_MEMBER(loadHeight),
	CR_MEMBER(earlyDescent),
	CR_MEMBER(orbitRadius),
	CR_MEMBER(pickupCmd),
	CR_MEMBER(attackDip),
	CR_MEMBER(takeoffOvershoot),
	CR_MEMBER(takeoffPeak),
	CR_MEMBER(passRun),

	CR_MEMBER(horSpeed),
	CR_MEMBER(idleStage),
	CR_MEMBER(idleFrame),
	CR_MEMBER(idleHeight),
	CR_MEMBER(falling),
	CR_MEMBER(fallVelocity),
	CR_MEMBER(passOut),
	CR_MEMBER(passLeg),
	CR_MEMBER(passTurn),
	CR_MEMBER(passSpeedFactor),
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
	combatTurnRate = std::max(0.0f, CustomParam(ud, "supcom_air_combat_turn_rate", 0.0f)) / GAME_SPEED * SPRING_CIRCLE_DIVS / 360.0f;

	const float delay = CustomParam(ud, "supcom_air_idle_land_delay", -1.0f);
	idleLandDelay = (delay < 0.0f)? -1.0f: (delay * GAME_SPEED);
	landDecay = std::max(0.0f, CustomParam(ud, "supcom_air_land_decay", 0.0f)) / GAME_SPEED;
	landHoldHeight = std::max(0.0f, CustomParam(ud, "supcom_air_land_hold_height", 0.0f));
	landHoldFrames = std::max(0.0f, CustomParam(ud, "supcom_air_land_hold_time", 0.0f)) * GAME_SPEED;

	passSwing = std::max(0.0f, CustomParam(ud, "supcom_air_pass_swing", 0.0f));
	maxBank = std::max(0.0f, CustomParam(ud, "supcom_air_max_bank", 0.0f)) * math::DEG_TO_RAD;
	transportDescent = std::max(0.0f, CustomParam(ud, "supcom_air_transport_descent", 0.0f)) / GAME_SPEED;
	loadHeight = std::max(0.0f, CustomParam(ud, "supcom_air_load_height", 0.0f));
	earlyDescent = (CustomParam(ud, "supcom_air_early_descent", 0.0f) != 0.0f);
	pickupCmd = int(CustomParam(ud, "supcom_air_pickup_cmd", 0.0f));
	attackDip = std::max(0.0f, CustomParam(ud, "supcom_air_attack_dip", 0.0f));
	passRun = std::max(0.0f, CustomParam(ud, "supcom_air_pass_overflight", 0.0f));
	passSpeedFactor = std::clamp(CustomParam(ud, "supcom_air_pass_speed_factor", 1.0f), 0.05f, 1.0f);
	takeoffOvershoot = std::max(0.0f, CustomParam(ud, "supcom_air_takeoff_overshoot", 0.0f));

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

	// idle half rate (SupComHalfRate.h): flying or hovering with nothing to do, well above the ground
	const bool idleHere = (aircraftState == AIRCRAFT_FLYING || aircraftState == AIRCRAFT_HOVERING)
		&& AttackTarget() == nullptr && !InTransportOrder() && !InAttackOrder() && !InInternalMove()
		&& owner->pos.y - CGround::GetHeightReal(owner->pos.x, owner->pos.z) > 3.0f * owner->radius;
	const int halfRate = SupComHalfRate::Air(owner, idleHere);
	if (halfRate == 1) {
		ApplyStep(float3(owner->speed));   // coast one step at the current velocity
		return true;
	}

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
			// a hovering aircraft leaving the ground climbs past the cruise height first (supcom_air_takeoff_overshoot)
			takeoffPeak = (!winged && takeoffOvershoot > 0.0f);
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

	// with an attack target the combat turn rate limits the turn (MOV-016 v5 R3)
	if (combatTurnRate > 0.0f && AttackTarget() != nullptr) {
		const float defTurnRate = turnRate;
		turnRate = combatTurnRate;
		UpdateHeading();
		turnRate = defTurnRate;
	} else {
		UpdateHeading();
		if (halfRate == 2)
			UpdateHeading();               // half rate: the turn of the coasted frame too (doubled dt)
	}

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
	static const unsigned int combatTurnRateHash = spring::LiteHash("supcomCombatTurnRate", 20, 0);
	static const unsigned int orbitRadiusHash = spring::LiteHash("supcomOrbitRadius", 17, 0);

	if (memberHash == luaAttackGoalHash) {
		// Lua passes a boolean as bool*, a number as float*; the key is boolean
		luaAttackGoal = *reinterpret_cast<bool*>(memberValue);
		return true;
	}
	if (memberHash == orbitRadiusHash) {
		orbitRadius = std::max(0.0f, *reinterpret_cast<float*>(memberValue));
		return true;
	}
	if (memberHash == combatTurnRateHash) {
		combatTurnRate = std::max(0.0f, *reinterpret_cast<float*>(memberValue)) / GAME_SPEED * SPRING_CIRCLE_DIVS / 360.0f;
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

	// supcomOrbitRadius: slow enough that the turning circle fits the orbit (speed <= turn rate x radius)
	if (winged && orbitRadius > 0.0f && AttackTarget() == nullptr)
		topSpeed = std::min(topSpeed, std::max(0.1f, turnRate * math::TWOPI / SPRING_CIRCLE_DIVS * orbitRadius));

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
				passOut = (aimDist < ((passRun > 0.0f)? passRun: passSwing));
				passTurn = !passOut;
			} else if ((aimDist < passSwing * 0.25f || aimDist < passRun) && aimDist > lastAimDist) {
				// moving away inside the overflight distance (supcom_air_pass_overflight) also runs out: a pass the game
				// lengthened with a move leg (mov_air_pass) resumes the attack outbound and holds its line to that distance
				passOut = true;
			}
		}

		lastAimDist = aimDist;

		// supcom_air_pass_speed_factor: the run-out and the turn back fly at this fraction of the top speed, until the
		// heading is back within 20 degrees of the aim (a smaller turning circle: radius = speed / turn rate)
		// an internal move leg (a break-off the game inserted, e.g. mov_air_pass) counts as the run-out: the factor applies on
		// it, and the turn back after it is a pass turn
		const bool internalLeg = (passSpeedFactor < 1.0f && InInternalMove());
		if (passLeg && !internalLeg)
			passTurn = true;
		passLeg = internalLeg;

		if (!passLeg && passTurn && (passSwing <= 0.0f || aimDist <= 1.0f || std::abs(short(GetHeadingFromVector(toAim.x, toAim.z) - owner->heading)) < short(SPRING_CIRCLE_DIVS / 18)))
			passTurn = false;
		if ((passOut || passLeg || passTurn) && passSpeedFactor < 1.0f)
			topSpeed *= passSpeedFactor;

		// always forwards along the heading, turning towards the goal (round it when idle)
		if (orbitRadius > 0.0f && AttackTarget() == nullptr && aimDist > 1.0f) {
			// supcomOrbitRadius (set from Lua, e.g. an aircraft waiting to be stored): hold a circle of that radius round the
			// goal instead of passes; the steer mixes the tangent with the radial error (as tight as the turn rate allows)
			passOut = false;
			const float3 radial = toAim / aimDist;
			const float3 tangent = float3(-radial.z, 0.0f, radial.x);
			const float k = std::clamp((aimDist - orbitRadius) / orbitRadius * 2.0f, -1.0f, 3.0f);
			const float3 want = tangent + radial * k;
			wantedHeading = GetHeadingFromVector(want.x, want.z);
		} else if (!passOut && aimDist > 1.0f)
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
		const bool inTransport = InTransportOrder();
		const float downRate = (transportDescent > 0.0f && inTransport)? transportDescent: climbRate;
		// supcom_air_early_descent: under a transport order the transport lowers to its load height from the order on,
		// during the approach, not only once the command AI has it over the spot
		float wh = (earlyDescent && loadHeight > 0.0f && inTransport)? std::min(wantedHeight, loadHeight): wantedHeight;

		// take-off overshoot: the raised height until it is reached, then the plain wanted height; any order but a
		// plain flight (transport, idle lowering) ends it
		if (takeoffPeak) {
			const float peak = wantedHeight * (1.0f + takeoffOvershoot);

			if (inTransport || wantedHeight != cruiseHeight || (pos.y - ref) >= peak - 0.5f)
				takeoffPeak = false;
			else
				wh = peak;
		}

		// attack dip: inbound on an attack order (not running out past the aim point), within 1.5 s of flight of it
		if (winged && attackDip > 0.0f && !passOut && InAttackOrder()) {
			const float aimDist = ((goalPos - pos) * XZVector).Length2D();

			if (aimDist < topSpeed * GAME_SPEED * 1.5f)
				wh = std::max(0.0f, wh - attackDip);
		}

		newY = pos.y + std::clamp((ref + wh) - pos.y, -downRate, climbRate);
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

bool CSupComAirMoveType::InAttackOrder() const
{
	const auto& queue = owner->commandAI->commandQue;
	return (!queue.empty() && queue.front().GetID() == CMD_ATTACK);
}

bool CSupComAirMoveType::InInternalMove() const
{
	const auto& queue = owner->commandAI->commandQue;
	return (!queue.empty() && queue.front().GetID() == CMD_MOVE && (queue.front().GetOpts() & INTERNAL_ORDER) != 0);
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

	// a game's own pickup command (customParams supcom_air_pickup_cmd) counts as a transport order
	if (pickupCmd != 0 && queue.front().GetID() == pickupCmd)
		return true;

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

bool CSupComAirMoveType::AirPassThrough(const CUnit* owner, const CUnit* other)
{
	if (owner == nullptr || other == nullptr || dynamic_cast<const CSupComAirMoveType*>(owner->moveType) == nullptr)
		return false;
	if (!owner->IsInAir() || !other->IsInAir())
		return false;
	return other->unitDef->IsAirUnit();
}
