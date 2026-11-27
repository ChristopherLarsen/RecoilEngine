/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 UI rules for RND-005 v2 R11-R13 (game-build requirements/released/RND-005-v2.md): three small unsynced API tables.
//
// Spring.KT1.Terrain (R11)  Form(): the terrain renderer form the engine created ("roam", "basic", "none" in a headless engine). The form
//                           comes from the graphics setting at terrain creation; nothing here reads the camera or the zoom.
// Spring.KT1.Draw    (R12)  SetCutoff(name, v) / GetCutoff(name): named draw cut-offs in the camera measure (camera distance in elmos);
//                           "unit_text" defaults to 80 (the parity value), nearer than the health-bar cut-off. CameraMeasure(): the
//                           camera's distance to the ground point at the screen centre (elmos), the measure the cut-offs are read in.
//                           ModelTint(): 12, the first of the four per-object user uniforms (12..15) the GL4 model shaders read as a
//                           tint: gl.SetFeatureBufferUniforms(id, {r, g, b, 1}, 12) darkens that feature in the engine's own pass;
//                           {1, 1, 1, 0} clears it (ModelVertProgGL4.glsl, "KT1 hook: model tint").
// Spring.KT1.Decals  (R13)  SetFade(f) / GetFade(): the fade fraction, default 0.25 at game start: a Lua decal with a cut-off (user data 0 y)
//                           starts fading at f x its cut-off and is not drawn at the cut-off (GroundDecalsVertProg.glsl, uniform cutFade).
//                           SetCutoff(decalID, v): that decal's own cut-off. GetDrawnAlpha(decalID, measure): the alpha the shader
//                           draws it with at that camera measure (its alpha x the cut-off fade), computed on the CPU with the shader's formula.
//
// Determinism: draw only; no sim state is read or written. Writes come from unsynced Lua only.

#include <algorithm>
#include <map>
#include <memory>
#include <string>

#include "Game/Camera.h"
#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1Rule.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "Map/SMF/SMFGroundDrawer.h"
#include "Map/SMF/Basic/BasicMeshDrawer.h"
#include "Map/SMF/ROAM/RoamMeshDrawer.h"
#include "Rendering/Env/Decals/GroundDecal.h"
#include "Rendering/Env/Decals/SupComDecalCut.h"
#include "Rendering/Env/IGroundDecalDrawer.h"

namespace KT1 {
namespace Rnd005 {
namespace {

std::map<std::string, float>& Cutoffs()
{
	static std::map<std::string, float> c = {{"unit_text", 80.0f}};   // PARITY (RND-005 v2 R12): unit timer text, in the camera measure
	return c;
}

float SmoothStep(float e0, float e1, float x)
{
	if (e1 <= e0)
		return (x < e1) ? 0.0f : 1.0f;
	const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

// ---- Terrain ----
int LForm(lua_State* L)
{
	const char* form = "none";
	if (readMap != nullptr) {
		if (auto* gd = dynamic_cast<CSMFGroundDrawer*>(readMap->GetGroundDrawer()); gd != nullptr) {
			IMeshDrawer* md = gd->GetMeshDrawer();
			if (dynamic_cast<CRoamMeshDrawer*>(md) != nullptr)
				form = "roam";
			else if (dynamic_cast<CBasicMeshDrawer*>(md) != nullptr)
				form = "basic";
			else
				form = "other";
		}
	}
	lua_pushstring(L, form);
	return 1;
}

// ---- Draw ----
int LSetCutoff(lua_State* L)
{
	if (CLuaHandle::GetHandleSynced(L)) return 0;
	Cutoffs()[luaL_checkstring(L, 1)] = luaL_checkfloat(L, 2);
	return 0;
}

int LGetCutoff(lua_State* L)
{
	const auto& c = Cutoffs();
	const auto it = c.find(luaL_checkstring(L, 1));
	if (it == c.end())
		return 0;
	lua_pushnumber(L, it->second);
	return 1;
}

int LCameraMeasure(lua_State* L)
{
	if (camera == nullptr)
		return 0;
	const float3 pos = camera->GetPos();
	float d = CGround::LineGroundCol(pos, camera->GetDir(), 1.0e6f, false);
	if (d < 0.0f)
		d = pos.y - ((readMap != nullptr) ? readMap->GetCurrAvgHeight() : 0.0f);   // looking above the horizon: the height over the map
	lua_pushnumber(L, d);
	return 1;
}

// ---- Decals ----
int LSetFade(lua_State* L)
{
	if (CLuaHandle::GetHandleSynced(L)) return 0;
	SupComDecalCut::fade = std::clamp(luaL_checkfloat(L, 1), 0.0f, 1.0f);
	return 0;
}

int LGetFade(lua_State* L)
{
	lua_pushnumber(L, SupComDecalCut::fade);
	return 1;
}

int LDecalSetCutoff(lua_State* L)
{
	if (CLuaHandle::GetHandleSynced(L)) return 0;
	GroundDecal* d = (groundDecals != nullptr) ? groundDecals->GetDecalById(luaL_checkint(L, 1)) : nullptr;
	if (d == nullptr) {
		lua_pushboolean(L, false);
		return 1;
	}
	d->userDefined[0].y = std::max(0.0f, luaL_checkfloat(L, 2));
	lua_pushboolean(L, true);
	return 1;
}

int LGetDrawnAlpha(lua_State* L)
{
	const IGroundDecalDrawer* gdd = groundDecals;
	const GroundDecal* d = (gdd != nullptr) ? gdd->GetDecalById(luaL_checkint(L, 1)) : nullptr;
	if (d == nullptr)
		return 0;
	const float measure = luaL_checkfloat(L, 2);
	const float y = d->userDefined[0].y;
	float a = d->alpha;
	if (d->info.type == static_cast<uint8_t>(GroundDecal::Type::DECAL_LUA) && y > 0.0f)   // the shader's rule: Lua decals with a cut-off only
		a *= 1.0f - SmoothStep(y * SupComDecalCut::fade, y, measure);
	lua_pushnumber(L, a);
	return 1;
}

class TerrainRule : public Rule {
public:
	TerrainRule() : Rule({"Terrain", 1, false, "(RND-005 R11 read)", true}) {}
	void PushLua(lua_State* L) override { LuaPushRawNamedCFunc(L, "Form", LForm); }
};

class DrawRule : public Rule {
public:
	DrawRule() : Rule({"Draw", 2, false, "(RND-005 R12 cut-offs)", true}) {}
	static int LModelTint(lua_State* L) { lua_pushinteger(L, 12); return 1; }
	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "ModelTint", LModelTint);
		LuaPushRawNamedCFunc(L, "SetCutoff", LSetCutoff);
		LuaPushRawNamedCFunc(L, "GetCutoff", LGetCutoff);
		LuaPushRawNamedCFunc(L, "CameraMeasure", LCameraMeasure);
	}
};

class DecalsRule : public Rule {
public:
	DecalsRule() : Rule({"Decals", 1, false, "(RND-005 R13 decal fade)", true}) {}
	void Start() override { SupComDecalCut::fade = SupComDecalCut::DEFAULT_FADE; }   // set at game start
	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "SetFade", LSetFade);
		LuaPushRawNamedCFunc(L, "GetFade", LGetFade);
		LuaPushRawNamedCFunc(L, "SetCutoff", LDecalSetCutoff);
		LuaPushRawNamedCFunc(L, "GetDrawnAlpha", LGetDrawnAlpha);
	}
};

} // namespace
} // namespace Rnd005

std::unique_ptr<Rule> MakeTerrain() { return std::make_unique<Rnd005::TerrainRule>(); }
std::unique_ptr<Rule> MakeDraw() { return std::make_unique<Rnd005::DrawRule>(); }
std::unique_ptr<Rule> MakeDecals() { return std::make_unique<Rnd005::DecalsRule>(); }

} // namespace KT1
