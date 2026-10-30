/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

// supcom: our own camera controller (Kardashev Type One), beside the stock CSpringController. Chosen with springsetting
// KardashevCamera (default 1); 0 starts the stock spring camera exactly as in Recoil. Display only: it reads units and
// the selection but changes nothing in the simulation.

#ifndef _KARDASHEV_CAMERA_H
#define _KARDASHEV_CAMERA_H

#include "CameraController.h"
#include "System/float4.h"
#include "System/MathConstants.h"

class CUnit;

class CKardashevCamera : public CCameraController
{
public:
	CKardashevCamera();
	~CKardashevCamera();

	const std::string GetName() const override { return "kardashev"; }

	void KeyMove(float3 move) override;
	void MouseMove(float3 move) override;
	void ScreenEdgeMove(float3 move) override;
	void MouseWheelMove(float move) override;
	void MouseWheelMove(float move, const float3& newDir) override { MouseWheelMove(move); }

	void Update() override;
	float3 GetPos() const override;
	float3 GetRot() const override { return {rotX, rotY, 0.0f}; }
	void SetPos(const float3& newPos) override;
	void SetRot(const float3& newRot) override;
	void SetDir(const float3& newDir) override;
	bool DisableTrackingByKey() override { return false; }
	void SetTrackingInfo(const float3& pos, float radius) override {}

	float3 SwitchFrom() const override { return GetPos(); }
	void SwitchTo(const CCameraController* oldCam, const bool showText) override;

	void GetState(StateMap& sm) const override;
	bool SetState(const StateMap& sm) override;

	void ConfigNotify(const std::string& key, const std::string& value);
	void ConfigUpdate();

	// the engine's track action (CUnitTracker) while this controller is active: follow the first selected unit, or stop
	void ToggleTrack();

	enum TrackMode { TRACK_NONE = 0, TRACK_FOLLOW = 1, TRACK_LOCK = 2 };

private:
	struct Limits { float dMin = 100.0f; float dMax = 5000.0f; };

	// the proportions and pacing the game tunes (Lua SetCameraState settings of the same names, angles in degrees there).
	// DESIGN (2026-10-30, camera feel pass 2): defaults are the game's values (code/LuaUI/config/camera.lua), so a
	// game that sends none gets the same camera
	struct Feel {
		float minDist    = 32.0f;                        // closest view (elmos focus to camera): one unit large and readable
		float margin     = 1.04f;                        // free border round the playable area at the furthest view (4 %)
		float elevNear   = 30.0f * math::DEG_TO_RAD;     // close: low and oblique, horizon in frame
		float elevFar    = 89.0f * math::DEG_TO_RAD;     // far: practically straight down, a flat plan
		float fovNear    = 64.0f;                        // vertical fov close (degrees): wide
		float fovFar     = 60.0f;                        // vertical fov far: close to the near one, no long-lens look
		float shape      = 0.35f;                        // eases the curve's ends; slope there 1 - shape (never 0)
		float centreFrom = 0.45f;                        // above this u the framing box round the middle starts to shrink
		float stepOut    = 0.50f;                        // a notch out adds 50 % of the distance; in removes 33 %
		float tau        = 0.22f;                        // glide time constant (s): fast start, long soft landing
		float keyPan     = 2.0f;                         // keys and edge: view heights per second
		float keyFast    = 4.0f;                         // the fast modifier on the pan keys
		float dragPan    = 0.5f;                         // middle-drag: view pixels per pointer pixel
		float clearance  = 8.0f;                         // camera and its line to the focus stay this far above the ground
		bool freeRotate  = false;                        // the engine's rotate and tilt modifiers turn and tilt (an option; the
		                                                 // game's own look key turns through Lua either way)
	};

	float DistOf(float u) const;
	float UOf(float d) const;
	float ElevCurve(float u) const;
	float FovCurve(float u) const;
	float Elev() const;
	float Aspect() const;
	float3 Heading() const;
	float CentreWeight(float u) const;

	void Refit();
	void Notches(float n);
	void Tilt(float dElev);
	void Pan(float dRight, float dFwd);
	void ResetView();
	void ZoomPop();
	void SetAnchor();

	void StartTrack(int unitID, int mode);
	void StopTrack(int reason);
	const CUnit* TrackedUnit() const;
	bool TrackFocus(float dt, float3& focus);

	float3 ClampFocus(float3 p, float u) const;
	float ClearElev(const float3& f, float d, float e) const;

private:
	float rotX = 2.677f;          // pitch as the engine reads it (pi/2 + elevation below the horizon)
	float rotY = 0.0f;            // heading
	float curU = 0.5f;            // zoom position 0 (closest) .. 1 (furthest), log distance
	float tgtU = 0.5f;            // where the wheel glide goes (notches accumulate here)
	float curDist = 1000.0f;
	float heldElev = -1.0f;       // manual tilt (elevation), < 0: the zoom curve owns the angle
	float glideScale = 1.0f;      // 0: every zoom is instant (reduced motion)

	Feel feel;
	Limits lim;
	float fitAspect = 0.0f;
	float4 playRect;              // x0, z0, x1, z1 in elmos
	float4 clearIns;              // screen insets the furthest view keeps the map clear of (left, right, top, bottom; 0..1)
	bool refitNeeded = true;

	// zoom-out anchor: the ground point under the pointer stays on its screen spot
	bool anchorOn = false;
	float3 anchorG;
	float anchorSX = 0.0f, anchorSY = 0.0f, anchorC0 = 0.0f;
	float3 anchorOff;

	// tracking and the lock
	int trackUnit = -1;
	int trackMode = TRACK_NONE;
	int lastEnd = 0;              // why the last track ended (state key "trackEnd")
	float3 trackOff;              // focus off the unit, easing to 0
	float3 trackLast;
	int lockUnit = -1;            // the unit the lock arms on while selected (the player's commander; set by Lua)
	bool lockWasSelected = false;

	float lastTick = -1.0f;

	// player options (springsettings)
	float wheelSens = 1.0f;
	float panSpeed = 1.0f;
	float rotSpeed = 1.0f;
	bool invertPan = false;
	float scrollWheelSpeed = 25.0f;
};

#endif // _KARDASHEV_CAMERA_H
