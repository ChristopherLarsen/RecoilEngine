/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: check mode is compute-only (KT1Register.h). No engine include.
//
// The host says which system's code is running; every write of that system asks Allow() first. In check mode the write
// does not happen and is counted on the system's total line (writes_skipped). Sim thread only.

#ifndef KT1_WRITE_GATE_H
#define KT1_WRITE_GATE_H

#include "CheckTally.h"

namespace KT1::Core::SimHost {

class WriteGate {
public:
	// The system running for the life of the scope; the one before it (a nested call) comes back at its end.
	class Scope {
	public:
		Scope(WriteGate& gate, const char* system, bool check) : gate(gate), before(gate.running) { gate.running = Running{system, check}; }
		~Scope() { gate.running = before; }
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

	private:
		struct Running {
			const char* system = nullptr;
			bool check = false;
		};
		friend class WriteGate;
		WriteGate& gate;
		Running before;
	};

	// True: do the write. False: the running system is in check mode.
	// KT1 hot
	bool Allow(CheckTally& tally) const {
		if (!running.check) return true;
		tally.Skipped(running.system, 1);
		return false;
	}

private:
	Scope::Running running;
};

} // namespace KT1::Core::SimHost

#endif
