/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

// supcom: the HAPFS synced state in save games (paths under their ids, pending searches, estimator costs and update
// queues, synced path caches, heat map), so a loaded game plans exactly as the saved one. Our own file; the stock
// classes keep only the declarations. Written and read only with springsetting SaveGameFullState (SupComSaveState.h).

#include "PathManager.h"
#include "PathCache.h"
#include "PathHeatMap.h"
#include "PathingState.h"
#include "PathSearch.h"
#include "PathStateIO.h"
#include "Registry.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/Objects/SolidObject.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "System/Log/ILog.h"

#include <algorithm>
#include <string>
#include <vector>

namespace HAPFS {

// supcom: save games keep the synced path state (Q69). Bump on any layout change.
static constexpr std::uint32_t PATH_STATE_VERSION = 1;

// the estimators' states live in PathManager.cpp (file-static); reached through the manager's accessors
static PathingState* LowResPS(const CPathManager* pm) { return const_cast<PathingState*>(pm->GetLowResPS()); }
static PathingState* MedResPS(const CPathManager* pm) { return const_cast<PathingState*>(pm->GetMedResPS()); }

static std::int32_t SavedUnitID(const CSolidObject* obj) {
	const CUnit* unit = dynamic_cast<const CUnit*>(obj);
	return ((unit != nullptr)? unit->id: -1);
}

static CSolidObject* LoadedUnit(std::int32_t id) {
	return ((id >= 0)? unitHandler.GetUnit(id): nullptr);
}

static std::int32_t SavedPathType(const MoveDef* md) {
	return ((md != nullptr)? md->pathType: -1);
}

static const MoveDef* LoadedMoveDef(std::int32_t pathType) {
	return ((pathType >= 0)? moveDefHandler.GetMoveDefByPathType(pathType): nullptr);
}

void CPathManager::SaveMultiPath(std::ostream& os, const MultiPath& mp)
{
	StateIO::PutPath(os, mp.lowResPath);
	StateIO::PutPath(os, mp.medResPath);
	StateIO::PutPath(os, mp.maxResPath);
	StateIO::Put(os, std::int32_t(mp.searchResult));
	StateIO::Put(os, mp.start);
	StateIO::Put(os, mp.finalGoal);

	const CCircularSearchConstraint& pd = mp.peDef;
	StateIO::Put(os, pd.wsStartPos);
	StateIO::Put(os, pd.wsGoalPos);
	StateIO::Put(os, pd.sqGoalRadius);
	StateIO::Put(os, pd.maxRawPathLen);
	StateIO::Put(os, pd.minRawSpeedMod);
	StateIO::Put(os, pd.startInGoalRadius);
	StateIO::Put(os, pd.constraintDisabled);
	StateIO::Put(os, pd.skipSubSearches);
	StateIO::Put(os, pd.testMobile);
	StateIO::Put(os, pd.needPath);
	StateIO::Put(os, pd.exactPath);
	StateIO::Put(os, pd.allowRawPath);
	StateIO::Put(os, pd.allowDefPath);
	StateIO::Put(os, pd.dirIndependent);
	StateIO::Put(os, pd.synced);
	StateIO::Put(os, pd.useVerifiedStartBlock);
	StateIO::Put(os, pd.startSquareX);
	StateIO::Put(os, pd.startSquareZ);
	StateIO::Put(os, pd.goalSquareX);
	StateIO::Put(os, pd.goalSquareZ);
	StateIO::Put(os, pd.halfWayX);
	StateIO::Put(os, pd.halfWayZ);
	StateIO::Put(os, pd.searchRadiusSq);

	StateIO::Put(os, SavedPathType(mp.moveDef));
	StateIO::Put(os, SavedUnitID(mp.caller));
	StateIO::Put(os, mp.updated);
}

void CPathManager::LoadMultiPath(std::istream& is, MultiPath& mp)
{
	std::int32_t searchResult = 0;
	std::int32_t pathType = -1;
	std::int32_t callerID = -1;

	StateIO::GetPath(is, mp.lowResPath);
	StateIO::GetPath(is, mp.medResPath);
	StateIO::GetPath(is, mp.maxResPath);
	StateIO::Get(is, searchResult);
	StateIO::Get(is, mp.start);
	StateIO::Get(is, mp.finalGoal);

	CCircularSearchConstraint& pd = mp.peDef;
	StateIO::Get(is, pd.wsStartPos);
	StateIO::Get(is, pd.wsGoalPos);
	StateIO::Get(is, pd.sqGoalRadius);
	StateIO::Get(is, pd.maxRawPathLen);
	StateIO::Get(is, pd.minRawSpeedMod);
	StateIO::Get(is, pd.startInGoalRadius);
	StateIO::Get(is, pd.constraintDisabled);
	StateIO::Get(is, pd.skipSubSearches);
	StateIO::Get(is, pd.testMobile);
	StateIO::Get(is, pd.needPath);
	StateIO::Get(is, pd.exactPath);
	StateIO::Get(is, pd.allowRawPath);
	StateIO::Get(is, pd.allowDefPath);
	StateIO::Get(is, pd.dirIndependent);
	StateIO::Get(is, pd.synced);
	StateIO::Get(is, pd.useVerifiedStartBlock);
	StateIO::Get(is, pd.startSquareX);
	StateIO::Get(is, pd.startSquareZ);
	StateIO::Get(is, pd.goalSquareX);
	StateIO::Get(is, pd.goalSquareZ);
	StateIO::Get(is, pd.halfWayX);
	StateIO::Get(is, pd.halfWayZ);
	StateIO::Get(is, pd.searchRadiusSq);

	StateIO::Get(is, pathType);
	StateIO::Get(is, callerID);
	StateIO::Get(is, mp.updated);

	mp.searchResult = IPath::SearchResult(searchResult);
	mp.moveDef = LoadedMoveDef(pathType);
	mp.caller = LoadedUnit(callerID);
}

void CPathManager::SaveState(std::ostream& os) const
{
	assert(IsFinalized());
	StateIO::Put(os, PATH_STATE_VERSION);

	// paths in id order, so equal games write equal saves
	std::vector<unsigned int> pathIDs;
	pathIDs.reserve(pathMap.size());
	for (const auto& p: pathMap)
		pathIDs.push_back(p.first);
	std::sort(pathIDs.begin(), pathIDs.end());

	StateIO::Put(os, std::uint64_t(pathIDs.size()));
	for (const unsigned int pathID: pathIDs) {
		StateIO::Put(os, pathID);
		SaveMultiPath(os, pathMap.find(pathID)->second);
	}
	StateIO::Put(os, nextPathID);

	// searches requested since the last Update (resolved by the next one)
	std::vector<entt::entity> searches;
	for (const entt::entity e: registry.view<PathSearch>())
		searches.push_back(e);
	std::sort(searches.begin(), searches.end(), [](entt::entity a, entt::entity b) {
		return (registry.get<PathSearch>(a).pathId < registry.get<PathSearch>(b).pathId);
	});

	StateIO::Put(os, std::uint64_t(searches.size()));
	for (const entt::entity e: searches) {
		const PathSearch& ps = registry.get<PathSearch>(e);
		const PathExtension* pe = registry.try_get<PathExtension>(e);

		StateIO::Put(os, ps.pathId);
		StateIO::Put(os, SavedUnitID(ps.caller));
		StateIO::Put(os, SavedPathType(ps.moveDef));
		StateIO::Put(os, ps.startPos);
		StateIO::Put(os, ps.goalPos);
		StateIO::Put(os, ps.goalRadius);
		StateIO::Put(os, std::int32_t((pe != nullptr)? int(pe->pathResToExtend): -1));
	}

	StateIO::Put(os, frameNumToRefreshPathStateWorkloadRatio);
	StateIO::Put(os, pathStateWorkloadRatio);
	StateIO::Put(os, std::uint8_t(highPriorityResPS == LowResPS(this)));

	LowResPS(this)->SaveState(os);
	MedResPS(this)->SaveState(os);
	pathHeatMap->SaveState(os);
}

void CPathManager::LoadState(std::istream& is)
{
	assert(IsFinalized());

	std::uint32_t version = 0;
	StateIO::Get(is, version);
	if (version != PATH_STATE_VERSION)
		throw content_error("[HAPFS] save game path state version " + std::to_string(version) + ", this build reads " + std::to_string(PATH_STATE_VERSION));

	pathMap.clear();
	registry.clear();

	std::uint64_t numPaths = 0;
	StateIO::Get(is, numPaths);
	for (std::uint64_t i = 0; i < numPaths; i++) {
		unsigned int pathID = 0;
		StateIO::Get(is, pathID);
		LoadMultiPath(is, pathMap[pathID]);
	}
	StateIO::Get(is, nextPathID);

	std::uint64_t numSearches = 0;
	StateIO::Get(is, numSearches);
	for (std::uint64_t i = 0; i < numSearches; i++) {
		unsigned int pathID = 0;
		std::int32_t callerID = -1;
		std::int32_t pathType = -1;
		float3 startPos;
		float3 goalPos;
		float goalRadius = 0.0f;
		std::int32_t extendRes = -1;

		StateIO::Get(is, pathID);
		StateIO::Get(is, callerID);
		StateIO::Get(is, pathType);
		StateIO::Get(is, startPos);
		StateIO::Get(is, goalPos);
		StateIO::Get(is, goalRadius);
		StateIO::Get(is, extendRes);

		const entt::entity e = registry.create();
		registry.emplace<PathSearch>(e, LoadedUnit(callerID), LoadedMoveDef(pathType), startPos, goalPos, goalRadius, pathID);
		if (extendRes >= 0)
			registry.emplace<PathExtension>(e, ExtendPathResType(extendRes));
	}

	std::uint8_t lowResFirst = 0;
	StateIO::Get(is, frameNumToRefreshPathStateWorkloadRatio);
	StateIO::Get(is, pathStateWorkloadRatio);
	StateIO::Get(is, lowResFirst);
	highPriorityResPS = (lowResFirst? LowResPS(this): MedResPS(this));
	lowPriorityResPS  = (lowResFirst? MedResPS(this): LowResPS(this));

	LowResPS(this)->LoadState(is);
	MedResPS(this)->LoadState(is);
	pathHeatMap->LoadState(is);

	stateLoaded = true;
	LOG("[HAPFS::%s] loaded %u paths, %u pending searches", __func__, unsigned(numPaths), unsigned(numSearches));
}


void PathingState::SaveState(std::ostream& os) const
{
	StateIO::PutVec(os, vertexCosts);
	StateIO::PutVec(os, blockStates.nodeMask);
	StateIO::PutVec(os, blockStates.nodeLinksObsoleteFlags);

	StateIO::Put(os, std::uint64_t(blockStates.peNodeOffsets.size()));
	for (const auto& pathTypeOffsets: blockStates.peNodeOffsets)
		StateIO::PutVec(os, pathTypeOffsets);

	StateIO::PutDeque(os, updatedBlocks);
	pathCache[true]->SaveState(os);
}

void PathingState::LoadState(std::istream& is)
{
	StateIO::GetVecSized(is, vertexCosts, "vertex costs");
	StateIO::GetVecSized(is, blockStates.nodeMask, "node mask");
	StateIO::GetVecSized(is, blockStates.nodeLinksObsoleteFlags, "node links");

	std::uint64_t numPathTypes = 0;
	StateIO::Get(is, numPathTypes);
	if (numPathTypes != blockStates.peNodeOffsets.size())
		throw content_error("[HAPFS::StateIO] path state size mismatch: path types");

	for (auto& pathTypeOffsets: blockStates.peNodeOffsets)
		StateIO::GetVecSized(is, pathTypeOffsets, "block offsets");

	StateIO::GetDeque(is, updatedBlocks);
	pathCache[true]->LoadState(is);
}


void CPathCache::SaveState(std::ostream& os) const
{
	// every queue item names one cached path (AddPath queues a hash only when it adds it)
	StateIO::Put(os, std::uint64_t(cacheQue.size()));

	for (const CacheQueItem& qi: cacheQue) {
		const auto it = cachedPaths.find(qi.hash);
		assert(it != cachedPaths.end());
		const CacheItem& ci = it->second;

		StateIO::Put(os, qi.timeout);
		StateIO::Put(os, qi.hash);
		StateIO::Put(os, std::int32_t(ci.result));
		StateIO::PutPath(os, ci.path);
		StateIO::Put(os, ci.strtBlock);
		StateIO::Put(os, ci.goalBlock);
		StateIO::Put(os, ci.goalRadius);
		StateIO::Put(os, ci.pathType);
	}
}

void CPathCache::LoadState(std::istream& is)
{
	std::uint64_t n = 0;
	StateIO::Get(is, n);

	cacheQue.clear();
	cachedPaths.clear();

	for (std::uint64_t i = 0; i < n; i++) {
		CacheQueItem qi;
		CacheItem ci;
		std::int32_t result = 0;

		StateIO::Get(is, qi.timeout);
		StateIO::Get(is, qi.hash);
		StateIO::Get(is, result);
		StateIO::GetPath(is, ci.path);
		StateIO::Get(is, ci.strtBlock);
		StateIO::Get(is, ci.goalBlock);
		StateIO::Get(is, ci.goalRadius);
		StateIO::Get(is, ci.pathType);
		ci.result = IPath::SearchResult(result);

		cacheQue.push_back(qi);
		cachedPaths[qi.hash] = std::move(ci);
	}

	maxCacheSize = std::max<std::uint64_t>(maxCacheSize, cacheQue.size());
}


void PathHeatMap::SaveState(std::ostream& os) const {
	StateIO::PutVec(os, heatMap);
	StateIO::Put(os, heatMapOffset);
}

void PathHeatMap::LoadState(std::istream& is) {
	StateIO::GetVecSized(is, heatMap, "heat map");
	StateIO::Get(is, heatMapOffset);
}

}
