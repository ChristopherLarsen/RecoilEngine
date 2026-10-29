/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComTerrain.h"

#include "BasicMapDamage.h"
#include "MapDamage.h"
#include "Sim/Misc/SmoothHeightMesh.h"
#include "Sim/Path/HAPFS/PathStateIO.h"
#include "ReadMap.h"
#include "System/Config/ConfigHandler.h"

#include <cinttypes>
#include <istream>
#include <ostream>
#include <queue>
#include <string>

CONFIG(bool, TerrainFollowHeights).defaultValue(true).description("supcom: the derived terrain (normals, slopes) follows the heights in every frame a crater or terraform changes them, and the recalculation covers building footprints and a terraform's last step (1); stock updates only at the end of a crater or on the builder's slow update (0).");

namespace SupComTerrain {
	bool FollowHeights()
	{
		static const bool on = configHandler->GetBool("TerrainFollowHeights");
		return on;
	}

	void UpdateDerived(const Area& a)
	{
		readMap->UpdateHeightMapSynced({std::max(a.x1, 0), std::max(a.z1, 0), std::min(a.x2, mapDims.mapx), std::min(a.z2, mapDims.mapy)});
	}

	void Recalc(const Area& a, int r)
	{
		mapDamage->RecalcArea(a.x1 - r, a.x2 + r, a.z1 - r, a.z2 + r);
	}
}


// supcom: terrain state in save games (Q69), our own file; the stock classes keep only the declarations. Written and
// read only with springsetting SaveGameFullState (System/LoadSave/SupComSaveState.h).

// supcom (Q69): the smoothed mesh and the updates still queued for it. The mesh follows terrain changes over many
// frames (damage queues, then the horizontal and vertical blur passes), so a loaded game keeps the saved state
// instead of rebuilding it from the loaded heights.
namespace {
	constexpr std::uint32_t SMOOTH_MESH_STATE_VERSION = 1;

	void PutQueue(std::ostream& os, std::queue<int> q) {
		HAPFS::StateIO::Put(os, std::uint64_t(q.size()));
		for (; !q.empty(); q.pop())
			HAPFS::StateIO::Put(os, q.front());
	}
	void GetQueue(std::istream& is, std::queue<int>& q) {
		std::uint64_t n = 0;
		HAPFS::StateIO::Get(is, n);
		q = {};
		for (std::uint64_t i = 0; i < n; i++) {
			int v = 0;
			HAPFS::StateIO::Get(is, v);
			q.push(v);
		}
	}
}

void SmoothHeightMesh::SaveState(std::ostream& os) const
{
	using namespace HAPFS::StateIO;
	Put(os, SMOOTH_MESH_STATE_VERSION);
	Put(os, enabled);
	Put(os, maxx);
	Put(os, maxy);
	PutVec(os, maximaMesh);
	PutVec(os, mesh);
	PutVec(os, tempMesh);
	PutVec(os, origMesh);
	PutVec(os, colsMaxima);
	PutVec(os, maximaRows);

	const std::vector<std::uint8_t> damage(mapChangeTrack.damageMap.begin(), mapChangeTrack.damageMap.end());
	PutVec(os, damage);
	PutQueue(os, mapChangeTrack.damageQueue[0]);
	PutQueue(os, mapChangeTrack.damageQueue[1]);
	PutQueue(os, mapChangeTrack.horizontalBlurQueue);
	PutQueue(os, mapChangeTrack.verticalBlurQueue);
	Put(os, mapChangeTrack.queueReleaseOnFrame);
	Put(os, mapChangeTrack.activeBuffer);
}

void SmoothHeightMesh::LoadState(std::istream& is)
{
	using namespace HAPFS::StateIO;
	std::uint32_t version = 0;
	Get(is, version);
	if (version != SMOOTH_MESH_STATE_VERSION)
		throw content_error("[SmoothHeightMesh] save game has smooth mesh state version " + std::to_string(version));

	bool savedEnabled = false;
	int savedMaxX = 0;
	int savedMaxY = 0;
	Get(is, savedEnabled);
	Get(is, savedMaxX);
	Get(is, savedMaxY);
	if (savedEnabled != enabled || savedMaxX != maxx || savedMaxY != maxy)
		throw content_error("[SmoothHeightMesh] smooth mesh of the save game does not fit this game (size or enabled state)");

	GetVecSized(is, maximaMesh, "smooth mesh maxima");
	GetVecSized(is, mesh, "smooth mesh");
	GetVecSized(is, tempMesh, "smooth mesh blur buffer");
	GetVecSized(is, origMesh, "original smooth mesh");
	GetVecSized(is, colsMaxima, "smooth mesh column maxima");
	GetVecSized(is, maximaRows, "smooth mesh maxima rows");

	std::vector<std::uint8_t> damage;
	GetVec(is, damage);
	if (damage.size() != mapChangeTrack.damageMap.size())
		throw content_error("[SmoothHeightMesh] smooth mesh damage map size mismatch");
	std::copy(damage.begin(), damage.end(), mapChangeTrack.damageMap.begin());
	GetQueue(is, mapChangeTrack.damageQueue[0]);
	GetQueue(is, mapChangeTrack.damageQueue[1]);
	GetQueue(is, mapChangeTrack.horizontalBlurQueue);
	GetQueue(is, mapChangeTrack.verticalBlurQueue);
	Get(is, mapChangeTrack.queueReleaseOnFrame);
	Get(is, mapChangeTrack.activeBuffer);
}

// supcom (Q69): craters still being dug when the game is saved. Each explosion lowers its squares over
// EXPLOSION_LIFETIME frames from the height steps it put in the square pool; without them a loaded game would
// stop the crater part-way. Saves the queue and, for every explosion still running, its steps from the pool.
namespace {
	constexpr std::uint32_t MAP_DAMAGE_STATE_VERSION = 1;
}

void CBasicMapDamage::SaveState(std::ostream& os) const
{
	using namespace HAPFS::StateIO;
	Put(os, MAP_DAMAGE_STATE_VERSION);
	Put(os, std::uint64_t(explosionSquaresPool.size()));
	Put(os, explSquaresPoolIdx);
	Put(os, explUpdateQueueIdx);
	Put(os, std::uint64_t(explosionUpdateQueue.size()));

	for (const Explo& e: explosionUpdateQueue) {
		Put(os, e.pos);
		Put(os, e.strength);
		Put(os, e.radius);
		Put(os, e.ttl);
		Put(os, e.x1); Put(os, e.x2);
		Put(os, e.y1); Put(os, e.y2);
		Put(os, e.idx);
		PutVec(os, e.buildings);

		const std::uint64_t n = (e.ttl > 0)? std::uint64_t(e.x2 - e.x1 + 1) * (e.y2 - e.y1 + 1): 0;
		Put(os, n);
		for (std::uint64_t k = 0; k < n; k++)
			Put(os, explosionSquaresPool[(e.idx + k) % explosionSquaresPool.size()]);
	}
}

void CBasicMapDamage::LoadState(std::istream& is)
{
	using namespace HAPFS::StateIO;
	std::uint32_t version = 0;
	Get(is, version);
	if (version != MAP_DAMAGE_STATE_VERSION)
		throw content_error("[BasicMapDamage] save game has map damage state version " + std::to_string(version));

	std::uint64_t poolSize = 0;
	std::uint64_t count = 0;
	Get(is, poolSize);
	if (poolSize != explosionSquaresPool.size())
		throw content_error("[BasicMapDamage] explosion square pool size mismatch");

	Get(is, explSquaresPoolIdx);
	Get(is, explUpdateQueueIdx);
	Get(is, count);

	explosionUpdateQueue.clear();
	explosionUpdateQueue.resize(count);

	for (Explo& e: explosionUpdateQueue) {
		Get(is, e.pos);
		Get(is, e.strength);
		Get(is, e.radius);
		Get(is, e.ttl);
		Get(is, e.x1); Get(is, e.x2);
		Get(is, e.y1); Get(is, e.y2);
		Get(is, e.idx);
		GetVec(is, e.buildings);

		std::uint64_t n = 0;
		Get(is, n);
		for (std::uint64_t k = 0; k < n; k++)
			Get(is, explosionSquaresPool[(e.idx + k) % explosionSquaresPool.size()]);
	}
}
