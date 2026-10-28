/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef HAPFS_PATHSTATEIO_H
#define HAPFS_PATHSTATEIO_H

// supcom: raw binary reads and writes of the HAPFS synced state for save games.
// Saves only load on the build that wrote them, so values are written as they are
// in memory; sizes are checked on load because they depend on the map and movedefs.

#include <cinttypes>
#include <deque>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "IPath.h"
#include "System/Exceptions.h"

namespace HAPFS {
namespace StateIO {
	template<typename T> void Put(std::ostream& os, const T& v) {
		os.write(reinterpret_cast<const char*>(&v), sizeof(T));
	}
	template<typename T> void Get(std::istream& is, T& v) {
		is.read(reinterpret_cast<char*>(&v), sizeof(T));
		if (!is)
			throw content_error("[HAPFS::StateIO] save game ends inside the path state");
	}

	template<typename T> void PutVec(std::ostream& os, const std::vector<T>& v) {
		Put(os, std::uint64_t(v.size()));
		if (!v.empty())
			os.write(reinterpret_cast<const char*>(v.data()), v.size() * sizeof(T));
	}
	template<typename T> void GetVec(std::istream& is, std::vector<T>& v) {
		std::uint64_t n = 0;
		Get(is, n);
		v.resize(n);
		if (n == 0)
			return;
		is.read(reinterpret_cast<char*>(v.data()), n * sizeof(T));
		if (!is)
			throw content_error("[HAPFS::StateIO] save game ends inside the path state");
	}
	// reads a vector whose size the running game already fixed (map size, movedef count)
	template<typename T> void GetVecSized(std::istream& is, std::vector<T>& v, const char* what) {
		const std::size_t expected = v.size();
		GetVec(is, v);
		if (v.size() != expected)
			throw content_error(std::string("[HAPFS::StateIO] path state size mismatch: ") + what);
	}

	template<typename T> void PutDeque(std::ostream& os, const std::deque<T>& d) {
		Put(os, std::uint64_t(d.size()));
		for (const T& v: d)
			Put(os, v);
	}
	template<typename T> void GetDeque(std::istream& is, std::deque<T>& d) {
		std::uint64_t n = 0;
		Get(is, n);
		d.clear();
		for (std::uint64_t i = 0; i < n; i++) {
			T v;
			Get(is, v);
			d.push_back(v);
		}
	}

	inline void PutPath(std::ostream& os, const IPath::Path& p) {
		PutVec(os, p.path);
		PutVec(os, p.squares);
		Put(os, p.desiredGoal);
		Put(os, p.pathGoal);
		Put(os, p.goalRadius);
		Put(os, p.pathCost);
	}
	inline void GetPath(std::istream& is, IPath::Path& p) {
		GetVec(is, p.path);
		GetVec(is, p.squares);
		Get(is, p.desiredGoal);
		Get(is, p.pathGoal);
		Get(is, p.goalRadius);
		Get(is, p.pathCost);
	}
}
}

#endif
