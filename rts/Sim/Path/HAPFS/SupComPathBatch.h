/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
/* SupCom (PERF-REVIEW-F P1a, 2026-11-26): the path requests of one frame share their coarse searches.
 * The frame's queued requests run in one batch (CPathManager::Update, PathRequestsSerial: serially inside a parallel-section
 * flag), and inside a parallel section nothing is added to the estimators' path cache, so an army given one order searched
 * the same low/med-res route once per unit (PB1 perf_pathburst: 2,000 units, about 0.4 s on one frame). During a serial batch
 * the estimator levels (block size > 1) also keep a batch cache with the shared cache's own key and rule (start block, goal
 * block, goal radius, path type, synced; results Ok and GoalOutOfRange): a request whose start block, goal block, radius and
 * path type equal an earlier one in the batch gets that one's coarse path, as the shared cache would give it outside the
 * batch. The max-res parts (raw search, refine) stay per unit. Emptied at the end of every batch.
 * Mod option "pathbatchcache" (SupComSimSwitch: the same on every client), default on; "0": every request searches, as before.
 * Changes paths (not bit-identical): a sync record goes with it (docs/engine/DETERMINISM.md). */
#ifndef SUPCOM_PATH_BATCH_H
#define SUPCOM_PATH_BATCH_H

#include "PathCache.h"
#include "System/type2.h"

namespace SupComPathBatch {
	bool On();
	// true while a serial batch runs on the main thread (the only time Find/Add are used)
	bool Active();

	struct Scope {
		Scope();
		~Scope();
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
		bool active;
	};

	const CPathCache::CacheItem* Find(unsigned int blockSize, int2 strtBlock, int2 goalBlock, float goalRadius, int pathType, bool synced);
	void Add(unsigned int blockSize, const IPath::Path& path, IPath::SearchResult result, int2 strtBlock, int2 goalBlock, float goalRadius, int pathType, bool synced);

	// counts of the last batch, for the log line (hits, misses)
	void Counts(unsigned int& hits, unsigned int& misses, unsigned int& requests);
	void CountRequest();
}

#endif
