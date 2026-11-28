/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: filling FrameFacts (see FrameFacts.h). Units only through DrawView.

#include "FrameFacts.h"

#include "Game/Camera.h"
#include "Game/GlobalUnsynced.h"
#include "Game/SelectedUnitsHandler.h"
#include "Game/UI/MiniMap.h"
#include "Game/UI/MouseHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/SupComZoomLevel.h"

namespace KT1 {

void FrameFacts::Reset() {
	selection.Init(0);
	visibleBits.Init(0);
	visible.clear();
	pointer = PointerFacts{};
	camera = CameraFacts{};
	zoomLevel = 0;
	zoomDist = stepFraction = 0.0f;
	step = 0;
	frame = 0;
	valid = false;
	forced = false;
}

void FrameFacts::Refresh(uint32_t drawFrame, const DrawView& view) {
	if (valid && frame == drawFrame) return;
	const int capacity = view.UnitCapacity();
	if (visibleBits.Capacity() != capacity) {   // the first frame of a game
		selection.Init(capacity);
		visibleBits.Init(capacity);
		visible.reserve(capacity);
	}
	frame = drawFrame;
	valid = true;

	ReadCamera();
	zoomLevel = SupComZoomLevel::Get();
	zoomDist = SupComZoomLevel::Dist();
	step = view.Step();
	stepFraction = globalRendering->timeOffset;
	ReadPointer(view);
	selection.Update(selectedUnitsHandler.selectedUnits);
	ReadVisible(view);
}

void FrameFacts::ReadCamera() {
	camera.pos = ::camera->GetPos();
	camera.forward = ::camera->GetDir();
	camera.right = ::camera->GetRight();
	camera.up = ::camera->GetUp();
	camera.viewProj = ::camera->GetViewProjectionMatrix();
	camera.viewW = globalRendering->viewSizeX;
	camera.viewH = globalRendering->viewSizeY;
}

void FrameFacts::ReadPointer(const DrawView& view) {
	pointer.x = forced ? forcedX : (mouse->lastx - globalRendering->viewPosX);
	pointer.y = forced ? forcedY : (globalRendering->viewSizeY - mouse->lasty - 1);
	const int wx = pointer.x + globalRendering->viewPosX;
	const int wy = globalRendering->viewSizeY - 1 - pointer.y;
	pointer.overMiniMap = minimap != nullptr && !minimap->GetMinimized() && minimap->IsInside(wx, wy);
	pointer.hit = view.Trace(pointer.x, pointer.y);
}

void FrameFacts::ReadVisible(const DrawView& view) {   // KT1 hot
	visibleBits.Clear();
	UnitView u;
	for (const int id : view.UnitIds()) {
		if (!view.Unit(id, u) || u.hidden || !u.inLos) continue;
		if (::camera->InView(u.drawMidPos, u.drawRadius)) visibleBits.Set(id);
	}
	visible.clear();
	visibleBits.AppendTo(visible);
}

} // namespace KT1
