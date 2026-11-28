/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: which rules a call starts. No engine include.
//
// Synced rules start once, at game setup: the same moment on every machine, before any unit exists and before any
// validator can be asked, so per-def data is there. (A path that never ran game setup starts them at its first sim step.)
// Unsynced rules start once, at the first drawn frame, which differs between machines; nothing synced may depend on it,
// and an engine that draws nothing never starts them.
// A rule here is a pointer-like to something with Enabled(), Info().synced and Start().

#ifndef KT1_START_GATE_H
#define KT1_START_GATE_H

namespace KT1::Core::SimHost {

class StartGate {
public:
	// game setup, and each sim step for a path without one; true when this call started the synced rules
	template<class Rules> bool SetupIfNeeded(Rules& rules) { return Start(rules, true, syncedDone); }
	// each drawn frame: never a synced rule
	template<class Rules> void Unsynced(Rules& rules) { Start(rules, false, unsyncedDone); }
	void Reset() { syncedDone = unsyncedDone = false; }

private:
	template<class Rules> static bool Start(Rules& rules, bool synced, bool& done) {
		if (done) return false;
		done = true;
		for (auto& r : rules)
			if (r->Enabled() && r->Info().synced == synced) r->Start();
		return true;
	}

	bool syncedDone = false;
	bool unsyncedDone = false;
};

} // namespace KT1::Core::SimHost

#endif
