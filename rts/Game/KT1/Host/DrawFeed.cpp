/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: engine events into the draw feed (see Draw/Common/DrawFeed.h).
//
// The client is unsynced and reads as LuaUI does (the local ally team, or everything with a full view), so the event
// handler itself holds back what the viewer may not hear. It only copies values into a queue: no draw code runs inside
// a sim step. With no listener there is no client.

#include "Game/KT1/Draw/Common/DrawFeed.h"

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "ViewerRules.h"

#include "Sim/Features/FeatureDef.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/EventClient.h"
#include "System/EventHandler.h"

namespace KT1::DrawFeed {
namespace {

constexpr int CLIENT_ORDER = -1000000;   // DESIGN: ahead of every Lua handle, so events are recorded as raised

// the engine's call-in of each kind, in Kind order
constexpr const char* CALLINS[static_cast<int>(Kind::Count)] = {
	"UnitCreated", "UnitFinished", "UnitDestroyed", "RenderUnitDestroyed", "UnitEnteredLos", "UnitLeftLos",
	"UnitEnteredRadar", "UnitLeftRadar", "UnitTaken", "UnitGiven", "UnitCloaked", "UnitDecloaked", "UnitDamaged",
	"FeatureCreated", "FeatureDestroyed", "PlayerChanged"
};

struct Reader {
	Mask kinds = 0;
	Listener fn;
};

struct Feed {
	std::deque<Reader> readers;   // a deque: a Listen during delivery moves no reader
	std::vector<Event> queued;
	std::vector<Event> out;       // being delivered
	Mask wanted = 0;
};

Feed& State() {
	static Feed feed;
	return feed;
}

void Push(const Event& e) {
	if (State().wanted & Bit(e.kind)) State().queued.push_back(e);
}

// the attacker or builder, when the viewer sees it
int SeenId(const CUnit* u) { return (u != nullptr && ViewerRules::Seen(*u)) ? u->id : -1; }

Event UnitEvent(Kind kind, const CUnit& u) {
	Event e;
	e.kind = kind;
	e.id = u.id;
	e.defId = ViewerRules::DefId(u);
	e.team = u.team;
	e.allyTeam = u.allyteam;
	e.step = gs->frameNum;
	return e;
}

Event FeatureEvent(Kind kind, const CFeature& f) {
	Event e;
	e.kind = kind;
	e.id = f.id;
	e.defId = ViewerRules::Seen(f) ? f.def->id : 0;
	e.team = (f.allyteam < 0) ? -1 : f.team;
	e.allyTeam = f.allyteam;
	e.step = gs->frameNum;
	return e;
}

class Client final : public CEventClient {
public:
	Client() : CEventClient("[KT1DrawFeed]", CLIENT_ORDER, false) {}

	bool WantsEvent(const std::string& name) override {
		for (int k = 0; k < static_cast<int>(Kind::Count); k++)
			if (name == CALLINS[k]) return (State().wanted & Bit(static_cast<Kind>(k))) != 0;
		return false;
	}
	// asked at every event, so a change of view takes effect at once
	int GetReadAllyTeam() const override { return gu->spectatingFullView ? AllAccessTeam : gu->myAllyTeam; }

	void UnitCreated(const CUnit* u, const CUnit* builder) override {
		Event e = UnitEvent(Kind::UnitCreated, *u);
		e.other = SeenId(builder);
		Push(e);
	}
	void UnitFinished(const CUnit* u) override { Push(UnitEvent(Kind::UnitFinished, *u)); }
	void UnitDestroyed(const CUnit* u, const CUnit* attacker, int weaponDefID) override {
		Event e = UnitEvent(Kind::UnitDestroyed, *u);
		e.other = SeenId(attacker);
		e.weaponDefId = weaponDefID;
		Push(e);
	}
	// every unit reaches this one: the viewer's sight is checked here
	void RenderUnitDestroyed(const CUnit* u) override {
		if (ViewerRules::Seen(*u)) Push(UnitEvent(Kind::UnitRemoved, *u));
	}
	void UnitEnteredLos(const CUnit* u, int at) override { Sight(Kind::UnitEnteredLos, *u, at); }
	void UnitLeftLos(const CUnit* u, int at) override { Sight(Kind::UnitLeftLos, *u, at); }
	void UnitEnteredRadar(const CUnit* u, int at) override { Sight(Kind::UnitEnteredRadar, *u, at); }
	void UnitLeftRadar(const CUnit* u, int at) override { Sight(Kind::UnitLeftRadar, *u, at); }
	void UnitTaken(const CUnit* u, int oldTeam, int newTeam) override { Transfer(Kind::UnitTaken, *u, oldTeam, newTeam); }
	void UnitGiven(const CUnit* u, int oldTeam, int newTeam) override { Transfer(Kind::UnitGiven, *u, oldTeam, newTeam); }
	void UnitCloaked(const CUnit* u) override { Push(UnitEvent(Kind::UnitCloaked, *u)); }
	void UnitDecloaked(const CUnit* u) override { Push(UnitEvent(Kind::UnitDecloaked, *u)); }
	void UnitDamaged(const CUnit* u, const CUnit* attacker, float damage, int weaponDefID, int, bool paralyzer) override {
		Event e = UnitEvent(Kind::UnitDamaged, *u);
		e.other = SeenId(attacker);
		e.damage = damage;
		e.weaponDefId = weaponDefID;
		e.paralyzer = paralyzer;
		Push(e);
	}
	void FeatureCreated(const CFeature* f) override { Push(FeatureEvent(Kind::FeatureCreated, *f)); }
	void FeatureDestroyed(const CFeature* f) override { Push(FeatureEvent(Kind::FeatureDestroyed, *f)); }
	void PlayerChanged(int playerID) override {
		Event e;
		e.kind = Kind::PlayerChanged;
		e.id = playerID;
		e.step = gs->frameNum;
		Push(e);
	}

private:
	static void Sight(Kind kind, const CUnit& u, int allyTeam) {
		Event e = UnitEvent(kind, u);
		e.allyTeam = allyTeam;
		Push(e);
	}
	static void Transfer(Kind kind, const CUnit& u, int oldTeam, int newTeam) {
		Event e = UnitEvent(kind, u);
		e.team = newTeam;
		e.oldTeam = oldTeam;
		Push(e);
	}
};

// Not a static object: CEventClient's destructor calls the event handler, which may be gone by process exit.
Client* client = nullptr;

} // namespace

void Listen(Mask kinds, Listener listener) {
	Feed& feed = State();
	feed.readers.push_back({kinds, std::move(listener)});
	if (client != nullptr && (feed.wanted | kinds) == feed.wanted) return;
	feed.wanted |= kinds;
	if (client == nullptr) client = new Client();
	else eventHandler.RemoveClient(client);   // re-link: the wanted events have changed
	eventHandler.AddClient(client);
}

void Deliver() {
	Feed& feed = State();
	if (feed.queued.empty()) return;
	feed.out.swap(feed.queued);   // an event raised by a listener waits for the next frame
	for (const Event& e : feed.out) {
		for (size_t i = 0; i < feed.readers.size(); i++) {
			if (feed.readers[i].kinds & Bit(e.kind)) feed.readers[i].fn(e);
		}
	}
	feed.out.clear();
}

void Reset() {
	delete client;   // ~CEventClient removes it from the event handler
	client = nullptr;
	Feed& feed = State();
	feed.readers.clear();
	feed.queued.clear();
	feed.out.clear();
	feed.wanted = 0;
}

} // namespace KT1::DrawFeed
