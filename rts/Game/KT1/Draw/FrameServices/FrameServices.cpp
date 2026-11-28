/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: filling FrameState (FrameState.h, FrameRefresh.h; LUA-FREE V01). Nothing runs on its own: a part is read when
// a reader asks for it, a ray is cast again only when its key moved (Core/ViewSettings/FrameLatch.h). Units only through
// DrawView. Reads the engine, writes nothing.

#include "FrameRefresh.h"

#include <SDL_keycode.h>
#include <SDL_mouse.h>

#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/Camera/CameraController.h"
#include "Game/KT1/Core/DrawHost/SelectionTracker.h"
#include "Game/KT1/Core/ViewSettings/FrameLatch.h"
#include "Game/SelectedUnitsHandler.h"
#include "Game/UI/MiniMap.h"
#include "Game/UI/MouseHandler.h"
#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/SupComZoomLevel.h"
#include "System/Input/KeyInput.h"

namespace KT1::FrameServices {
namespace {

namespace View = Core::View;
using View::FrameWhat;

struct Services {
	FrameState state;
	View::FrameChanges changes;
	DrawView view;
	SelectionTracker selection;
	int selectionCapacity = -1;
	View::FrameLatch<View::RayKey> pointerRay, centreRay;
	std::string cameraName;
	std::vector<std::pair<std::string, float>> cameraValues;
};

Services& Get() {
	static Services services;
	return services;
}

const FrameState& Stamped(Services& s) {
	s.state.frame = globalRendering->drawFrame;
	s.state.valid = true;
	return s.state;
}

std::array<int, 4> MiniMapRect() {
	if (minimap == nullptr || minimap->GetMinimized()) return {};
	return {minimap->GetPosX() - globalRendering->viewPosX, minimap->GetPosY() - globalRendering->viewPosY,
	        minimap->GetSizeX(), minimap->GetSizeY()};
}

View::RayKey KeyFor(const Services& s, int x, int y) {
	const CCamera* cam = CCamera::GetActive();
	const float3 pos = cam->GetPos(), dir = cam->GetDir();
	View::RayKey k;
	k.drawFrame = globalRendering->drawFrame;
	k.step = s.view.Step();
	k.x = x;
	k.y = y;
	k.view = {globalRendering->viewPosX, globalRendering->viewPosY, globalRendering->viewSizeX, globalRendering->viewSizeY};
	k.cam = {pos.x, pos.y, pos.z, dir.x, dir.y, dir.z, cam->GetVFOV()};
	k.miniMap = MiniMapRect();
	return k;
}

bool SameHit(const RayHit& a, const RayHit& b) {
	return a.kind == b.kind && a.unitId == b.unitId && a.featureId == b.featureId && a.hasGround == b.hasGround &&
	       (!a.hasGround || a.ground == b.ground);
}

bool SameMouse(const MouseState& a, const MouseState& b) {
	return a.x == b.x && a.y == b.y && a.left == b.left && a.middle == b.middle && a.right == b.right &&
	       a.offscreen == b.offscreen && a.overMiniMap == b.overMiniMap && a.alt == b.alt && a.ctrl == b.ctrl &&
	       a.meta == b.meta && a.shift == b.shift;
}

bool SameCamera(const CameraState& a, const CameraState& b) {
	return a.pos == b.pos && a.forward == b.forward && a.up == b.up && a.distance == b.distance && a.height == b.height &&
	       a.pitch == b.pitch && a.yaw == b.yaw && a.fov == b.fov && a.minDistance == b.minDistance &&
	       a.maxDistance == b.maxDistance && a.mode == b.mode && a.viewW == b.viewW && a.viewH == b.viewH;
}

// copies the controller's state into `held`; true when a value differs from the one held
bool TakeValues(CCameraController::StateMap& map, std::vector<std::pair<std::string, float>>& held) {
	size_t n = 0;
	bool differs = false;
	for (const auto& entry : map) {
		if (n >= held.size()) {
			held.emplace_back(std::string(entry.first.c_str()), entry.second);
			differs = true;
		} else if (held[n].first != entry.first.c_str() || held[n].second != entry.second) {
			held[n].first = entry.first.c_str();
			held[n].second = entry.second;
			differs = true;
		}
		++n;
	}
	if (held.size() != n) {
		held.resize(n);
		differs = true;
	}
	return differs;
}

float ValueOr(const std::vector<std::pair<std::string, float>>& values, const char* name, float fallback) {
	for (const auto& v : values) {
		if (v.first == name) return v.second;
	}
	return fallback;
}

bool WholeMapInView() {
	const float x = static_cast<float>(mapDims.mapx * SQUARE_SIZE), z = static_cast<float>(mapDims.mapy * SQUARE_SIZE);
	const CCamera* cam = CCamera::GetActive();
	return cam->InView(float3(0, 0, 0)) && cam->InView(float3(x, 0, 0)) && cam->InView(float3(0, 0, z)) &&
	       cam->InView(float3(x, 0, z));
}

} // namespace

const FrameState& Selection() {
	Services& s = Get();
	const int capacity = s.view.UnitCapacity();
	if (capacity != s.selectionCapacity) {
		s.selection.Init(capacity);
		s.selectionCapacity = capacity;
		s.state.selection.clear();
	}
	const bool differs = s.selection.Update(selectedUnitsHandler.selectedUnits);
	if (differs) s.state.selection = s.selection.Ids();
	s.changes.Note(FrameWhat::Selection, differs);
	return Stamped(s);
}

bool IsSelected(int id) {
	Selection();
	const Services& s = Get();
	return id >= 0 && id < s.selectionCapacity && s.selection.Contains(id);
}

const FrameState& Mouse() {
	Services& s = Get();
	MouseState m;
	m.x = mouse->lastx - globalRendering->viewPosX;
	m.y = globalRendering->viewSizeY - mouse->lasty - 1;
	m.left = mouse->buttons[SDL_BUTTON_LEFT].pressed;
	m.middle = mouse->buttons[SDL_BUTTON_MIDDLE].pressed;
	m.right = mouse->buttons[SDL_BUTTON_RIGHT].pressed;
	m.offscreen = mouse->offscreen;
	m.overMiniMap = minimap != nullptr && !minimap->GetMinimized() &&
	                minimap->IsInside(mouse->lastx, mouse->lasty);
	m.alt = KeyInput::GetKeyModState(KMOD_ALT);
	m.ctrl = KeyInput::GetKeyModState(KMOD_CTRL);
	m.meta = KeyInput::GetKeyModState(KMOD_GUI);
	m.shift = KeyInput::GetKeyModState(KMOD_SHIFT);
	s.changes.Note(FrameWhat::Mouse, !SameMouse(m, s.state.mouse));
	s.state.mouse = m;
	return Stamped(s);
}

const FrameState& Pointer() {
	Services& s = Get();
	const MouseState m = Mouse().mouse;
	const View::RayKey key = KeyFor(s, m.x, m.y);
	if (!s.pointerRay.Moved(key)) return Stamped(s);

	HoverState& h = s.state.hover;
	const RayHit hit = s.view.Trace(m.x, m.y);
	// Spring.TraceScreenRay's minimap rectangle and point (useMiniMap)
	const auto [px, py, sx, sy] = key.miniMap;
	const bool onMini = m.x >= px && m.x < px + sx && m.y >= py && m.y < py + sy;
	float3 miniGround;
	if (onMini) {
		miniGround = minimap->GetMapPosition(m.x + globalRendering->viewPosX, globalRendering->viewSizeY - 1 - m.y);
		miniGround.y = CGround::GetHeightReal(miniGround.x, miniGround.z, false);
	}
	const bool differs = !SameHit(hit, h.pointer.hit) || onMini != h.onMiniMapGround ||
	                     (onMini && !(miniGround == h.miniMapGround));
	h.pointer.x = m.x;
	h.pointer.y = m.y;
	h.pointer.overMiniMap = m.overMiniMap;
	h.pointer.hit = hit;
	h.onMiniMapGround = onMini;
	h.miniMapGround = miniGround;
	s.changes.Note(FrameWhat::Hover, differs);
	return Stamped(s);
}

const FrameState& Centre() {
	Services& s = Get();
	const int x = globalRendering->viewSizeX / 2, y = globalRendering->viewSizeY / 2;   // DESIGN: the middle of the view
	if (s.centreRay.Moved(KeyFor(s, x, y))) {
		const RayHit hit = s.view.Trace(x, y);
		s.state.hover.centreOnGround = hit.hasGround;
		s.state.hover.centreGround = hit.ground;
	}
	return Stamped(s);
}

const FrameState& Camera() {
	Services& s = Get();
	const CCamera* active = CCamera::GetActive();
	CCameraController::StateMap map;
	camHandler->GetState(map);
	const bool valuesDiffer = TakeValues(map, s.cameraValues);
	const std::string name = camHandler->GetCurrentController().GetName();

	CameraState c;
	c.pos = active->GetPos();
	c.forward = active->GetDir();
	c.right = active->GetRight();
	c.up = active->GetUp();
	c.distance = camHandler->GetCurrentController().GetSupComDist();
	c.height = ValueOr(s.cameraValues, "height", c.pos.y);
	c.pitch = ValueOr(s.cameraValues, "rx", 0.0f);
	c.yaw = ValueOr(s.cameraValues, "ry", 0.0f);
	c.fov = active->GetVFOV();
	c.minDistance = ValueOr(s.cameraValues, "dMin", -1.0f);
	c.maxDistance = ValueOr(s.cameraValues, "dMax", -1.0f);
	c.mode = static_cast<int>(camHandler->GetCurrentControllerNum());
	c.viewW = globalRendering->viewSizeX;
	c.viewH = globalRendering->viewSizeY;
	s.changes.Note(FrameWhat::Camera, valuesDiffer || name != s.cameraName || !SameCamera(c, s.state.cam));
	s.cameraName = name;
	s.state.cam = c;
	return Stamped(s);
}

const FrameState& Zoom() {
	Services& s = Get();
	const int level = SupComZoomLevel::Get();
	const float dist = SupComZoomLevel::Dist();
	const bool whole = WholeMapInView();
	s.changes.Note(FrameWhat::Zoom, level != s.state.zoomLevel || dist != s.state.zoomDist || whole != s.state.wholeMap);
	s.state.zoomLevel = level;
	s.state.zoomDist = dist;
	s.state.wholeMap = whole;
	return Stamped(s);
}

const std::string& CameraName() { return Get().cameraName; }
const std::vector<std::pair<std::string, float>>& CameraValues() { return Get().cameraValues; }

void Reset() { Get() = Services{}; }

const FrameState& State() {
	Selection();
	Pointer();
	Centre();
	Camera();
	return Zoom();
}

uint32_t Changes(Core::View::FrameWhat what) {
	switch (what) {
		case FrameWhat::Selection: Selection(); break;
		case FrameWhat::Hover: Pointer(); break;
		case FrameWhat::Camera: Camera(); break;
		case FrameWhat::Zoom: Zoom(); break;
		case FrameWhat::Mouse: Mouse(); break;
		default: break;
	}
	return Get().changes.Count(what);
}

} // namespace KT1::FrameServices
