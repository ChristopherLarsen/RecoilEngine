/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: one instanced draw of quads per system and frame. The system keeps its instances in the batch (by id); the
// batch copies only the ranges that changed into a persistently mapped buffer (a ring of regions, fenced by the engine's
// stream buffer) and draws them all with one call. Without GL_ARB_buffer_storage it uploads the same ranges with
// glBufferSubData into a single region.
//
// Use:   Init once (GL context); each frame Instances().Set / Remove; bind the shader and the GL state; Draw().
// Shader inputs: location 0 = the quad corner (vec2, -1..1, six vertices), locations 1.. = the instance attributes.
// Draw side only. The bookkeeping (what changed, which region lacks it) is Core/DrawHost/InstanceStore.h, tested there.

#ifndef KT1_DRAW_INSTANCEBATCH_H
#define KT1_DRAW_INSTANCEBATCH_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "Game/KT1/Core/DrawHost/InstanceStore.h"

template<typename T> class IStreamBuffer;
class VAO;
class VBO;

namespace KT1 {

// one per-instance shader input: `components` values read at byte `offset` of the instance
struct InstanceAttrib {
	int location = 1;
	int components = 4;
	bool bytes = false;     // false: floats; true: unsigned bytes read as 0..1
	size_t offset = 0;
};

// the GL half, free of the instance type
class InstanceBuffer {
public:
	InstanceBuffer();
	~InstanceBuffer();
	InstanceBuffer(const InstanceBuffer&) = delete;
	InstanceBuffer& operator=(const InstanceBuffer&) = delete;

	bool Init(const char* name, size_t stride, int capacity, const std::vector<InstanceAttrib>& attribs);
	void Kill();
	bool Valid() const { return stream != nullptr; }
	int Regions() const { return regions; }
	int Region() const;     // the region the next Upload writes and the next Draw reads

	// copies `ranges` of `data` (the whole instance array) into the current region
	void Upload(const void* data, const std::vector<SlotRange>& ranges);
	// instances of the first `count` slots of the current region that differ from `data` (a GPU read-back: check mode only)
	int Differences(const void* data, int count);
	// one instanced draw from the current region, then on to the next
	void Draw(int count);

private:
	std::unique_ptr<IStreamBuffer<uint8_t>> stream;
	std::unique_ptr<VBO> corners;
	std::vector<std::unique_ptr<VAO>> vaos;    // one per region: the attribute offsets are set once
	std::vector<uint8_t> readBack;
	size_t stride = 0;
	int capacity = 0;
	int regions = 1;
};

struct InstanceBatchStats {
	int instances = 0;      // of the last Draw
	int uploaded = 0;       // instances copied for it
	int ranges = 0;         // copies made for it
	int draws = 0;          // Draw calls that reached GL, since Init
	int differences = 0;    // check mode: instances the GPU held differently, summed since Init
};

template<typename T>
class InstanceBatch {
public:
	struct Config {
		const char* name = "kt1";
		int idCapacity = 0;
		int mergeGap = 16;    // DESIGN: unchanged instances copied along to join two changed ranges into one copy
	};

	bool Init(const Config& c, const std::vector<InstanceAttrib>& attribs) {
		config = c;
		stats = InstanceBatchStats{};
		if (!buffer.Init(c.name, sizeof(T), c.idCapacity, attribs)) return false;
		store.Init(c.idCapacity, buffer.Regions());
		ranges.reserve(c.idCapacity);
		return true;
	}

	void Kill() { buffer.Kill(); store.Init(0, 1); }
	bool Valid() const { return buffer.Valid(); }
	InstanceStore<T>& Instances() { return store; }
	const InstanceBatchStats& Stats() const { return stats; }

	// the caller has bound its shader and set the GL state; `check`: also read the region back and count differences
	void Draw(bool check = false) {
		stats.instances = store.Size();
		stats.uploaded = 0;
		stats.ranges = 0;
		if (!buffer.Valid() || store.Size() == 0) return;
		store.TakeStale(buffer.Region(), config.mergeGap, ranges);
		for (const SlotRange& r : ranges) stats.uploaded += r.count;
		stats.ranges = static_cast<int>(ranges.size());
		buffer.Upload(store.Data(), ranges);
		if (check) stats.differences += buffer.Differences(store.Data(), store.Size());
		buffer.Draw(store.Size());
		stats.draws++;
	}

private:
	Config config;
	InstanceBuffer buffer;
	InstanceStore<T> store;
	std::vector<SlotRange> ranges;
	InstanceBatchStats stats;
};

} // namespace KT1

#endif
