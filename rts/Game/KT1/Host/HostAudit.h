/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: the draw host's audit of itself. While on, every phase call of the host is wrapped in a count of the
// primitives GL generated (every draw call generates some) and a comparison of the GL state before and after.
// "idle" counts the calls made with no draw system enabled: they must generate nothing and change nothing. "busy" counts
// the others: with a drawing system their primitives show that the count sees draws, and the state must still be left.
// A test tool: reading GL back stalls the pipeline.

#ifndef KT1_HOST_HOSTAUDIT_H
#define KT1_HOST_HOSTAUDIT_H

#include <cstdint>

namespace KT1 {

struct HostAuditCounts {
	uint64_t calls = 0;          // phase calls audited
	uint64_t primitives = 0;     // primitives generated inside them
	uint64_t stateChanges = 0;   // calls after which the compared GL state differed
	uint64_t uncounted = 0;      // calls inside someone else's primitives query: no primitive count
};

class HostAudit {
public:
	static void Set(bool on);   // on: both counts start at 0
	static bool CanRead();      // false in a headless engine: only the calls are counted
	static const HostAuditCounts& Counts(bool busy);

	// around one phase call; does nothing while the audit is off
	class Scope {
	public:
		explicit Scope(bool busy);
		~Scope();
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

	private:
		HostAuditCounts* counts = nullptr;
		bool counting = false;   // this scope began the query
	};
};

} // namespace KT1

#endif
