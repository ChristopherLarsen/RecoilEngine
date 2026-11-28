/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: which rules a call starts. No engine include.
//
// Synced rules start once, at game setup: the same moment on every machine, before any unit exists and before any
// validator can be asked, so per-def data is there. Unsynced rules start once, at the first sim step or drawn frame,
// which differs between machines; nothing synced may depend on it.
// A rule here is a pointer-like to something with Enabled(), Info().synced and Start().

#ifndef KT1_START_GATE_H
#define KT1_START_GATE_H

namespace KT1::Core::SimHost {

class StartGate {
public:
	template<class Rules> void Setup(Rules& rules) { Start(rules, true, syncedDone); }
	// from a sim step or a drawn frame: never a synced rule
	template<class Rules> void StepOrDraw(Rules& rules) { Start(rules, false, unsyncedDone); }
	void Reset() { syncedDone = unsyncedDone = false; }

private:
	template<class Rules> static void Start(Rules& rules, bool synced, bool& done) {
		if (done) return;
		done = true;
		for (auto& r : rules)
			if (r->Enabled() && r->Info().synced == synced) r->Start();
	}

	bool syncedDone = false;
	bool unsyncedDone = false;
};

} // namespace KT1::Core::SimHost

#endif
