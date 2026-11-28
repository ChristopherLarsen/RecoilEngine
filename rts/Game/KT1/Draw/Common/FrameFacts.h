/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: FrameFacts, what every draw system needs to know about the drawn frame, worked out once per frame instead of
// once per system (game-build docs/engine/LUA-FREE-PLAN.md 2.6): the camera, the zoom level, how far the frame is into
// the sim step, what is under the pointer (one ray), the selection with a change counter, and the units in view.
//
// The draw host fills it at DrawGenesis, after the camera and the drawn unit positions of the frame are set. Systems in
// the Update phase therefore read the previous frame's facts.

#ifndef KT1_DRAW_FRAMEFACTS_H
#define KT1_DRAW_FRAMEFACTS_H

#include <cstdint>
#include <vector>

#include "Game/KT1/Core/DrawHost/IdBits.h"
#include "Game/KT1/Core/DrawHost/SelectionTracker.h"
#include "Game/KT1/Host/DrawView.h"
#include "System/Matrix44f.h"
#include "System/float3.h"

namespace KT1 {

struct CameraFacts {
	float3 pos, forward, right, up;
	CMatrix44f viewProj;
	int viewW = 0, viewH = 0;
};

struct PointerFacts {
	int x = 0, y = 0;           // view pixels, y from the bottom (as Spring.GetMouseState)
	bool overMiniMap = false;   // the hit below is still the world ray's
	RayHit hit;                 // the hovered unit, feature or ground
};

class FrameFacts {
public:
	static constexpr int ZOOM_UNKNOWN = -1;

	const CameraFacts& Camera() const { return camFacts; }
	int ZoomLevel() const { return zoomLevel; }              // 0 (closest) .. 7 (SupComZoomLevel); ZOOM_UNKNOWN without it
	float ZoomDist() const { return zoomDist; }              // camera zoom distance, elmos; -1 when unknown
	int Step() const { return step; }                        // sim step number
	float StepFraction() const { return stepFraction; }      // how far past that step the drawn frame is, in steps
	const PointerFacts& Pointer() const { return pointer; }
	const std::vector<int>& Selected() const { return selection.Ids(); }        // ascending
	bool IsSelected(int id) const { return selection.Contains(id); }
	uint32_t SelectionChanges() const { return selection.Changes(); }
	const std::vector<int>& VisibleUnits() const { return visible; }            // in the view and seen, ascending
	bool IsVisible(int id) const { return visibleBits.Test(id); }
	uint32_t Frame() const { return frame; }                 // the drawn frame these are of
	bool Valid() const { return valid; }

	// host: fills everything for drawn frame `drawFrame`; a second call for the same frame does nothing
	void Refresh(uint32_t drawFrame, const DrawView& view);
	void Reset();
	// tests: read the pointer at this view pixel instead of the mouse; ClearPointer returns to the mouse
	void SetPointer(int x, int y) { forced = true; forcedX = x; forcedY = y; }
	void ClearPointer() { forced = false; }

private:
	void ReadCamera();
	void ReadZoom();
	void ReadPointer(const DrawView& view);
	void ReadVisible(const DrawView& view);

	CameraFacts camFacts;   // not "camera": Game/Camera.h defines camera as a macro
	PointerFacts pointer;
	SelectionTracker selection;
	IdBits visibleBits;
	std::vector<int> visible;
	int zoomLevel = ZOOM_UNKNOWN;
	float zoomDist = -1.0f;
	int step = 0;
	float stepFraction = 0.0f;
	uint32_t frame = 0;
	bool valid = false;
	bool forced = false;
	int forcedX = 0, forcedY = 0;
};

} // namespace KT1

#endif
