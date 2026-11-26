/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComPathBatch.h"

#include "System/SupComSimSwitch.h"
#include "System/Threading/ThreadPool.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace SupComPathBatch {
	namespace {
		struct Key {
			std::uint32_t blockSize;
			std::int32_t sx, sz, gx, gz;
			std::uint32_t radiusBits;
			std::int32_t pathType;
			std::uint32_t synced;
			bool operator==(const Key& o) const {
				return blockSize == o.blockSize && sx == o.sx && sz == o.sz && gx == o.gx && gz == o.gz &&
					radiusBits == o.radiusBits && pathType == o.pathType && synced == o.synced;
			}
		};
		struct KeyHash {
			size_t operator()(const Key& k) const {
				std::uint64_t h = 1469598103934665603ull;
				const std::uint32_t v[] = { k.blockSize, std::uint32_t(k.sx), std::uint32_t(k.sz), std::uint32_t(k.gx), std::uint32_t(k.gz), k.radiusBits, std::uint32_t(k.pathType), k.synced };
				for (std::uint32_t x: v) { h ^= x; h *= 1099511628211ull; }
				return size_t(h);
			}
		};
		Key MakeKey(unsigned int blockSize, int2 s, int2 g, float r, int pathType, bool synced) {
			std::uint32_t rb;
			std::memcpy(&rb, &r, sizeof(rb));
			return Key{ blockSize, s.x, s.y, g.x, g.y, rb, pathType, synced ? 1u : 0u };
		}

		// main thread only (Active() is false elsewhere)
		std::unordered_map<Key, CPathCache::CacheItem, KeyHash> items;
		bool active = false;
		int on = -1;
		unsigned int hits = 0, misses = 0, requests = 0;
	}

	bool On() {
		if (on < 0)
			on = SupComSimSwitch::Get("PathBatchCache", true) ? 1 : 0;
		return (on != 0);
	}

	bool Active() {
		return active && ThreadPool::GetThreadNum() == 0;
	}

	Scope::Scope(): active(On()) {
		if (!active)
			return;
		items.clear();
		hits = misses = requests = 0;
		SupComPathBatch::active = true;
	}

	Scope::~Scope() {
		if (!active)
			return;
		SupComPathBatch::active = false;
		items.clear();
	}

	const CPathCache::CacheItem* Find(unsigned int blockSize, int2 strtBlock, int2 goalBlock, float goalRadius, int pathType, bool synced) {
		const auto it = items.find(MakeKey(blockSize, strtBlock, goalBlock, goalRadius, pathType, synced));
		if (it == items.end()) {
			++misses;
			return nullptr;
		}
		++hits;
		return &it->second;
	}

	void Add(unsigned int blockSize, const IPath::Path& path, IPath::SearchResult result, int2 strtBlock, int2 goalBlock, float goalRadius, int pathType, bool synced) {
		CPathCache::CacheItem ci;
		ci.result = result;
		ci.path = path;
		ci.strtBlock = strtBlock;
		ci.goalBlock = goalBlock;
		ci.goalRadius = goalRadius;
		ci.pathType = pathType;
		items.emplace(MakeKey(blockSize, strtBlock, goalBlock, goalRadius, pathType, synced), std::move(ci));
	}

	void Counts(unsigned int& h, unsigned int& m, unsigned int& r) { h = hits; m = misses; r = requests; }
	void CountRequest() { ++requests; }
}
