/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the serializers and columns behind TurnTrace.h. Every field is written by itself (no padding), in the
// order of the structs in TurnStep.h; a bool is one byte. No engine include.

#include "TurnTrace.h"

#include "Game/KT1/Core/Trace/TraceColumns.h"

namespace KT1::Core::Orders {

namespace {

using Trace::ByteReader;
using Trace::Bytes;

void PutBool(Bytes& b, bool v) { b.Put(static_cast<uint8_t>(v ? 1 : 0)); }

// false: a byte that is neither 0 nor 1
bool GetBool(ByteReader& b, bool& v) {
	const uint8_t raw = b.Get<uint8_t>();
	v = raw == 1;
	return raw <= 1;
}

void PutCount(Bytes& b, size_t n) { b.Put(static_cast<uint32_t>(n)); }

// false: more entries than bytes are left (each entry takes at least one)
bool GetCount(ByteReader& b, size_t& n) {
	n = b.Get<uint32_t>();
	if (n <= b.Left())
		return true;
	n = 0;
	return false;
}

bool GetKind(ByteReader& b, CommandKind& kind) {
	const uint8_t raw = b.Get<uint8_t>();
	if (raw >= static_cast<uint8_t>(CommandKind::Count))
		return false;
	kind = static_cast<CommandKind>(raw);
	return true;
}

void PutInit(Bytes& b, const TurnInit& init) {
	b.Put(init.stepsPerSecond);
	b.Put(init.maxUnits);
	for (const float v: {init.mapX, init.mapZ, init.guardCheckS, init.patrolCheckS, init.fightCheckS, init.safetyNetS,
	                     init.reachEveryS, init.reachWalkingEveryS})
		b.Put(v);
	PutCount(b, init.defFlags.size());
	for (const uint32_t flags: init.defFlags)
		b.Put(flags);
}

bool GetInit(ByteReader& b, TurnInit& init) {
	init.stepsPerSecond = b.Get<int32_t>();
	init.maxUnits = b.Get<int32_t>();
	for (float* v: {&init.mapX, &init.mapZ, &init.guardCheckS, &init.patrolCheckS, &init.fightCheckS, &init.safetyNetS,
	                &init.reachEveryS, &init.reachWalkingEveryS})
		*v = b.Get<float>();
	size_t n = 0;
	if (!GetCount(b, n))
		return false;
	init.defFlags.resize(n);
	for (uint32_t& flags: init.defFlags)
		flags = b.Get<uint32_t>();
	return init.stepsPerSecond >= 1 && init.maxUnits >= 0;
}

// the parameters kept are the first ORDER_PARAMS of the order's own count
size_t KeptParams(const OrderView& v) { return v.numParams < ORDER_PARAMS ? v.numParams : ORDER_PARAMS; }

void PutOrder(Bytes& b, const OrderView& v) {
	b.Put(v.id);
	b.Put(static_cast<uint8_t>(v.kind));
	b.Put(v.tag);
	b.Put(v.options);
	PutBool(b, v.internal);
	PutBool(b, v.shift);
	b.Put(v.numParams);
	for (size_t i = 0; i < KeptParams(v); ++i)
		b.Put(v.params[i]);
}

bool GetOrder(ByteReader& b, OrderView& v) {
	v = OrderView{};
	v.id = b.Get<int32_t>();
	bool ok = GetKind(b, v.kind);
	v.tag = b.Get<uint32_t>();
	v.options = b.Get<uint8_t>();
	ok = GetBool(b, v.internal) && ok;
	ok = GetBool(b, v.shift) && ok;
	v.numParams = b.Get<uint8_t>();
	for (size_t i = 0; i < KeptParams(v); ++i)
		v.params[i] = b.Get<float>();
	return ok;
}

// ours[u], reachTag[u], given[u] and amove[u]
void PutLuaState(Bytes& b, const TurnUnit& u) {
	b.Put(u.own.tag);
	b.Put(static_cast<uint8_t>(u.own.kind));
	b.Put(u.own.target);
	PutBool(b, u.walking);
	PutCount(b, static_cast<size_t>(u.walk.count));
	for (int32_t i = 0; i < u.walk.count; ++i)
		b.Put(u.walk.tags[i]);
	PutCount(b, u.given.size());
	for (const GivenWork& g: u.given) {
		b.Put(static_cast<uint8_t>(g.kind));
		b.Put(g.target);
	}
	PutBool(b, u.hasTask);
	b.Put(u.taskTag);
}

bool GetLuaState(ByteReader& b, TurnUnit& u) {
	u.own.tag = b.Get<uint32_t>();
	const uint8_t kind = b.Get<uint8_t>();
	u.own.kind = static_cast<OwnKind>(kind);
	u.own.target = b.Get<int32_t>();
	bool ok = kind <= static_cast<uint8_t>(OwnKind::Follow) && GetBool(b, u.walking);
	size_t n = 0;
	if (!ok || !GetCount(b, n) || n > WALK_ORDERS)
		return false;
	u.walk = ReachWalk{};
	u.walk.count = static_cast<int32_t>(n);
	for (size_t i = 0; i < n; ++i)
		u.walk.tags[i] = b.Get<uint32_t>();
	if (!GetCount(b, n))
		return false;
	u.given.resize(n);
	for (GivenWork& g: u.given) {
		ok = GetKind(b, g.kind) && ok;
		g.target = b.Get<float>();
	}
	ok = GetBool(b, u.hasTask) && ok;
	u.taskTag = b.Get<uint32_t>();
	return ok;
}

void PutUnit(Bytes& b, const TurnUnit& u) {
	b.Put(u.unit);
	b.Put(u.defId);
	b.Put(u.queueLen);
	b.Put(u.queueLenAfter);
	PutBool(b, u.gone);
	b.Put(u.numQueue);
	for (size_t i = 0; i < u.numQueue && i < u.queue.size(); ++i)
		PutOrder(b, u.queue[i]);
	PutLuaState(b, u);
}

bool GetUnit(ByteReader& b, TurnUnit& u) {
	u.unit = b.Get<int32_t>();
	u.defId = b.Get<int32_t>();
	u.queueLen = b.Get<int32_t>();
	u.queueLenAfter = b.Get<int32_t>();
	bool ok = GetBool(b, u.gone);
	u.numQueue = b.Get<uint8_t>();
	if (u.numQueue > u.queue.size())
		return false;
	u.queue = {};
	for (size_t i = 0; i < u.numQueue; ++i)
		ok = GetOrder(b, u.queue[i]) && ok;
	return GetLuaState(b, u) && ok;
}

bool GetEvents(ByteReader& b, TurnInput& in) {
	size_t n = 0;
	if (!GetCount(b, n))
		return false;
	in.events.resize(n);
	for (TurnEvent& e: in.events) {
		const uint8_t kind = b.Get<uint8_t>();
		if (kind > static_cast<uint8_t>(TurnEventKind::Leave))
			return false;
		e.kind = static_cast<TurnEventKind>(kind);
		e.unit = b.Get<int32_t>();
	}
	if (!GetCount(b, n))
		return false;
	in.netUnits.resize(n);
	for (UnitId& u: in.netUnits)
		u = b.Get<int32_t>();
	return true;
}

} // namespace

void Put(Trace::Bytes& bytes, const TurnInput& in) {
	PutBool(bytes, in.hasInit);
	if (in.hasInit)
		PutInit(bytes, in.init);
	bytes.Put(in.now);
	PutCount(bytes, in.events.size());
	for (const TurnEvent& e: in.events) {
		bytes.Put(static_cast<uint8_t>(e.kind));
		bytes.Put(e.unit);
	}
	PutCount(bytes, in.netUnits.size());
	for (const UnitId u: in.netUnits)
		bytes.Put(u);
	PutCount(bytes, in.units.size());
	for (const TurnUnit& u: in.units)
		PutUnit(bytes, u);
}

bool Get(Trace::ByteReader& bytes, TurnInput& in) {
	if (!GetBool(bytes, in.hasInit))
		return false;
	in.init = TurnInit{};
	if (in.hasInit && !GetInit(bytes, in.init))
		return false;
	in.now = bytes.Get<int32_t>();
	size_t n = 0;
	if (!GetEvents(bytes, in) || !GetCount(bytes, n))
		return false;
	in.units.resize(n);
	for (TurnUnit& u: in.units) {
		if (!GetUnit(bytes, u))
			return false;
	}
	return !bytes.Failed() && bytes.Left() == 0;
}

void GiveTurnColumns(const TurnOutput& out, Trace::Outputs& columns) {
	for (size_t i = 0; i < out.turnList.size(); ++i) {
		const Trace::ReplayColumns unit{columns, out.turnList[i]};
		unit("front_class", out.frontClass[i], true);
		unit("next_check", out.nextCheck[i], true);
	}
	const Trace::ReplayColumns list{columns, 0};
	list("turn_list", out.turnList, true);
}

} // namespace KT1::Core::Orders
