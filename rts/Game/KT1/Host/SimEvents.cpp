/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: engine events into Core/SimHost batches. See SimEvents.h.

#include "SimEvents.h"

#include <string>
#include <vector>

#include "Check.h"
#include "Fakes.h"

#include "Game/GameHelper.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectile.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Weapons/WeaponDef.h"
#include "System/EventClient.h"
#include "System/EventHandler.h"

namespace KT1 {
namespace {

using Core::SimHost::Event;
using Core::SimHost::EventType;

constexpr int CLIENT_ORDER = -1000000;   // DESIGN: ahead of every Lua handle (LUA_HANDLE_ORDER_RULES is 100)

// The engine's call-in name of each event type, in EventType order.
constexpr const char* CALLINS[Core::SimHost::EVENT_TYPES] = {
	"UnitCreated", "UnitFinished", "UnitFromFactory", "UnitDestroyed", "UnitGiven", "UnitTaken", "UnitDamaged",
	"UnitLoaded", "UnitUnloaded", "UnitCmdDone", "UnitIdle", "ProjectileCreated", "ProjectileDestroyed", "Explosion",
	"FeatureCreated", "FeatureDestroyed"
};

Core::SimHost::SplitEventLog eventLog;

void SetPos(Event& e, const float3& p) {
	e.pos[0] = p.x;
	e.pos[1] = p.y;
	e.pos[2] = p.z;
}

Event UnitEvent(EventType type, const CUnit* u) {
	Event e;
	e.type = type;
	e.id = u->id;
	e.def = u->unitDef->id;
	e.team = u->team;
	SetPos(e, u->pos);
	return e;
}

Event ProjectileEvent(EventType type, const CProjectile* p) {
	Event e;
	e.type = type;
	e.id = p->id;
	if (p->weapon) {
		const WeaponDef* wd = static_cast<const CWeaponProjectile*>(p)->GetWeaponDef();
		e.def = (wd != nullptr) ? wd->id : -1;
	}
	e.team = p->GetTeamID();
	e.other = static_cast<int32_t>(p->GetOwnerID());
	e.flags = p->piece ? 1 : 0;
	SetPos(e, p->pos);
	return e;
}

Event FeatureEvent(EventType type, const CFeature* f) {
	Event e;
	e.type = type;
	e.id = f->id;
	e.def = f->def->id;
	e.team = f->team;
	SetPos(e, f->pos);
	return e;
}

int32_t IdOf(const CUnit* u) { return (u != nullptr) ? u->id : -1; }

class Client final : public CEventClient {
public:
	Client() : CEventClient("[KT1SimEvents]", CLIENT_ORDER, true) {}

	bool WantsEvent(const std::string& name) override {
		if (name == "GameOver") return true;
		for (size_t t = 0; t < Core::SimHost::EVENT_TYPES; ++t)
			if (name == CALLINS[t]) return (eventLog.Wanted() & (Core::SimHost::EventMask(1) << t)) != 0;
		return false;
	}
	int GetReadAllyTeam() const override { return AllAccessTeam; }
	void GameOver(const std::vector<unsigned char>&) override { Check::Total(); }

	void UnitCreated(const CUnit* u, const CUnit* builder) override {
		Event e = UnitEvent(EventType::UnitCreated, u);
		e.other = IdOf(builder);
		PushUnit(e, u);
	}
	void UnitFinished(const CUnit* u) override { PushUnit(UnitEvent(EventType::UnitFinished, u), u); }
	void UnitIdle(const CUnit* u) override { PushUnit(UnitEvent(EventType::UnitIdle, u), u); }
	void UnitFromFactory(const CUnit* u, const CUnit* factory, bool userOrders) override {
		Event e = UnitEvent(EventType::UnitFromFactory, u);
		e.other = IdOf(factory);
		e.flags = userOrders ? 1 : 0;
		PushUnit(e, u);
	}
	void UnitDestroyed(const CUnit* u, const CUnit* attacker, int weaponDefID) override {
		Event e = UnitEvent(EventType::UnitDestroyed, u);
		e.other = IdOf(attacker);
		e.arg = weaponDefID;
		PushUnit(e, u);
	}
	void UnitGiven(const CUnit* u, int oldTeam, int newTeam) override { Transfer(EventType::UnitGiven, u, oldTeam, newTeam); }
	void UnitTaken(const CUnit* u, int oldTeam, int newTeam) override { Transfer(EventType::UnitTaken, u, oldTeam, newTeam); }
	void UnitDamaged(const CUnit* u, const CUnit* attacker, float damage, int weaponDefID, int projectileID, bool paralyzer) override {
		Event e = UnitEvent(EventType::UnitDamaged, u);
		e.other = IdOf(attacker);
		e.value = damage;
		e.arg = weaponDefID;
		e.arg2 = projectileID;
		e.flags = paralyzer ? 1 : 0;
		PushUnit(e, u);
	}
	void UnitLoaded(const CUnit* u, const CUnit* transport) override { Cargo(EventType::UnitLoaded, u, transport); }
	void UnitUnloaded(const CUnit* u, const CUnit* transport) override { Cargo(EventType::UnitUnloaded, u, transport); }
	void UnitCmdDone(const CUnit* u, const Command& c) override {
		Event e = UnitEvent(EventType::UnitCmdDone, u);
		e.arg = c.GetID();
		e.arg2 = static_cast<int32_t>(c.GetTag());
		PushUnit(e, u);
	}
	void ProjectileCreated(const CProjectile* p) override { Push(ProjectileEvent(EventType::ProjectileCreated, p)); }
	void ProjectileDestroyed(const CProjectile* p) override { Push(ProjectileEvent(EventType::ProjectileDestroyed, p)); }
	bool Explosion(int weaponDefID, const WeaponDef*, const CExplosionParams& params) override {
		Event e;
		e.type = EventType::Explosion;
		e.id = weaponDefID;
		e.def = weaponDefID;
		e.other = IdOf(params.owner);
		e.arg = IdOf(params.hitObject.GetTyped<CUnit>());
		const CFeature* hit = params.hitObject.GetTyped<CFeature>();
		e.arg2 = (hit != nullptr) ? hit->id : -1;
		SetPos(e, params.pos);
		Push(e);
		return false;   // never claims the explosion's effects
	}
	void FeatureCreated(const CFeature* f) override { Push(FeatureEvent(EventType::FeatureCreated, f)); }
	void FeatureDestroyed(const CFeature* f) override { Push(FeatureEvent(EventType::FeatureDestroyed, f)); }

private:
	static void Push(const Event& e) { eventLog.Push(e, false); }
	// a unit event, about its subject: hidden from readers that do not see fakes (the units Lua gadgets do not hear)
	static void PushUnit(const Event& e, const CUnit* u) { eventLog.Push(e, Fakes::Is(u)); }
	static void Transfer(EventType type, const CUnit* u, int oldTeam, int newTeam) {
		Event e = UnitEvent(type, u);
		e.arg = oldTeam;
		e.arg2 = newTeam;
		PushUnit(e, u);
	}
	static void Cargo(EventType type, const CUnit* u, const CUnit* transport) {
		Event e = UnitEvent(type, u);
		e.other = IdOf(transport);
		PushUnit(e, u);
	}
};

// Not a static object: CEventClient's destructor calls the event handler, which may be gone by process exit.
Client* client = nullptr;

} // namespace

int SimEvents::Subscribe(Core::SimHost::EventMask mask, bool seesFakes) {
	const int reader = eventLog.AddReader(mask, seesFakes);
	if (client == nullptr) client = new Client();
	else eventHandler.RemoveClient(client);   // re-link: the wanted events have changed
	eventHandler.AddClient(client);
	return reader;
}

Core::SimHost::EventBatch SimEvents::Take(int reader) { return eventLog.Take(reader); }

void SimEvents::Compact() { eventLog.Compact(); }

void SimEvents::Reset() {
	delete client;
	client = nullptr;
	eventLog.Clear();
}

} // namespace KT1
