/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the running icon set behind IconSet.h (LUA-FREE D01). Made by the system "Icons" at Start and owned by it;
// IconSet.cpp keeps it up to date, IconQuery.cpp answers the HUD and the minimap from it. Private to this folder.

#ifndef KT1_DRAW_ICON_RUNTIME_H
#define KT1_DRAW_ICON_RUNTIME_H

#include <cstdint>
#include <string>
#include <vector>

#include "IconSet.h"

#include "Game/KT1/Draw/Common/DrawFeed.h"
#include "Game/KT1/Host/DrawPhases.h"
#include "System/Misc/SpringTime.h"
#include "System/float3.h"

namespace KT1::Icons {

namespace IL = Core::IconLexicon;

struct Runtime {
	IL::IconConfig config;
	IL::IconState state;
	IL::IconParams params;
	IL::HudOptions options;
	std::vector<IL::IconClass> classes;      // by def id; 0 is no def
	std::vector<std::string> engineIcons;    // by def id: the engine icon type before the transparent one was put on
	std::vector<IL::Paint> paints;           // by team id
	DrawView view;                           // for the reads asked between frames (HUD); it keeps nothing of a frame
	ViewerView viewer;

	std::vector<IL::IconUnitFacts> facts;    // scratch: a rebuild's units
	std::vector<int32_t> scratch;            // scratch: the contacts' units
	std::vector<IL::IconRect> rects;         // the drawn boxes, as of rectsFrame and rectsChanges
	uint32_t rectsFrame = 0, rectsChanges = 0;
	bool rectsValid = false;

	uint32_t selectionChanges = 0;
	int age = 0;                             // drawn frames since the last rebuild
	int hoverAge = 0;                        // drawn frames since the last hover read
	int pointerX = 0, pointerY = 0;
	float3 camPos, camForward;
	float switchDistance = -1.0f;
	bool swapped = false;                    // the engine icons are the transparent type
	bool paused = false;
	bool failed = false;                     // the shader did not build
	std::string why;
	spring_time epoch;

	void Init(const IL::IconConfig& with);   // def classes and the set (Start, and a reloaded config)
	void Update(const DrawContext& ctx);     // once per drawn frame, before the draw
	void OnEvent(const DrawFeed::Event& event, bool running);
	void SwapEngineIcons();
	float Seconds() const { return (spring_gettime() - epoch).toSecsf(); }

	// what the set reads of one unit; false: dead or not seen (out then names the unit only, and it shows nothing)
	bool ReadUnit(const DrawView& from, const ViewerView& who, int id, IL::IconUnitFacts& out) const;
	IL::Relation RelationOf(const DrawView& from, const ViewerView& who, int team) const;
	IL::Rgb TeamFill(const DrawView& from, int team) const;
	const IL::IconClass* Class(int defId) const;

private:
	void ReadViewer(const DrawView& from);
	void ReadParams(const FrameFacts& frame);
	void ReadPaints(const DrawView& from);
	void Hover(const FrameFacts& frame);
	void Apply(const DrawContext& ctx);
	void ApplyContacts(const DrawContext& ctx);
	bool ApplyUnit(const DrawContext& ctx, int id);
	void Rebuild(const DrawContext& ctx);
};

Runtime* Current();                          // null before Start and in a game without the system
void SetCurrent(Runtime* runtime);

} // namespace KT1::Icons

#endif
