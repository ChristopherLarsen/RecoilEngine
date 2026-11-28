/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: FrameState, one copy per drawn frame of what the remaining widgets poll for themselves: the selection, what is
// under the pointer, the camera, the zoom level and the mouse, each with a change counter (LUA-FREE H01, FRAME §4.1;
// step V01 writes the bodies). It adds to Draw/Common/FrameFacts.h, which fills first; system "frame" is the
// first draw Update system, so later systems and every widget of the frame read the same copy.
// Native readers: FrameServices::State(). Lua: Spring.KT1.Frame (FrameBridge.h).

#ifndef KT1_DRAW_FRAME_STATE_H
#define KT1_DRAW_FRAME_STATE_H

#include <cstdint>
#include <vector>

#include "Game/KT1/Core/ViewSettings/FrameChanges.h"
#include "Game/KT1/Draw/Common/FrameFacts.h"
#include "System/float3.h"

namespace KT1::FrameServices {

struct CameraState {
	float3 pos, forward, right, up;
	float distance = -1.0f;      // camera-to-focus, elmos (Spring.GetCameraState().dist); -1: this camera mode has none
	float height = 0.0f;         // the camera state's height field, or the camera's height
	float pitch = 0.0f;          // rx, radians
	float yaw = 0.0f;            // ry
	float fov = 0.0f;            // vertical, degrees
	float minDistance = -1.0f;   // the controller's zoom limits (dMin, dMax); -1: it gives none
	float maxDistance = -1.0f;
	int mode = 0;                // the engine's camera controller index (its name goes to Lua)
	int viewW = 0, viewH = 0;
};

struct MouseState {
	int x = 0, y = 0;            // view pixels, y from the bottom
	bool left = false, middle = false, right = false;
	bool offscreen = false;
	bool overMiniMap = false;
	bool alt = false, ctrl = false, meta = false, shift = false;
};

// The two ray variants the widgets use most come from one DrawView::Trace: `pointer.hit` holds the unit or feature under
// the pointer and the ground point (also under an object); the view's centre and the minimap point are added.
struct HoverState {
	PointerFacts pointer;
	bool onMiniMapGround = false;   // the pointer is over the minimap: the world point it shows
	float3 miniMapGround;
	bool centreOnGround = false;    // the ray through the middle of the view
	float3 centreGround;
};

struct FrameState {
	uint32_t frame = 0;             // the drawn frame these are of
	std::vector<int> selection;     // ascending unit ids
	HoverState hover;
	CameraState cam;
	MouseState mouse;
	int zoomLevel = FrameFacts::ZOOM_UNKNOWN;
	float zoomDist = -1.0f;
	bool wholeMap = false;
	bool valid = false;
};

const FrameState& State();                          // the last filled frame
uint32_t Changes(Core::View::FrameWhat what);       // moves when that part differs from the frame before

} // namespace KT1::FrameServices

#endif
