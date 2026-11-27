/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_PATH_REQUESTS_H
#define SUPCOM_PATH_REQUESTS_H

#include "System/Threading/ThreadPool.h"
#include "System/Threading/SupComThreading.h"

namespace SupComThreading {
	// see PathRequestsSerial: serially inside a parallel-section flag (so the path cache rules match the threaded run)
	template<typename F> void ForPathRequests(int n, const F& f) {
		if (PathRequestsSerial()) {
			if (n >= PATH_LANES && PathLanes()) {
				// see PathLanes: lane k runs the k-th contiguous slice in order with finder group k
				for_mt(0, PATH_LANES, [&](int lane) {
					const int prev = pathLane;
					pathLane = lane;
					const int b = int((long long)n * lane / PATH_LANES);
					const int e = int((long long)n * (lane + 1) / PATH_LANES);
					for (int i = b; i < e; ++i)
						f(i);
					pathLane = prev;
				});
				return;
			}
			MultithreadedSection mtSection;
			const int prev = pathLane;
			pathLane = 0;
			for (int i = 0; i < n; ++i)
				f(i);
			pathLane = prev;
			return;
		}
		for_mt(0, n, f);
	}
}

#endif
