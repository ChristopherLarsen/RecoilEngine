/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ViewSettings (LUA-FREE D07): the ported rules against the values the Lua computed.

#include <cmath>
#include <vector>

#include "Game/KT1/Core/ViewSettings/ViewCullState.h"
#include "Game/KT1/Core/ViewSettings/ViewSceneState.h"
#include "Game/KT1/Core/ViewSettings/ViewSettingsState.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::View;

namespace {
bool Near(float a, float b, float eps = 0.001f) { return std::fabs(a - b) <= eps; }

ViewRow Row(const char* gate, const char* unit, float from, float to) {
	ViewRow r;
	r.gate = gate;
	r.unit = unit;
	r.from = from;
	r.to = to;
	return r;
}
}

KT1_CASE("VIEW-LEVEL", "the level at each bound, the band for one level, none at the icon switch") {
	const ZoomBounds b;
	KT1_CHECK_EQ(RawLevel(b, 449.9f), 0);
	KT1_CHECK_EQ(RawLevel(b, 450), 1);
	KT1_CHECK_EQ(LevelFor(b, 460, 0, false), 0);                   // inside 5 percent
	KT1_CHECK_EQ(LevelFor(b, 473, 0, false), 1);
	KT1_CHECK_EQ(LevelFor(b, 440, 1, false), 1);
	KT1_CHECK_EQ(LevelFor(b, 427, 1, false), 0);
	KT1_CHECK_EQ(LevelFor(b, 1040, 3, false), 4);                  // the icon switch: one step
	KT1_CHECK_EQ(LevelFor(b, 1039, 4, false), 3);
	KT1_CHECK_EQ(LevelFor(b, 5000, 0, false), 6);                  // a jump takes no band
	KT1_CHECK_EQ(LevelFor(b, 100, 3, true), 7);
	KT1_CHECK_EQ(LevelFor(b, 460, ZOOM_NONE, false), 1);
}

KT1_CASE("VIEW-DETAIL", "the detail option scales by the ratio of bracket midpoints") {
	const DetailConfig d;
	KT1_CHECK_EQ(DetailIndex(d, 30), 0);
	KT1_CHECK_EQ(DetailIndex(d, 60), 1);
	KT1_CHECK_EQ(DetailIndex(d, 120), 2);
	KT1_CHECK(DetailFactor(d, "models", 1) == 1);
	KT1_CHECK(Near(DetailFactor(d, "models", 0), 385.0f / 485.0f));
	KT1_CHECK(Near(DetailFactor(d, "models", 2), 510.0f / 485.0f));
	KT1_CHECK(DetailFactor(d, "plants", 2) == 1);                  // the middle bracket is open
	KT1_CHECK(Near(DetailFactor(d, "plants", 0), 570.0f / 600.0f));
	KT1_CHECK(DetailFactor(d, "effects", 0) == DetailFactor(d, "models_small", 0));
	KT1_CHECK(DetailFactor(d, "nothing", 0) == 1);
}

KT1_CASE("VIEW-GATES", "rows hold by level or distance; a row is written only when its bit moves") {
	ViewSettingsConfig c;
	c.rows = {Row("shadow", "level", 0, 1), Row("far", "elmos", 1000, NO_END)};
	c.rows[0].target = "SupComX";
	c.rows[0].value = 1;
	c.rows[0].write_other = true;
	c.rows[0].other_value = 7;
	c.rows[1].hysteresis = 0.1f;
	ViewState s;
	KT1_CHECK(CompileRows(c, s.table));
	ViewInputs in;
	in.distance = 300;
	std::vector<int> rows;
	KT1_CHECK(UpdateView(s, c, in));
	RowsToWrite(s, c, rows);
	KT1_CHECK_EQ(rows.size(), size_t(1));
	KT1_CHECK_EQ(s.log.Writes().back().value, 1.0f);
	rows.clear();
	KT1_CHECK(!UpdateView(s, c, in));
	RowsToWrite(s, c, rows);
	KT1_CHECK(rows.empty());
	in.distance = 1050;                                           // inside the far row's band: not yet
	UpdateView(s, c, in);
	KT1_CHECK_EQ(HoldingRow(s, c, FindGate(c, "far")), -1);
	RowsToWrite(s, c, rows);                                      // the shadow row stopped holding
	KT1_CHECK_EQ(s.log.Writes().back().value, 7.0f);
	in.distance = 1200;
	UpdateView(s, c, in);
	KT1_CHECK_EQ(HoldingRow(s, c, FindGate(c, "far")), 1);
	in.distance = 950;                                            // leaves late
	UpdateView(s, c, in);
	KT1_CHECK_EQ(HoldingRow(s, c, FindGate(c, "far")), 1);
	KT1_CHECK_EQ(FindGate(c, "none"), -1);
}

KT1_CASE("VIEW-SHADOW", "shadows end at the smaller of setting and cap, with a 2 percent band") {
	const ShadowCutConfig c;
	KT1_CHECK_EQ(ShadowClamp(c, 100), 200);
	KT1_CHECK_EQ(ShadowClamp(c, 1000), 440);
	KT1_CHECK_EQ(ShadowClamp(c, 269), 260);
	KT1_CHECK_EQ(ShadowClamp(c, 271), 280);
	const float cut = ShadowCutZoom(c, 260, true);
	KT1_CHECK(cut == 102.5f);
	KT1_CHECK(ShadowCutZoom(c, 260, false) == 260);
	KT1_CHECK(!ShadowNext(false, cut * 1.019f, cut, c.hysteresis));
	KT1_CHECK(ShadowNext(false, cut * 1.021f, cut, c.hysteresis));
	KT1_CHECK(ShadowNext(true, cut * 0.981f, cut, c.hysteresis));
	KT1_CHECK(!ShadowNext(true, cut * 0.979f, cut, c.hysteresis));
	KT1_CHECK_EQ(int(ShadowMask(c, 255, true)), 255 - 48);
	KT1_CHECK_EQ(int(ShadowMask(c, 207, false)), 255);
	KT1_CHECK_EQ(int(ShadowMask(c, 0, false)), 0);
	IdSweep sweep;
	const std::vector<int32_t> ids = {1, 2, 3, 4, 5};
	StartSweep(sweep, ids, true);
	std::vector<int32_t> out;
	KT1_CHECK(!StepSweep(sweep, 3, out));
	KT1_CHECK(StepSweep(sweep, 3, out));
	KT1_CHECK(out == ids);
}

KT1_CASE("VIEW-MODEL", "removal zoom by class, the detail factor and the 1500 cap") {
	ViewSettingsConfig c;
	KT1_CHECK(ClassOf(c.model_cull, {"x", 2, 2, true}) == ModelClass::Lead);
	KT1_CHECK(ClassOf(c.model_cull, {"a_cmd_b", 6, 6, false}) == ModelClass::Lead);
	KT1_CHECK(ClassOf(c.model_cull, {"x", 2, 6, false}) == ModelClass::Small);
	KT1_CHECK(ClassOf(c.model_cull, {"x", 6, 6, false}) == ModelClass::Large);
	KT1_CHECK(CullZoom(c, ModelClass::Large, 1) == 1500);
	KT1_CHECK(CullZoom(c, ModelClass::Small, 0) < 1500);
	c.model_cull.early = false;
	KT1_CHECK(CullZoom(c, ModelClass::Lead, 1) == 236.5f * 8);
	KT1_CHECK(CullZoom(c, ModelClass::Small, 1) == 1760);
	KT1_CHECK(CullZoom(c, ModelClass::Large, 1) == 3680);
}

KT1_CASE("VIEW-WRECK", "a wreck takes the farthest zoom of its units; an experimental's is never cut") {
	WreckZoom w;
	MergeWreckZoom(w, 900, false);
	MergeWreckZoom(w, 1200, false);
	MergeWreckZoom(w, 900, false);
	KT1_CHECK(w.zoom == 1200);
	KT1_CHECK(!w.never);
	MergeWreckZoom(w, 0, true);
	MergeWreckZoom(w, 900, false);
	KT1_CHECK(w.never);
}

KT1_CASE("VIEW-CULL", "a set is switched at its zoom; a new object takes its set's state") {
	CullState s;
	s.Init(16);
	std::vector<MaskWrite> out;
	AddCullable(s, 1, 1500, out);
	AddCullable(s, 2, 900, out);
	StepCull(s, 899, out);
	KT1_CHECK(out.empty());
	StepCull(s, 1500, out);
	KT1_CHECK_EQ(out.size(), size_t(2));
	out.clear();
	AddCullable(s, 3, 1500, out);                                 // arrives culled
	KT1_CHECK_EQ(out.size(), size_t(1));
	KT1_CHECK(out[0].hide);
	out.clear();
	RemoveCullable(s, 1);
	RestoreCull(s, out);
	KT1_CHECK_EQ(out.size(), size_t(2));                          // the removed one is not written
	KT1_CHECK(!out[0].hide);
}

KT1_CASE("VIEW-REFLECT", "reflections fade with the tank's screen length, in 32 steps") {
	const ZoomSwitchConfig c;
	KT1_CHECK(ReflectionStrength(c, 200, 1, 1080, 45) == 1);
	KT1_CHECK(ReflectionStrength(c, 5000, 1, 1080, 45) == 0);
	const float s = ReflectionStrength(c, 1500, 1, 1080, 45);
	KT1_CHECK(s > 0);
	KT1_CHECK(s < 1);
	KT1_CHECK(Near(s * 32, std::floor(s * 32 + 0.5f)));
	KT1_CHECK(ReflectionStrength(c, 1500, 0.5f, 1080, 45) < s);    // 50 percent switches off nearer
	KT1_CHECK(Near(TankPixels(DistanceForPixels(12, 1080, 45, 19.3f), 1080, 45, 19.3f), 12));
	KT1_CHECK(ZoomSwitchLevel(c, 25) == 0.25f);
	KT1_CHECK(ZoomSwitchLevel(c, 0.5f) == 0.5f);
	KT1_CHECK(ZoomSwitchLevel(c, 70) == 0.5f);
}

KT1_CASE("VIEW-PROPCUT", "a kind's last distance, scaled, with the play-zoom floor; its size class") {
	const PropCullConfig c;
	const auto prop = [](const char* cls, float sideUnits, bool wreck = false) {
		PropFacts f;
		f.propClass = cls;
		f.side = sideUnits * 8;
		f.wreck = wreck;
		return f;
	};
	KT1_CHECK(PropCut(c, prop("knockable_single", 0), 0.5f) == 2560);
	KT1_CHECK(PropCut(c, prop("boulder_small", 0), 0.5f) == 560);      // under the floor of 700: its own distance holds
	KT1_CHECK(PropCut(c, prop("boulder_medium", 0), 0.5f) == 1040);
	KT1_CHECK(PropCut(c, prop("tree", 9), 0.5f) == 2560);              // longest kinds stop at the plants'
	KT1_CHECK(PropCut(c, prop("tree", 9), 0) == 0);
	KT1_CHECK(PropCut(c, prop("", 1), 0.5f) == NO_END);
	KT1_CHECK(PropCut(c, prop("tree", 1, true), 0.5f) == NO_END);
	PropFacts decor = prop("", 1, true);
	decor.decor = true;
	KT1_CHECK(PropCut(c, decor, 0.5f) == 2560);
	KT1_CHECK_EQ(PropSize(c, prop("knockable_cluster", 9)), 0);
	KT1_CHECK_EQ(PropSize(c, prop("tree", 9)), 2);
	KT1_CHECK_EQ(PropSize(c, prop("tree", 1.0f)), 1);
}

KT1_CASE("VIEW-PROPS", "cells switch whole or prop by prop; own distance and zoom gate combine") {
	const ViewSettingsConfig c;
	PropState s;
	s.Init(16);
	std::vector<MaskWrite> out;
	PropFacts plant;
	plant.propClass = "knockable_single";
	plant.x = 100;
	plant.z = 100;
	AddProp(s, c.props, 3, plant, 1000, out);
	plant.x = 400;
	AddProp(s, c.props, 7, plant, 1000, out);
	const float inside[3] = {100, 0, 100}, through[3] = {-700, 0, 100};
	PropDistancePass(s, c.props, inside, out);
	KT1_CHECK(out.empty());
	PropDistancePass(s, c.props, through, out);                    // 800 and 1100 elmos away
	KT1_CHECK_EQ(out.size(), size_t(1));
	KT1_CHECK_EQ(out[0].id, 7);
	out.clear();
	PropZoomPass(s, c, 5000, 1, out);                              // gone: only the shown one changes
	KT1_CHECK_EQ(out.size(), size_t(1));
	KT1_CHECK_EQ(out[0].id, 3);
	out.clear();
	PropZoomPass(s, c, 100, 1, out);
	KT1_CHECK_EQ(out.size(), size_t(1));                           // 7 stays beyond its own distance
	RemoveProp(s, 3);
	out.clear();
	RestoreProps(s, out);
	KT1_CHECK_EQ(out.size(), size_t(1));
	KT1_CHECK_EQ(s.index.Size(), 1);
}

KT1_CASE("VIEW-LOOK", "the ground look moves only by a step or onto an end") {
	const GroundLookConfig c;
	KT1_CHECK(LookFar(c, 1500) == 0);
	KT1_CHECK(LookFar(c, 4800) == 1);
	KT1_CHECK(LookNear(c, 450) == 1);
	KT1_CHECK(LookNear(c, 1500) == 0);
	SceneState s;
	KT1_CHECK(LookMoved(s, c, 0, 0));
	s.lookFar = 0;
	s.lookNear = 0.5f;
	KT1_CHECK(!LookMoved(s, c, 0, 0.505f));
	KT1_CHECK(LookMoved(s, c, 0, 0.52f));
	s.lookNear = 0.995f;
	KT1_CHECK(LookMoved(s, c, 0, 1));
	GroundLook base;
	base.diffuse[0] = 1;
	base.splat[0] = 0.4f;
	const GroundLook l = ApplyLook(c, base, 1, 1);
	KT1_CHECK(Near(l.diffuse[0], 0.65f));
	KT1_CHECK(l.splat[0] == 0.4f);
}

KT1_CASE("VIEW-WATER", "the far cut gives the player's mode back; dynamic water is replaced") {
	const WaterConfig c;
	SceneState s;
	KT1_CHECK_EQ(FarWaterMode(s, c, 1, 4), NO_WATER_MODE);
	KT1_CHECK_EQ(FarWaterMode(s, c, 2, 4), 0);
	KT1_CHECK_EQ(FarWaterMode(s, c, 3, 0), NO_WATER_MODE);
	KT1_CHECK_EQ(FarWaterMode(s, c, 1, 0), 4);
	FarWaterMode(s, c, 2, 4);
	KT1_CHECK_EQ(FarWaterMode(s, c, 1, 1), NO_WATER_MODE);         // someone else's mode stays
	KT1_CHECK_EQ(GuardWaterMode(c, 2), 4);
	KT1_CHECK_EQ(GuardWaterMode(c, 4), NO_WATER_MODE);
}
