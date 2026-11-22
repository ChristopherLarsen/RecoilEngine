/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_PATH_REQUESTS_H
#define SUPCOM_PATH_REQUESTS_H

#include "System/Threading/ThreadPool.h"
#include "System/Threading/SupComThreading.h"

namespace SupComThreading {
	// see PathRequestsSerial: serially inside a parallel-section flag (so the path cache rules match the threaded run)
	template<typename F> void ForPathRequests(int n, const F& f) {
		if (PathRequestsSerial()) {
			MultithreadedSection mtSection;
			for (int i = 0; i < n; ++i)
				f(i);
			return;
		}
		for_mt(0, n, f);
	}
}

#endif
