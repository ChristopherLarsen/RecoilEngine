/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComRibbonProjectile.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "Game/Camera.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Projectiles/ProjectileMemPool.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectile.h"
#include "Sim/Weapons/WeaponDef.h"
#include "Sim/Weapons/WeaponDefHandler.h"
#include "System/SpringMath.h"
#include "System/UnorderedMap.hpp"
#include "System/Log/ILog.h"

CR_BIND_DERIVED(CSupComRibbonProjectile, CProjectile, )
CR_REG_METADATA(CSupComRibbonProjectile, (
	CR_MEMBER(pos1), CR_MEMBER(pos2), CR_MEMBER(dir1), CR_MEMBER(dir2),
	CR_MEMBER(creationTime), CR_MEMBER(weaponDefID)
))

static_assert(sizeof(CSupComRibbonProjectile) <= PMP_S, "a ribbon segment must fit the projectile pool");

namespace {
	struct RGBA { float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f; };

	struct Params {
		bool on = false;
		std::string texName;
		AtlasedTexture* tex = nullptr; // resolved on first draw
		int time = 15;                 // frames a segment lives (DEFAULT)
		int period = 1;                // frames per segment (DEFAULT)
		float w0 = 2.0f, w1 = 2.0f;    // half-width, elmos (DEFAULT)
		RGBA c0 = {1.0f, 1.0f, 1.0f, 0.0f};
		RGBA c1 = {0.0f, 0.0f, 0.0f, 0.0f};
	};
	std::vector<Params> params; // by weapon def id, read on first use
	std::vector<char> paramsRead;

	Params& Get(int id) {
		if (id >= (int)params.size()) { params.resize(id + 1); paramsRead.resize(id + 1, 0); }
		Params& p = params[id];
		if (paramsRead[id]) return p;
		paramsRead[id] = 1;
		const WeaponDef* wd = weaponDefHandler->GetWeaponDefByID(id);
		if (wd == nullptr) return p;
		const auto& cp = wd->customParams;
		const auto Find = [&](const char* k) -> const std::string* { const auto it = cp.find(k); return (it == cp.end())? nullptr: &it->second; };
		if (const auto* s = Find("supcom_ribbon")) p.on = (*s == "1" || *s == "true");
		if (!p.on) return p;
		if (const auto* s = Find("supcom_ribbon_texture")) p.texName = *s;
		if (const auto* s = Find("supcom_ribbon_time")) p.time = std::max(1, std::atoi(s->c_str()));
		if (const auto* s = Find("supcom_ribbon_period")) p.period = std::max(1, std::atoi(s->c_str()));
		p.time = std::max(p.time, p.period + 1); // a segment outlives the period that stretches it
		if (const auto* s = Find("supcom_ribbon_width")) { std::istringstream in(*s); in >> p.w0 >> p.w1; }
		if (const auto* s = Find("supcom_ribbon_color0")) { std::istringstream in(*s); in >> p.c0.r >> p.c0.g >> p.c0.b >> p.c0.a; }
		if (const auto* s = Find("supcom_ribbon_color1")) { std::istringstream in(*s); in >> p.c1.r >> p.c1.g >> p.c1.b >> p.c1.a; }
		return p;
	}

	struct Trail { CSupComRibbonProjectile* seg = nullptr; // open segment: lives longer than the period it is stretched for (like the missile smoke trail)
		 float3 oldPos; float3 oldDir; int seen = 0; int age = 0; };
	spring::unordered_map<int, Trail> trails; // weapon projectile id -> its open segment (drawing only)
	int lastPurge = 0;
	std::vector<char> logged; // weapon defs whose first segment was logged
	int segments = 0;         // segments made since the last count line
}

CSupComRibbonProjectile::CSupComRibbonProjectile(const CUnit* owner, int wdID, const float3& posNew, const float3& posOld, const float3& dirNew, const float3& dirOld)
	: CProjectile((posNew + posOld) * 0.5f, ZeroVector, owner, false, false, false)
	, pos1(posNew), pos2(posOld), dir1(dirNew), dir2(dirOld)
	, creationTime(gs->frameNum), weaponDefID(wdID)
{
	checkCol = false;
	castShadow = false;
	UpdateEndPos(posNew, dirNew);
}

void CSupComRibbonProjectile::UpdateEndPos(const float3& pos, const float3& dir)
{
	pos1 = pos;
	dir1 = dir;
	SetPosition((pos1 + pos2) * 0.5f);
	SetRadiusAndHeight(pos1.distance(pos2) + 1.0f, 0.0f);
}

void CSupComRibbonProjectile::Update()
{
	deleteMe |= (gs->frameNum >= creationTime + Get(weaponDefID).time);
}

void CSupComRibbonProjectile::Draw()
{
	Params& p = Get(weaponDefID);
	if (p.tex == nullptr)
		p.tex = p.texName.empty()? projectileDrawer->smoketrailtex: &projectileDrawer->textureAtlas->GetTextureWithBackup(p.texName, "smoketrail");

	const AtlasedTexture* tex = p.tex;
	const float age = gs->frameNum + globalRendering->timeOffset - creationTime;
	// the new end (pos1) was laid down up to one period after the old end (pos2)
	const float t1 = std::clamp((age - p.period) / p.time, 0.0f, 1.0f);
	const float t2 = std::clamp(age / p.time, 0.0f, 1.0f);

	const float3 v1 = (pos1 - camera->GetPos()).ANormalize();
	const float3 v2 = (pos2 - camera->GetPos()).ANormalize();
	const float3 o1 = v1.cross(dir1).ANormalize() * mix(p.w0, p.w1, t1);
	const float3 o2 = v2.cross(dir2).ANormalize() * mix(p.w0, p.w1, t2);

	const auto Col = [&](float t) {
		const auto C = [&](float a, float b) { return std::clamp(a + (b - a) * t, 0.0f, 1.0f); };
		return SColor(C(p.c0.r, p.c1.r), C(p.c0.g, p.c1.g), C(p.c0.b, p.c1.b), C(p.c0.a, p.c1.a));
	};
	const SColor col1 = Col(t1);
	const SColor col2 = Col(t2);

	AddEffectsQuad<0>(
		tex->pageNum,
		{ pos1 - o1, tex->xstart, tex->ystart, col1 },
		{ pos2 - o2, tex->xend  , tex->ystart, col2 },
		{ pos2 + o2, tex->xend  , tex->yend  , col2 },
		{ pos1 + o1, tex->xstart, tex->yend  , col1 }
	);
}

void CSupComRibbonProjectile::OnWeaponUpdate(CWeaponProjectile* wp)
{
	const WeaponDef* wd = wp->GetWeaponDef();
	if (wd == nullptr || !Get(wd->id).on)
		return;

	const Params& p = Get(wd->id);
	const int now = gs->frameNum;

	auto it = trails.find(wp->id);
	if (it == trails.end()) {
		Trail t;
		t.oldPos = wp->pos;
		t.oldDir = (wp->speed.SqLength() > 1e-6f)? float3(wp->speed).ANormalize(): UpVector;
		t.seen = now;
		trails[wp->id] = t;
	} else {
		Trail& t = it->second;
		const float3 moved = wp->pos - t.oldPos;
		const float3 dir = (wp->speed.SqLength() > 1e-6f)? float3(wp->speed).ANormalize(): ((moved.SqLength() > 1e-6f)? float3(moved).ANormalize(): t.oldDir);
		t.seen = now;
		t.age += 1;

		// stretch the open segment to the projectile
		if (t.seg != nullptr)
			t.seg->UpdateEndPos(wp->pos, dir);

		if ((t.age % p.period) == 0) {
			t.seg = projMemPool.alloc<CSupComRibbonProjectile>(wp->owner(), wd->id, wp->pos, t.oldPos, dir, t.oldDir);
			segments += 1;
			if (wd->id >= (int)logged.size()) logged.resize(wd->id + 1, 0);
			if (!logged[wd->id]) { logged[wd->id] = 1; LOG("[SupComRibbon] first segment for weapon %s at frame %d", wd->name.c_str(), now); }
			t.oldPos = wp->pos;
			t.oldDir = dir;
		}
	}

	if (wp->deleteMe)
		trails.erase(wp->id);

	// drop trails whose projectile went away without being seen with deleteMe set
	if (now - lastPurge >= 30) { // frames between purges
		lastPurge = now;
		if (segments > 0 && now % 300 < 30) { LOG("[SupComRibbon] %d segments, %d open trails at frame %d", segments, (int)trails.size(), now); segments = 0; }
		for (auto i = trails.begin(); i != trails.end(); ) {
			if (now - i->second.seen > 2) i = trails.erase(i); else ++i;
		}
	}
}
