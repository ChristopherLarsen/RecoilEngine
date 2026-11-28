/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free Core tests: the case registry and the failure record.

#include "KT1Test.h"

#include <atomic>
#include <cstdlib>
#include <new>

#ifdef _WIN32
	#include <malloc.h>
#endif

namespace {
	std::atomic<long long> allocations = 0;

	void* Counted(void* p) {
		if (p == nullptr)
			throw std::bad_alloc();
		++allocations;
		return p;
	}
}

// The default array and nothrow forms call these.
void* operator new(std::size_t n) { return Counted(std::malloc(n > 0 ? n : 1)); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

#ifdef _WIN32
void* operator new(std::size_t n, std::align_val_t a) { return Counted(_aligned_malloc(n > 0 ? n : 1, static_cast<std::size_t>(a))); }
void operator delete(void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }
#endif

namespace KT1::Test {
	long long AllocationCount() {
		return allocations;
	}

	std::vector<CaseDef>& Registry() {
		static std::vector<CaseDef> cases;   // first use: static registrars run in no fixed order
		return cases;
	}

	void Case::Fail(const std::string& what) {
		if (failures++ > 0)
			return;

		first = what;
		if (stepsPerSecond > 0)
			first += " (at " + std::to_string(stepsPerSecond) + " steps/s)";
	}

	void Case::Fail(const char* file, int line, const std::string& what) {
		const char* name = file;
		for (const char* c = file; *c != '\0'; ++c)
			if (*c == '/' || *c == '\\')
				name = c + 1;

		Fail(std::string(name) + ":" + std::to_string(line) + " " + what);
	}

	std::string Case::Report() const {
		if (failures <= 1)
			return first;

		return first + " (+" + std::to_string(failures - 1) + " more)";
	}
}
