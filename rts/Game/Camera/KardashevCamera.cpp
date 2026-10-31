/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

// supcom: our own camera controller (Kardashev Type One; owner decision 2026-10-29), beside the stock CSpringController,
// which stays untouched and selectable (springsetting KardashevCamera = 0, or CamModeName = "spring").
//
// The view is a focus on the ground or water surface, a heading, and a zoom position u: 0 at the closest view, 1 at the
// view that frames the whole playable area, in log distance (every wheel notch moves u by the same amount). The angle
// below the horizon and the vertical field of view follow u on one strictly monotonic curve: low and oblique close in,
// almost straight down far out. Only the zoom eases (exponentially, in u); pan, rotation and tilt are direct. A manual
// tilt holds its angle through zooming until a reset. Zooming in goes toward the focus; zooming out keeps the ground under
// the pointer on its screen spot, and over the top of the range slides the focus to the middle of the playable area.
// Tracking follows a unit's drawn (interpolated) position, or its transport's; the lock does the same for one unit (the
// player's commander, set by Lua) while it is selected. Lua reads and drives it through Spring.Get/SetCameraState (keys
// in GetState and SetState below). Display only: nothing here touches the simulation.

#include "KardashevCamera.h"

#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/GlobalUnsynced.h"
#include "Game/SelectedUnitsHandler.h"
#include "Game/UI/MouseHandler.h"
#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"
#include "System/MathConstants.h"
#include "System/SpringMath.h"
#include "System/Misc/SpringTime.h"

#include "System/Misc/TracyDefs.h"

#include <algorithm>
#include <cmath>

CONFIG(bool,  KardashevCamera).defaultValue(true).description("supcom: start in our own camera (KardashevCamera); 0 starts the stock spring camera (CamMode) as in Recoil.");
CONFIG(float, KardashevCamWheelSensitivity).defaultValue(1.0f).minimumValue(0.1f).maximumValue(4.0f).description("supcom: KardashevCamera wheel notch size, times the default.");
CONFIG(float, KardashevCamPanSpeed).defaultValue(1.0f).minimumValue(0.1f).maximumValue(4.0f).description("supcom: KardashevCamera pan speed (keys, edge, middle-drag), times the default.");
CONFIG(float, KardashevCamRotateSpeed).defaultValue(1.0f).minimumValue(0.1f).maximumValue(4.0f).description("supcom: KardashevCamera rotation speed, times the default.");
CONFIG(bool,  KardashevCamInvertPan).defaultValue(false).description("supcom: KardashevCamera middle-drag pans the other way.");

// The fixed numbers of the camera. DESIGN (2026-10-29): chosen by us and tuned by feel and screenshots (docs/ui/camera.md
// in the game repository); no outside numbers. The numbers the game tunes (proportions and pacing: limits, the curve, the
// wheel step, the glide, pan speeds, terrain clearance) are CKardashevCamera::Feel in the header, settable from Lua.
namespace kcam {
	constexpr float KEY_ZOOM         = 8.0f;                    // zoom keys: notches per second
	constexpr float KEY_ZOOM_FAST    = 2.5f;                    // Shift on the zoom keys
	// tilt
	constexpr float TILT_STEP        = 4.0f * math::DEG_TO_RAD; // one modifier+wheel notch
	constexpr float TILT_DRAG        = 90.0f * math::DEG_TO_RAD; // modifier+drag over the screen's height
	constexpr float KEY_TILT         = 1.0f;                    // modifier+keys (rad/s)
	constexpr float RELEASE_BAND     = 2.5f * math::DEG_TO_RAD; // a set angle this close to the curve's is the curve's
	constexpr float ELEV_MIN         = 3.0f * math::DEG_TO_RAD; // tilt limit near the horizon
	constexpr float ELEV_MAX         = 88.0f * math::DEG_TO_RAD; // tilt limit near straight down
	// rotation
	constexpr float ROT_DRAG         = 1.0f;                    // turns per screen width of modifier+drag
	constexpr float KEY_ROT          = 1.5f;                    // modifier+keys (rad/s)
	constexpr float DEFAULT_HEADING  = 0.0f;                    // the reset heading (north up)
	// pan
	constexpr int   EDGE_PX          = 2;                       // edge scroll only this close to the screen edge (pixels)
	constexpr float PAN_TOL          = 1.0f;                    // a focus set further than this (elmos) is a move (ends tracking)
	// terrain
	constexpr int   CLEAR_SAMPLES    = 16;
	constexpr float CLEAR_STEP       = 1.0f * math::DEG_TO_RAD; // over a hill the view steepens in steps of this
	// tracking
	// keys
	constexpr float START_U          = 0.55f;                   // the view before anything sets one
	constexpr float POP_U            = 0.12f;                   // zoom pop: the close view at the pointer
	constexpr float POP_SPLIT        = 0.5f;                    // zoom pop goes close above this u, else to the full map
	constexpr float POP_GLIDE        = 0.3f;                    // zoom pop and reset glides (s)
}

namespace {
	// 2 (deselected) and 6 (lost from sight) are retired (wheel-zoom 2026-10-31): tracking is a latched follow
	enum TrackEnd { END_NONE = 0, END_PAN = 1, END_DIED = 3, END_KEY = 4, END_SCRIPT = 5 };

	float Smooth(float t) { t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }
	float Shape(float u, float shape) { u = std::clamp(u, 0.0f, 1.0f); return (1.0f - shape) * u + shape * Smooth(u); }

	// the view's axes for elevation e and ground heading h: forward D, right R (on the ground), up U (as CCamera's)
	struct Basis {
		float3 D, R, U;
		Basis(float e, const float3& h) {
			const float ce = std::cos(e), se = std::sin(e);
			D = float3(ce * h.x, -se, ce * h.z);
			R = float3(-h.z, 0.0f, h.x);
			U = R.cross(D);
		}
	};

	float FocusHeight(float x, float z) { return CGround::GetHeightAboveWater(x, z, false); }

	bool CanSee(const CUnit* u) {
		return gu->spectatingFullView || u->allyteam == gu->myAllyTeam || (u->losStatus[gu->myAllyTeam] & LOS_INLOS) != 0;
	}

	bool IsSelected(int unitID) {
		const auto& su = selectedUnitsHandler.selectedUnits;
		return su.find(unitID) != su.end();
	}
}


CKardashevCamera::CKardashevCamera()
{
	RECOIL_DETAILED_TRACY_ZONE;
	playRect = float4(0.0f, 0.0f, mapDims.mapx * SQUARE_SIZE, mapDims.mapy * SQUARE_SIZE);
	clearIns = float4(0.0f, 0.0f, 0.0f, 0.0f);
	pos = float3(playRect.x + playRect.z, 0.0f, playRect.y + playRect.w) * 0.5f;
	curU = tgtU = kcam::START_U;
	fov = feel.fovNear;

	configHandler->NotifyOnChange(this, {"KardashevCamera", "KardashevCamWheelSensitivity", "KardashevCamPanSpeed", "KardashevCamRotateSpeed", "KardashevCamInvertPan", "ScrollWheelSpeed"});
	ConfigUpdate();
}

CKardashevCamera::~CKardashevCamera()
{
	configHandler->RemoveObserver(this);
}

void CKardashevCamera::ConfigUpdate()
{
	enabled = configHandler->GetBool("KardashevCamera");
	wheelSens = configHandler->GetFloat("KardashevCamWheelSensitivity");
	panSpeed = configHandler->GetFloat("KardashevCamPanSpeed");
	rotSpeed = configHandler->GetFloat("KardashevCamRotateSpeed");
	invertPan = configHandler->GetBool("KardashevCamInvertPan");
	scrollWheelSpeed = std::max(std::abs(configHandler->GetFloat("ScrollWheelSpeed")), 0.001f);
}

void CKardashevCamera::ConfigNotify(const std::string& key, const std::string& value)
{
	ConfigUpdate();
}


/******************************************************************************/
// the curve

float CKardashevCamera::DistOf(float u) const { return lim.dMin * std::pow(lim.dMax / lim.dMin, std::clamp(u, 0.0f, 1.0f)); }
float CKardashevCamera::UOf(float d) const { return std::clamp(std::log(std::max(d, 1.0f) / lim.dMin) / std::log(lim.dMax / lim.dMin), 0.0f, 1.0f); }
float CKardashevCamera::ElevCurve(float u) const { return feel.elevNear + (feel.elevFar - feel.elevNear) * Shape(u, feel.shape); }
float CKardashevCamera::FovCurve(float u) const { return feel.fovNear + (feel.fovFar - feel.fovNear) * Shape(u, feel.shape); }
float CKardashevCamera::CentreWeight(float u) const { return Smooth((u - feel.centreFrom) / std::max(1e-3f, 1.0f - feel.centreFrom)); }
float CKardashevCamera::Elev() const { return (heldElev >= 0.0f)? heldElev: ElevCurve(curU); }
float CKardashevCamera::Aspect() const { return globalRendering->viewSizeX / std::max(1.0f, float(globalRendering->viewSizeY)); }
float3 CKardashevCamera::Heading() const { return float3(std::sin(rotY), 0.0f, -std::cos(rotY)); }

// the furthest view: the least distance that shows the playable area round its middle at the far angle and fov, north up,
// every corner margin in from the screen border and off the insets Lua keeps clear (the HUD's frame panels). From the
// playable area and the viewport only: a turn changes neither the limits nor the zoom
void CKardashevCamera::Refit()
{
	RECOIL_DETAILED_TRACY_ZONE;
	fitAspect = Aspect();
	refitNeeded = false;

	const float3 h(0.0f, 0.0f, -1.0f);
	const Basis b(feel.elevFar, h);
	const float ta = std::tan(feel.fovFar * 0.5f * math::DEG_TO_RAD);
	const float3 c((playRect.x + playRect.z) * 0.5f, 0.0f, (playRect.y + playRect.w) * 0.5f);
	const float3 corners[4] = {{playRect.x, 0.0f, playRect.y}, {playRect.z, 0.0f, playRect.y}, {playRect.x, 0.0f, playRect.w}, {playRect.z, 0.0f, playRect.w}};
	const float xl = -1.0f + 2.0f * clearIns.x, xr = 1.0f - 2.0f * clearIns.y;
	const float yt = 1.0f - 2.0f * clearIns.z, yb = -1.0f + 2.0f * clearIns.w;

	const auto Fits = [&](float d) {
		const float3 cam = c - b.D * d;
		for (const float3& g: corners) {
			const float3 v = g - cam;
			const float depth = v.dot(b.D);
			if (depth <= 0.0f)
				return false;
			const float x = v.dot(b.R) / (depth * ta * fitAspect) * feel.margin;
			const float y = v.dot(b.U) / (depth * ta) * feel.margin;
			if (x < xl || x > xr || y < yb || y > yt)
				return false;
		}
		return true;
	};

	float lo = 1.0f, hi = std::max(playRect.z - playRect.x, playRect.w - playRect.y) * 100.0f;
	for (int i = 0; i < 60; i++) {
		const float mid = (lo + hi) * 0.5f;
		(Fits(mid)? hi: lo) = mid;
	}

	lim.dMin = feel.minDist;
	lim.dMax = std::max(hi, feel.minDist * 1.5f);
}


/******************************************************************************/
// input

// n notches (< 0 in); pointer: the wheel (in dives at the ground under the pointer), false for keys (never anchor)
void CKardashevCamera::Notches(float n, bool pointer)
{
	if (n == 0.0f)
		return;

	diveOn = (n < 0.0f && pointer && trackMode == TRACK_NONE);
	diveHeld = false;

	tgtU = std::clamp(tgtU + n * std::log(1.0f + feel.stepOut) / std::log(lim.dMax / lim.dMin), 0.0f, 1.0f);

	if (glideScale <= 0.0f)
		curU = tgtU;
}

void CKardashevCamera::Tilt(float dElev)
{
	heldElev = std::clamp(Elev() + dElev, kcam::ELEV_MIN, kcam::ELEV_MAX);
}

void CKardashevCamera::Pan(float dRight, float dFwd)
{
	StopTrack(END_PAN);
	diveOn = false;

	const float3 h = Heading();
	pos += (float3(-h.z, 0.0f, h.x) * dRight + h * dFwd);
	pos = ClampFocus(pos, curU);
}

void CKardashevCamera::KeyMove(float3 move)
{
	RECOIL_DETAILED_TRACY_ZONE;
	const bool* ms = camHandler->GetActiveCamera()->GetMovState();

	if (ms[CCamera::MOVE_STATE_RTT] && feel.freeRotate) {
		rotY += move.x * kcam::KEY_ROT * rotSpeed;
		if (move.y != 0.0f)
			Tilt(move.y * kcam::KEY_TILT);
		Update();
		return;
	}

	// move.xy are seconds of key hold this frame
	const float viewH = 2.0f * curDist * std::tan(fov * 0.5f * math::DEG_TO_RAD);
	const float rate = viewH * feel.keyPan * panSpeed * (ms[CCamera::MOVE_STATE_FST]? feel.keyFast: 1.0f);
	Pan(move.x * rate, move.y * rate);
	Update();
}

void CKardashevCamera::MouseMove(float3 move)
{
	RECOIL_DETAILED_TRACY_ZONE;
	const bool* ms = camHandler->GetActiveCamera()->GetMovState();

	if (ms[CCamera::MOVE_STATE_RTT] && feel.freeRotate) {
		// modifier+drag: across turns (a screen width is ROT_DRAG turns), up and down tilts (up: steeper)
		rotY += move.x / std::max(1.0f, float(globalRendering->viewSizeX)) * math::TWOPI * kcam::ROT_DRAG * rotSpeed;
		if (move.y != 0.0f)
			Tilt(-move.y / std::max(1.0f, float(globalRendering->viewSizeY)) * kcam::TILT_DRAG);
		Update();
		return;
	}

	// z is the engine's InvertMouse sign; a drag moves the same share of the view at any height
	const float epp = 2.0f * curDist * std::tan(fov * 0.5f * math::DEG_TO_RAD) / std::max(1.0f, float(globalRendering->viewSizeY));
	const float s = move.z * (invertPan? -1.0f: 1.0f) * epp * feel.dragPan * panSpeed;
	Pan(move.x * s, -move.y * s);
	Update();
}

void CKardashevCamera::ScreenEdgeMove(float3 move)
{
	RECOIL_DETAILED_TRACY_ZONE;
	// only within EDGE_PX of the window's edge, whatever EdgeMoveWidth says (Lua turns WindowedEdgeMove off over the HUD)
	const int x = mouse->lastx, y = mouse->lasty;
	const int w = globalRendering->winSizeX, h = globalRendering->viewSizeY;
	const float dx = (x < kcam::EDGE_PX)? -1.0f: ((x >= w - kcam::EDGE_PX)? 1.0f: 0.0f);
	const float dy = (y < kcam::EDGE_PX)?  1.0f: ((y >= h - kcam::EDGE_PX)? -1.0f: 0.0f);

	if (dx == 0.0f && dy == 0.0f)
		return;

	const bool* ms = camHandler->GetActiveCamera()->GetMovState();
	const float secs = globalRendering->lastFrameTime * 0.001f;
	const float viewH = 2.0f * curDist * std::tan(fov * 0.5f * math::DEG_TO_RAD);
	const float rate = viewH * feel.keyPan * panSpeed * (ms[CCamera::MOVE_STATE_FST]? feel.keyFast: 1.0f) * secs;
	Pan(dx * rate, dy * rate);
	Update();
}

void CKardashevCamera::MouseWheelMove(float move)
{
	RECOIL_DETAILED_TRACY_ZONE;
	const bool* ms = camHandler->GetActiveCamera()->GetMovState();

	if (ms[CCamera::MOVE_STATE_UP] || ms[CCamera::MOVE_STATE_DWN]) {
		// the zoom keys (moveup / movedown): move is 0.2 per millisecond of hold, positive out
		const float secs = std::abs(move) / 0.2f * 0.001f;
		Notches(std::copysign(kcam::KEY_ZOOM * secs * (ms[CCamera::MOVE_STATE_FST]? kcam::KEY_ZOOM_FAST: 1.0f), move), false);
		Update();
		return;
	}

	// the wheel: ScrollWheelSpeed per notch, negative in (precise scrolling gives shares of a notch)
	const float n = move / scrollWheelSpeed * wheelSens;

	if (ms[CCamera::MOVE_STATE_TLT] && feel.freeRotate) {
		Tilt(n * kcam::TILT_STEP);
		Update();
		return;
	}

	Notches(n, true);
	Update();
}

void CKardashevCamera::ResetView()
{
	rotY = kcam::DEFAULT_HEADING;
	heldElev = -1.0f;
	Update();
	camHandler->CameraTransition(kcam::POP_GLIDE * glideScale);
}

void CKardashevCamera::ZoomPop()
{
	if (curU > kcam::POP_SPLIT) {
		const float3 cam = GetPos();
		const float t = CGround::LineGroundCol(cam, cam + mouse->dir * 1e6f, false);
		if (t <= 0.0f)
			return;
		StopTrack(END_PAN);
		pos = cam + mouse->dir * t;
		curU = tgtU = kcam::POP_U;
	} else {
		curU = tgtU = 1.0f;
	}
	diveOn = false;
	Update();
	camHandler->CameraTransition(kcam::POP_GLIDE * glideScale);
}

// the ground or water surface under the pointer (a unit there: the ground at its base, which the ground ray finds); false
// for the sky or a point off the map
// the pointer's ground point and its spot on the screen in the view's own axes (the view as it is now, before this frame's
// zoom step)
bool CKardashevCamera::PointerAnchor(float3& g, float& sx, float& sy) const
{
	if (!PointerGround(g))
		return false;
	const float3 md = mouse->dir;
	const Basis b(rotX - math::HALFPI, Heading());
	const float depth = md.dot(b.D);
	if (depth <= 1e-4f)
		return false;
	const float ta = std::tan(fov * 0.5f * math::DEG_TO_RAD);
	sx = md.dot(b.R) / depth / (ta * Aspect());
	sy = md.dot(b.U) / depth / ta;
	return true;
}

bool CKardashevCamera::PointerGround(float3& g) const
{
	const float3 cam = GetPos();
	const float3 md = mouse->dir;
	const float t = CGround::LineGroundCol(cam, cam + md * 1e6f, false);
	if (t <= 0.0f)
		return false;
	g = cam + md * t;
	if (g.y < 0.0f && md.y < -1e-4f && cam.y > 0.0f)
		g = cam + md * (-cam.y / md.y);                      // water: the surface (water level 0)
	return g.x >= 0.0f && g.z >= 0.0f && g.x <= mapDims.mapx * SQUARE_SIZE && g.z <= mapDims.mapy * SQUARE_SIZE;
}


/******************************************************************************/
// tracking and the lock

// the selection in a fixed order (unit id), so the same selection always gives the same unit
std::vector<int> CKardashevCamera::SortedSelection() const
{
	std::vector<int> v(selectedUnitsHandler.selectedUnits.begin(), selectedUnitsHandler.selectedUnits.end());
	std::sort(v.begin(), v.end());
	return v;
}

// the first live unit of a set, or -1
int CKardashevCamera::FirstLive(const std::vector<int>& set) const
{
	for (const int id: set) {
		const CUnit* u = unitHandler.GetUnit(id);
		if (u != nullptr && !u->isDead)
			return id;
	}
	return -1;
}

// the drawn position of the followed unit (or its transport's); false while the viewer cannot see it
bool CKardashevCamera::TrackDrawPos(const CUnit* u, float3& p) const
{
	if (u == nullptr || !CanSee(u))
		return false;
	const CUnit* t = u->GetTransporter();
	p = (t != nullptr)? t->GetDrawPos(globalRendering->timeOffset): u->GetDrawPos(globalRendering->timeOffset);
	return true;
}

// follow the first live unit of `set` (sorted); no glide: the focus is on it from the next frame
void CKardashevCamera::StartTrack(std::vector<int> set, int mode)
{
	std::sort(set.begin(), set.end());
	const int id = FirstLive(set);
	if (id < 0)
		return;

	trackSet = std::move(set);
	trackUnit = id;
	trackMode = mode;
	float3 p;
	trackLast = TrackDrawPos(unitHandler.GetUnit(id), p)? p: pos;
	diveOn = false;
}

void CKardashevCamera::StopTrack(int reason)
{
	if (trackMode == TRACK_NONE)
		return;
	trackUnit = -1;
	trackMode = TRACK_NONE;
	trackSet.clear();
	lastEnd = reason;
}

// the track key: stops when the selection is empty or its first unit is the one followed; otherwise follows the
// selection (a new set switches the follow in one press)
void CKardashevCamera::ToggleTrack()
{
	std::vector<int> sel = SortedSelection();
	const int first = FirstLive(sel);
	if (trackMode != TRACK_NONE && (first < 0 || first == trackUnit)) {
		StopTrack(END_KEY);
		return;
	}
	if (first >= 0)
		StartTrack(std::move(sel), TRACK_FOLLOW);
}

// the focus for this frame while tracking; false (and tracking off, the focus left where it is) when it ends. A latched
// follow: leaving or clearing the selection, losing sight, cloak, a dive or fog do not end it (the focus stays on the last
// drawn position until the unit shows again); its death moves it to the next live unit of the set; it ends (died) when all
// are dead
bool CKardashevCamera::TrackFocus(float dt, float3& focus)
{
	const CUnit* u = unitHandler.GetUnit(trackUnit);
	if (u == nullptr || u->isDead) {
		const int next = FirstLive(trackSet);
		if (next < 0) {
			StopTrack(END_DIED);
			return false;
		}
		trackUnit = next;
		u = unitHandler.GetUnit(next);
	}

	float3 p;
	if (TrackDrawPos(u, p))
		trackLast = p;

	focus = float3(trackLast.x, 0.0f, trackLast.z);
	return true;
}


/******************************************************************************/
// placing the view

float3 CKardashevCamera::ClampFocus(float3 p, float u) const
{
	// inside the playable area; over the top of the range (phased in from centreFrom) only as far from its middle as keeps
	// the view's ground footprint (north up, as seen from above) over the area, so the furthest view frames the whole area
	// round its middle and a mid-range view is not pulled off the point it shows
	const float w = CentreWeight(u);
	const float vh = DistOf(u) * std::tan(FovCurve(u) * 0.5f * math::DEG_TO_RAD), vw = vh * Aspect();
	const float cx = (playRect.x + playRect.z) * 0.5f, hx = std::max(0.0f, (playRect.z - playRect.x) * 0.5f - vw * w);
	const float cz = (playRect.y + playRect.w) * 0.5f, hz = std::max(0.0f, (playRect.w - playRect.y) * 0.5f - vh * w);
	p.x = std::clamp(p.x, cx - hx, cx + hx);
	p.z = std::clamp(p.z, cz - hz, cz + hz);
	return p;
}

// elevation e raised as little as needed (CLEAR_STEP steps, at most ELEV_MAX) so that the camera and its line to the focus
// stay CLEARANCE above the ground and water, tapering to 0 at the focus
float CKardashevCamera::ClearElev(const float3& f, float d, float e) const
{
	const float3 h = Heading();
	const auto Ok = [&](float el) {
		const float ce = std::cos(el), se = std::sin(el);
		for (int i = 1; i <= kcam::CLEAR_SAMPLES; i++) {
			const float s = d * i / kcam::CLEAR_SAMPLES;
			const float3 q(f.x - s * ce * h.x, f.y + s * se, f.z - s * ce * h.z);
			if (q.y < CGround::GetHeightAboveWater(q.x, q.z, false) + feel.clearance * i / kcam::CLEAR_SAMPLES)
				return false;
		}
		return true;
	};
	while (e < kcam::ELEV_MAX && !Ok(e))
		e = std::min(kcam::ELEV_MAX, e + kcam::CLEAR_STEP);
	return e;
}

void CKardashevCamera::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;
	const float now = spring_gettime().toSecsf();
	const float dt = (lastTick < 0.0f)? 0.0f: std::clamp(now - lastTick, 0.0f, 0.1f);
	lastTick = now;

	if (refitNeeded || std::abs(Aspect() - fitAspect) > 1e-3f)
		Refit(); // u stays: the view keeps its place on the curve

	// the lock lets go when its unit leaves the selection or dies (TrackFocus); Lua arms it (SetState "commander")
	const float u0 = curU;

	if (curU != tgtU) {
		const float tau = feel.tau * glideScale;
		if (tau <= 0.0f) {
			curU = tgtU;
		} else {
			curU += (tgtU - curU) * (1.0f - std::exp(-dt / tau));
			if (std::abs(tgtU - curU) * std::log(lim.dMax / lim.dMin) < feel.snap)
				curU = tgtU;
		}
	}

	const float dist = DistOf(curU);
	const float elev = Elev();
	const float vfov = FovCurve(curU);
	float3 f = pos;

	if (trackMode != TRACK_NONE && TrackFocus(dt, f)) {
		// tracking: the unit at the centre, zoom and angle stay with the player
	} else if (curU < u0 && diveOn) {
		// wheel in: dive at the pointer; the ground or water under it (sampled every frame) keeps its place while the distance
		// shrinks. Sky or off the map: around the focus
		// (the focus that keeps that point on the pointer's screen spot at this frame's distance, angle and fov; the angle the
		// terrain allows is settled with it in three passes)
		// the pivot is taken again only when the pointer has moved (no drift over uneven ground)
		if (!diveHeld || mouse->lastx != diveMX || mouse->lasty != diveMY) {
			diveHeld = PointerAnchor(diveG, diveSX, diveSY);
			diveMX = mouse->lastx;
			diveMY = mouse->lasty;
		}
		const float3 g = diveG;
		const float sx = diveSX, sy = diveSY;
		if (diveHeld) {
			const float3 h = Heading();
			const float ta = std::tan(vfov * 0.5f * math::DEG_TO_RAD), aspect = Aspect();
			float fy = pos.y, eUse = ClearElev(pos, dist, elev);
			for (int i = 0; i < 3; i++) {
				const Basis b(eUse, h);
				const float3 wv = b.D + b.R * (sx * ta * aspect) + b.U * (sy * ta);
				if (wv.y >= -1e-6f)
					break;
				const float t = (dist * std::sin(eUse) + fy - g.y) / -wv.y;
				if (t <= 0.0f)
					break;
				const float3 a = g + b.D * dist - wv * t;
				f = ClampFocus(float3(a.x, 0.0f, a.z), curU);
				fy = FocusHeight(f.x, f.z);
				eUse = ClearElev(float3(f.x, fy, f.z), dist, elev);
			}
		}
	} else if (curU > u0) {
		// wheel or keys out: back from the focus; over the top of the range the focus slides to the middle
		const float3 c((playRect.x + playRect.z) * 0.5f, 0.0f, (playRect.y + playRect.w) * 0.5f);
		const float c0 = CentreWeight(u0), c1 = CentreWeight(curU);
		if (c1 > c0) {
			const float r = (c0 < 1.0f)? (1.0f - c1) / (1.0f - c0): 0.0f;
			f = float3(c.x + (f.x - c.x) * r, 0.0f, c.z + (f.z - c.z) * r);
		}
	}

	if (curU == tgtU)
		diveOn = false;

	f = ClampFocus(f, curU);
	f.y = FocusHeight(f.x, f.z);
	pos = f;
	curDist = dist;
	fov = vfov;
	rotX = math::HALFPI + ClearElev(pos, dist, elev);
	dir = CCamera::GetFwdFromRot(GetRot());
}

float3 CKardashevCamera::GetPos() const
{
	return pos - dir * curDist;
}

void CKardashevCamera::SetPos(const float3& newPos)
{
	// a move from outside (minimap, SetCameraTarget, the engine): ends tracking and any zoom anchor
	if (std::abs(newPos.x - pos.x) + std::abs(newPos.z - pos.z) > kcam::PAN_TOL)
		StopTrack(END_SCRIPT);
	diveOn = false;
	pos = ClampFocus(newPos, curU);
	Update();
}

void CKardashevCamera::SetRot(const float3& newRot)
{
	rotY = newRot.y;
	const float e = newRot.x - math::HALFPI;
	if (std::abs(newRot.x - rotX) > 0.005f)
		heldElev = (std::abs(e - ElevCurve(curU)) > kcam::RELEASE_BAND)? std::clamp(e, kcam::ELEV_MIN, kcam::ELEV_MAX): -1.0f;
	Update();
}

void CKardashevCamera::SetDir(const float3& newDir)
{
	SetRot(CCamera::GetRotFromDir(newDir));
}

void CKardashevCamera::SwitchTo(const CCameraController* oldCam, const bool showText)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (showText)
		LOG("Switching to Kardashev camera");

	if (oldCam->GetName() == "ov") {
		pos = oldCam->SwitchFrom();
		Update();
		return;
	}

	float3 r = oldCam->GetRot();
	if (r.x <= math::HALFPI)
		r.x = math::HALFPI + ElevCurve(curU);

	const float3 d = CCamera::GetFwdFromRot(r);
	const float3 from = oldCam->SwitchFrom();
	const float dist = DistanceToGround(from, d, 0.0f);

	rotY = r.y;
	if (dist > 0.0f) {
		Refit();
		pos = from + d * dist;
		curU = tgtU = UOf(dist);
	}
	rotX = -1.0f; // the angle the old camera had counts as set
	SetRot(r);
}


/******************************************************************************/
// Lua: Spring.GetCameraState / SetCameraState

void CKardashevCamera::GetState(StateMap& sm) const
{
	CCameraController::GetState(sm); // fov, px py pz (the focus), dx dy dz
	sm["dist"] = curDist;
	sm["rx"] = rotX;
	sm["ry"] = rotY;
	sm["rz"] = 0.0f;
	sm["tiltHeld"] = (heldElev >= 0.0f)? 1.0f: 0.0f;
	// read only
	sm["u"] = curU;
	sm["uTarget"] = tgtU;
	sm["dMin"] = lim.dMin;
	sm["dMax"] = lim.dMax;
	sm["trackUnit"] = trackUnit;
	sm["trackMode"] = trackMode;
	sm["trackEnd"] = lastEnd;
	sm["lockUnit"] = lockUnit;
}

// State keys (a bookmark or another camera state restores its view; the engine's transition glides to it):
//   px py pz, dist, rx, ry, tiltHeld   the view; a focus moved further than PAN_TOL ends tracking (unless "track" is given)
// Settings (never in GetState):
//   playX0 playZ0 playX1 playZ1        the playable rectangle (elmos; default the whole map)
//   minDist margin elevNear elevFar fovNear fovFar shape centreFrom stepOut tau snap keyPan keyFast dragPan clearance
//                                      the feel values (Feel in the header; angles in degrees), the game's own numbers
//   freeRotate                         1: the rotate modifier with drag or keys turns and the tilt modifier with the wheel
//                                      tilts (stock-like free rotation); 0: neither (Lua's "rotate" and "tilt" still do)
//   clearL clearR clearT clearB        screen insets (0..1) the furthest view keeps the map clear of (the HUD's panels)
//   glide                              1 normal, 0 every zoom instant (reduced motion)
// Commands (never in GetState):
//   zoomTo (distance, glides), wheel (notches; < 0 in: dives at the pointer), zoomKey (notches, never anchored: the zoom
//   keys), tilt (notches; < 0 toward the horizon), reset, pop,
//   panR panF (view heights right and forward, as a player's pan: ends tracking), rotate (radians, as a player's turn),
//   track (unit id: follow it; 0: stop; -1: toggle on the first selected unit), commander (unit id the lock arms on
//   when selected: arms now if it is selected; -1: none)
bool CKardashevCamera::SetState(const StateMap& sm)
{
	RECOIL_DETAILED_TRACY_ZONE;
	const auto Has = [&](const char* k) { return sm.find(k) != sm.cend(); };

	{
		bool refit = false;
		const auto SetF = [&](const char* k, float& v) { if (Has(k)) { v = sm[k]; refit = true; } };
		SetF("playX0", playRect.x); SetF("playZ0", playRect.y); SetF("playX1", playRect.z); SetF("playZ1", playRect.w);
		SetF("clearL", clearIns.x); SetF("clearR", clearIns.y); SetF("clearT", clearIns.z); SetF("clearB", clearIns.w);
		if (refit) {
			playRect.x = std::clamp(playRect.x, 0.0f, mapDims.mapx * SQUARE_SIZE * 1.0f);
			playRect.z = std::clamp(playRect.z, playRect.x + SQUARE_SIZE, mapDims.mapx * SQUARE_SIZE * 1.0f);
			playRect.y = std::clamp(playRect.y, 0.0f, mapDims.mapy * SQUARE_SIZE * 1.0f);
			playRect.w = std::clamp(playRect.w, playRect.y + SQUARE_SIZE, mapDims.mapy * SQUARE_SIZE * 1.0f);
			for (float* v: {&clearIns.x, &clearIns.y, &clearIns.z, &clearIns.w})
				*v = std::clamp(*v, 0.0f, 0.45f);
			Refit();
		}
		if (Has("glide"))
			glideScale = std::max(0.0f, sm["glide"]);

		// the feel values (CKardashevCamera::Feel; angles in degrees here)
		bool feelSet = false;
		const auto SetFeel = [&](const char* k, float& v, float lo, float hi, float scale) {
			if (Has(k)) { v = std::clamp(sm[k], lo, hi) * scale; feelSet = true; }
		};
		SetFeel("minDist", feel.minDist, 5.0f, 2000.0f, 1.0f);
		SetFeel("margin", feel.margin, 1.0f, 1.5f, 1.0f);
		SetFeel("elevNear", feel.elevNear, 3.0f, 89.5f, math::DEG_TO_RAD);
		SetFeel("elevFar", feel.elevFar, 3.0f, 89.9f, math::DEG_TO_RAD);
		SetFeel("fovNear", feel.fovNear, 10.0f, 120.0f, 1.0f);
		SetFeel("fovFar", feel.fovFar, 10.0f, 120.0f, 1.0f);
		SetFeel("shape", feel.shape, 0.0f, 0.95f, 1.0f);
		SetFeel("centreFrom", feel.centreFrom, 0.0f, 0.99f, 1.0f);
		SetFeel("stepOut", feel.stepOut, 0.01f, 3.0f, 1.0f);
		SetFeel("tau", feel.tau, 0.0f, 2.0f, 1.0f);
		SetFeel("keyPan", feel.keyPan, 0.0f, 20.0f, 1.0f);
		SetFeel("keyFast", feel.keyFast, 1.0f, 20.0f, 1.0f);
		SetFeel("dragPan", feel.dragPan, 0.0f, 20.0f, 1.0f);
		SetFeel("clearance", feel.clearance, 0.0f, 200.0f, 1.0f);
		SetFeel("snap", feel.snap, 0.0001f, 0.5f, 1.0f);
		if (Has("freeRotate"))
			feel.freeRotate = sm["freeRotate"] != 0.0f;
		if (feelSet) {
			const float d = curDist;
			Refit();
			curU = tgtU = UOf(d);    // the view keeps its distance where the new limits allow
		}
	}

	if (Has("ry"))
		rotY = sm["ry"];

	if (Has("px") || Has("pz")) {
		const float3 np(Has("px")? sm["px"]: pos.x, pos.y, Has("pz")? sm["pz"]: pos.z);
		if (std::abs(np.x - pos.x) + std::abs(np.z - pos.z) > kcam::PAN_TOL) {
			if (!Has("track"))
				StopTrack(END_SCRIPT);
			diveOn = false;
			pos = np;
		}
	}

	if (Has("dist")) {
		const float d = sm["dist"];
		if (std::abs(d - curDist) > curDist * 0.005f) {
			curU = tgtU = UOf(d);
			diveOn = false;
		}
	}

	if (Has("tiltHeld")) {
		const float e = (Has("rx")? sm["rx"]: rotX) - math::HALFPI;
		heldElev = (sm["tiltHeld"] > 0.0f)? std::clamp(e, kcam::ELEV_MIN, kcam::ELEV_MAX): -1.0f;
	} else if (Has("rx") && std::abs(sm["rx"] - rotX) > 0.005f) {
		const float e = sm["rx"] - math::HALFPI;
		heldElev = (std::abs(e - ElevCurve(curU)) > kcam::RELEASE_BAND)? std::clamp(e, kcam::ELEV_MIN, kcam::ELEV_MAX): -1.0f;
	}

	if (Has("zoomTo")) {
		tgtU = UOf(sm["zoomTo"]);
		diveOn = false;
		if (glideScale <= 0.0f)
			curU = tgtU;
	}
	if (Has("wheel"))
		Notches(sm["wheel"], true);
	if (Has("zoomKey"))
		Notches(sm["zoomKey"], false);
	if (Has("tilt"))
		Tilt(sm["tilt"] * kcam::TILT_STEP);
	if (Has("panR") || Has("panF")) {
		const float viewH = 2.0f * curDist * std::tan(fov * 0.5f * math::DEG_TO_RAD);
		Pan((Has("panR")? sm["panR"]: 0.0f) * viewH, (Has("panF")? sm["panF"]: 0.0f) * viewH);
	}
	if (Has("rotate"))
		rotY += sm["rotate"];
	if (Has("reset"))
		ResetView();
	if (Has("pop"))
		ZoomPop();

	if (Has("track")) {
		const int id = int(sm["track"]);
		if (id > 0) {
			StopTrack(END_KEY);
			StartTrack({ id }, TRACK_FOLLOW);
		} else if (id == 0) {
			StopTrack(END_KEY);
		} else {
			ToggleTrack();
		}
	}

	if (Has("commander")) {
		lockUnit = int(sm["commander"]);
		if (lockUnit >= 0 && IsSelected(lockUnit) && !(trackMode == TRACK_LOCK && trackUnit == lockUnit)) {
			StopTrack(END_KEY);
			StartTrack({ lockUnit }, TRACK_LOCK);
		}
	}

	return true;
}
