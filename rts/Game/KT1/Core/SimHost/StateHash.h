/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: the checksum of a system's own synced state (SYNC-RULES S1.6). No engine include.
//
// A system's state is written field by field, id first, in ascending id order. Only plain numbers go in: a struct's
// padding bytes differ between machines, so there is no way to add a struct whole.

#ifndef KT1_STATE_HASH_H
#define KT1_STATE_HASH_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace KT1::Core::SimHost {

class StateHash {
public:
	// KT1 hot
	template<class T> void Add(T v) {
		static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>, "add plain numbers, one field at a time");
		unsigned char b[sizeof(T)];
		std::memcpy(b, &v, sizeof(T));
		for (size_t i = 0; i < sizeof(T); ++i) { h ^= b[i]; h *= PRIME; }
	}

	uint32_t Get() const { return static_cast<uint32_t>(h ^ (h >> 32)); }

private:
	static constexpr uint64_t BASIS = 1469598103934665603ull;   // FNV-1a 64, as KT1 SysHash
	static constexpr uint64_t PRIME = 1099511628211ull;
	uint64_t h = BASIS;
};

} // namespace KT1::Core::SimHost

#endif
