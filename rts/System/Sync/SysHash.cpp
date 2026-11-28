/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 per-system sync hashes (NEW-ENGINE-PLAN S3). See SysHash.h.

#include "SysHash.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/Team.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Projectiles/Projectile.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

CONFIG(float, SysHashPeriodS).defaultValue(0.0f).minimumValue(0.0f)
	.description("KT1: log [SYSHASH] per-system sync hashes every this many seconds (0 = off); also keeps a ring of the last frames for the desync dump");

namespace {
	constexpr int RING = 32; // frames kept for the desync dump (3.2 s at 10 Hz)

	struct Hasher {
		uint64_t h = 1469598103934665603ull; // FNV-1a 64
		void Bytes(const void* p, size_t n) {
			const auto* b = static_cast<const unsigned char*>(p);
			for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
		}
		template<typename T> void Add(const T& v) { Bytes(&v, sizeof(v)); }
		uint32_t Get() const { return uint32_t(h ^ (h >> 32)); }
	};

	struct FrameRec {
		int frame = -1;
		uint32_t units = 0, features = 0, projectiles = 0, teams = 0;
		std::vector<std::pair<int, uint32_t>> perUnit; // ascending unit id
	};

	std::array<FrameRec, RING> ring;
	int ringPos = 0;
	bool configRead = false;
	float periodS = 0.0f;

	void HashFrame(FrameRec& r, int frameNum) {
		r.frame = frameNum;
		r.perUnit.clear();

		{
			const std::vector<CUnit*>& units = unitHandler.GetActiveUnits();
			r.perUnit.reserve(units.size());
			for (const CUnit* u: units) {
				Hasher uh;
				uh.Add(u->id); uh.Add(u->team);
				uh.Add(u->pos.x); uh.Add(u->pos.y); uh.Add(u->pos.z);
				uh.Add(u->speed.x); uh.Add(u->speed.y); uh.Add(u->speed.z);
				uh.Add(u->frontdir.x); uh.Add(u->frontdir.y); uh.Add(u->frontdir.z);
				uh.Add(u->heading);
				uh.Add(u->health); uh.Add(u->buildProgress);
				r.perUnit.emplace_back(u->id, uh.Get());
			}
			std::sort(r.perUnit.begin(), r.perUnit.end());
			Hasher all;
			for (const auto& [id, h]: r.perUnit) { all.Add(id); all.Add(h); }
			r.units = all.Get();
		}
		{
			std::vector<int> ids(featureHandler.GetActiveFeatureIDs().begin(), featureHandler.GetActiveFeatureIDs().end());
			std::sort(ids.begin(), ids.end());
			Hasher fh;
			for (const int id: ids) {
				const CFeature* f = featureHandler.GetFeature(id);
				if (f == nullptr)
					continue;
				fh.Add(id);
				fh.Add(f->pos.x); fh.Add(f->pos.y); fh.Add(f->pos.z);
				fh.Add(f->health); fh.Add(f->reclaimLeft);
			}
			r.features = fh.Get();
		}
		{
			const auto& projs = projectileHandler.GetActiveProjectiles(true);
			std::vector<const CProjectile*> ps(projs.begin(), projs.end());
			std::sort(ps.begin(), ps.end(), [](const CProjectile* a, const CProjectile* b) { return a->id < b->id; });
			Hasher ph;
			for (const CProjectile* p: ps) {
				ph.Add(p->id);
				ph.Add(p->pos.x); ph.Add(p->pos.y); ph.Add(p->pos.z);
				ph.Add(p->speed.x); ph.Add(p->speed.y); ph.Add(p->speed.z);
			}
			r.projectiles = ph.Get();
		}
		{
			Hasher th;
			for (int a = 0; a < teamHandler.ActiveTeams(); ++a) {
				const CTeam* t = teamHandler.Team(a);
				for (const float v: t->res) th.Add(v);
				for (const float v: t->resStorage) th.Add(v);
			}
			r.teams = th.Get();
		}
	}

	void LogRec(const FrameRec& r) {
		LOG("[SYSHASH] f=%d units=%08x features=%08x projectiles=%08x teams=%08x n=%u",
			r.frame, r.units, r.features, r.projectiles, r.teams, unsigned(r.perUnit.size()));
	}
}

void KT1::SysHash::Frame(int frameNum)
{
	if (!configRead) {
		periodS = configHandler->GetFloat("SysHashPeriodS");
		configRead = true;
	}
	if (periodS <= 0.0f)
		return;

	FrameRec& r = ring[ringPos];
	ringPos = (ringPos + 1) % RING;
	HashFrame(r, frameNum);

	const int every = std::max(1, int(periodS * GAME_SPEED + 0.5f));
	if ((frameNum % every) == 0)
		LogRec(r);
}

void KT1::SysHash::DumpRing(int desyncFrame)
{
	if (periodS <= 0.0f) {
		LOG("[SYSHASH] desync at f=%d: SysHashPeriodS is 0, no ring kept", desyncFrame);
		return;
	}
	LOG("[SYSHASH] desync at f=%d: ring of %d frames follows", desyncFrame, RING);
	for (int k = 0; k < RING; ++k) {
		const FrameRec& r = ring[(ringPos + k) % RING];
		if (r.frame < 0)
			continue;
		LogRec(r);
		for (const auto& [id, h]: r.perUnit)
			LOG("[SYSHASHU] f=%d id=%d h=%08x", r.frame, id, h);
	}
}
