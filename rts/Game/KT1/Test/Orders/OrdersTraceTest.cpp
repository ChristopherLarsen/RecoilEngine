/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/Orders: the busy-unit turn in a trace (TurnTrace.h): the input row's round trip and a replay of a small trace, at
// 30 and at 10 steps per second.

#include <memory>
#include <string_view>
#include <vector>

#include "../KT1Test.h"

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/Orders/OrdersConfig.h"
#include "Game/KT1/Core/Orders/TurnTrace.h"

using namespace KT1::Core::Orders;
using KT1::Core::Rate;
namespace Trace = KT1::Core::Trace;

namespace {
	using K = CommandKind;

	TurnInit Init() {
		std::vector<DefFacts> defs(3);
		defs[2].Set(DefFlag::Assists, true);
		return MakeTurnInit(OrdersConfig{}, defs, 64, 1024, 2048);
	}

	OrderView Entry(K kind, uint32_t tag) {
		OrderView v;
		v.id = 40 + static_cast<int32_t>(tag);
		v.kind = kind;
		v.tag = tag;
		return v;
	}

	// every field away from its default
	TurnInput Full() {
		TurnInput in;
		in.hasInit = true;
		in.init = Init();
		in.now = 77;
		in.events = {{TurnEventKind::Join, 5}, {TurnEventKind::Leave, 6}};
		in.netUnits = {5, 9};
		TurnUnit u;
		u.unit = 5;
		u.defId = 2;
		u.queueLen = 11;
		u.queueLenAfter = 12;
		u.gone = true;
		OrderView first = Entry(K::Repair, 3);
		first.options = 8;
		first.internal = true;
		first.shift = true;
		first.numParams = 12;   // more than are kept
		for (size_t i = 0; i < ORDER_PARAMS; ++i)
			first.params[i] = static_cast<float>(i) + 0.25f;
		const std::vector<OrderView> q = {first, Entry(K::Guard, 4)};
		SetTurnQueue(u, q);
		u.own = {3, OwnKind::Assist, 21};
		u.walking = true;
		u.walk.count = 2;
		u.walk.tags[0] = 30;
		u.walk.tags[1] = 31;
		u.given = {{K::Reclaim, 33}, {K::Repair, 34}};
		u.hasTask = true;
		u.taskTag = 99;
		in.units = {u, TurnUnit{}};
		in.units[1].unit = 9;
		return in;
	}

	std::vector<uint8_t> Written(const TurnInput& in) {
		Trace::Bytes b;
		Put(b, in);
		return {b.View().begin(), b.View().end()};
	}

	bool Read(std::span<const uint8_t> bytes, TurnInput& in) {
		Trace::ByteReader r(bytes);
		return Get(r, in);
	}

	void CheckUnit(KT1::Test::Case& testCase, const TurnUnit& a, const TurnUnit& b) {
		KT1_CHECK(a.unit == b.unit && a.defId == b.defId && a.queueLen == b.queueLen);
		KT1_CHECK(a.queueLenAfter == b.queueLenAfter && a.gone == b.gone && a.numQueue == b.numQueue);
		for (size_t i = 0; i < a.numQueue; ++i) {
			const OrderView& x = a.queue[i];
			const OrderView& y = b.queue[i];
			KT1_CHECK(x.id == y.id && x.kind == y.kind && x.tag == y.tag && x.options == y.options);
			KT1_CHECK(x.internal == y.internal && x.shift == y.shift && x.numParams == y.numParams);
			KT1_CHECK(x.params == y.params);
		}
		KT1_CHECK(a.own.tag == b.own.tag && a.own.kind == b.own.kind && a.own.target == b.own.target);
		KT1_CHECK(a.walking == b.walking && a.walk.count == b.walk.count && a.walk.tags == b.walk.tags);
		KT1_CHECK_EQ(a.given.size(), b.given.size());
		for (size_t i = 0; i < a.given.size() && i < b.given.size(); ++i)
			KT1_CHECK(a.given[i].kind == b.given[i].kind && a.given[i].target == b.given[i].target);
		KT1_CHECK(a.hasTask == b.hasTask && a.taskTag == b.taskTag);
	}
}

KT1_RATE_CASE("ORD-TRACE-ROUNDTRIP", "an input row gives back every field") {
	const TurnInput a = Full();
	const std::vector<uint8_t> bytes = Written(a);
	TurnInput b;
	KT1_CHECK(Read(bytes, b));
	KT1_CHECK(b.hasInit && b.now == 77 && b.netUnits == a.netUnits);
	const TurnInit& x = a.init;
	const TurnInit& y = b.init;
	KT1_CHECK(y.stepsPerSecond == testCase.StepsPerSecond() && y.maxUnits == 64 && y.mapX == 1024 && y.mapZ == 2048);
	KT1_CHECK(x.guardCheckS == y.guardCheckS && x.patrolCheckS == y.patrolCheckS && x.fightCheckS == y.fightCheckS);
	KT1_CHECK(x.safetyNetS == y.safetyNetS && x.reachEveryS == y.reachEveryS);
	KT1_CHECK(x.reachWalkingEveryS == y.reachWalkingEveryS && x.defFlags == y.defFlags && y.defFlags.size() == 3);
	KT1_CHECK(b.events.size() == 2 && b.events[1].kind == TurnEventKind::Leave && b.events[1].unit == 6);
	KT1_CHECK_EQ(b.units.size(), size_t(2));
	if (b.units.size() == 2) {
		CheckUnit(testCase, a.units[0], b.units[0]);
		CheckUnit(testCase, a.units[1], b.units[1]);
	}
	KT1_CHECK(Written(b) == bytes);
	// a later row has no init
	TurnInput later = a;
	later.hasInit = false;
	KT1_CHECK(Read(Written(later), b) && !b.hasInit && b.init.defFlags.empty() && b.units.size() == 2);
}

KT1_RATE_CASE("ORD-TRACE-BAD", "a short, long or out-of-range row is refused") {
	std::vector<uint8_t> bytes = Written(Full());
	TurnInput b;
	size_t accepted = 0;
	for (size_t n = 0; n < bytes.size(); ++n)
		accepted += Read(std::span<const uint8_t>(bytes.data(), n), b) ? 1 : 0;
	KT1_CHECK_EQ(accepted, size_t(0));
	bytes.push_back(0);
	KT1_CHECK(!Read(bytes, b));
	TurnInput bad = Full();
	bad.units[0].queue[0].kind = K::Count;
	KT1_CHECK(!Read(Written(bad), b));
	bad = Full();
	bad.events[0].kind = static_cast<TurnEventKind>(7);
	KT1_CHECK(!Read(Written(bad), b));
	bad = Full();
	bad.init.stepsPerSecond = 0;
	KT1_CHECK(!Read(Written(bad), b));
}

namespace {
	// the replay of the system, as a trace case makes it
	class TurnReplay: public Trace::ReplaySystem {
	public:
		void Input(std::string_view, Trace::ByteReader& bytes, int32_t) override { got = Get(bytes, in); }
		void EndStep(int32_t, Trace::Outputs& columns) override {
			if (!got)
				return;
			RunTurnStep(state, params, in, out);
			GiveTurnColumns(out, columns);
			got = false;
		}

	private:
		State state;
		TurnParams params;
		TurnInput in;
		TurnOutput out;
		bool got = false;
	};

	void Answer(Trace::Writer& w, std::string_view column, Step step, Trace::Key key, float value) {
		w.Output(column, 0, step, key, {{value}});
	}

	// two steps as the Lua would record them: a patrol (unit 13) and an idle unit (3) that leaves
	std::vector<uint8_t> SmallTrace(int stepsPerSecond, float spoil) {
		Trace::Writer w;
		w.Begin({"Orders", "test", "small", static_cast<uint32_t>(stepsPerSecond)});
		const TurnInit init = Init();
		const float interval = static_cast<float>(Rate::Every(OrdersConfig{}.patrol.patrol_check_s));
		const float phase = 1 + 1 + static_cast<float>(13 % static_cast<int>(interval));
		for (const Step f: {1, 2}) {
			TurnInput in;
			in.hasInit = f == 1;
			in.init = init;
			in.now = f;
			if (f == 1)
				in.events = {{TurnEventKind::Join, 3}, {TurnEventKind::Join, 13}};
			TurnUnit patrol;
			patrol.unit = 13;
			patrol.defId = 1;
			patrol.queueLen = patrol.queueLenAfter = 1;
			const std::vector<OrderView> q = {Entry(K::Patrol, 1)};
			SetTurnQueue(patrol, q);
			TurnUnit idle;
			idle.unit = 3;
			in.units = {idle, patrol};
			if (f == 2)
				in.units = {patrol};
			Trace::Bytes b;
			Put(b, in);
			w.Row(w.ColumnOf("turn", Trace::INPUT), f, b.View());
			const Step visits = f == 1 ? 2 : 1;
			Answer(w, "turn_list", f, {0, 0, 1}, static_cast<float>(visits));
			if (f == 1) {
				Answer(w, "turn_list", f, {0, 0, 0}, 3);
				Answer(w, "front_class", f, {3, 0, 0}, static_cast<float>(FrontClass::None));
				Answer(w, "next_check", f, {3, 0, 0}, -1);
			}
			Answer(w, "turn_list", f, {0, visits - 1, 0}, 13);
			Answer(w, "front_class", f, {13, 0, 0}, static_cast<float>(FrontClass::OwnFightOrPatrol));
			// the first check is at once; the next falls on the unit's phase and is not due at step 2 yet
			Answer(w, "next_check", f, {13, 0, 0}, f == 1 ? phase : phase + spoil);
		}
		w.End();
		return {w.Pending().begin(), w.Pending().end()};
	}
}

KT1_RATE_CASE("ORD-TRACE-REPLAY", "a recorded trace replays clean, and a different answer is found") {
	TurnReplay clean;
	const Trace::ReplayResult r = Trace::Replay(SmallTrace(testCase.StepsPerSecond(), 0), clean);
	KT1_CHECK_MSG(r.Clean(), r.error + (r.described.empty() ? "" : r.described[0]));
	KT1_CHECK(r.steps == 2 && r.inputs == 2 && r.compared == 11);
	TurnReplay spoiled;
	const Trace::ReplayResult d = Trace::Replay(SmallTrace(testCase.StepsPerSecond(), 1), spoiled);
	KT1_CHECK(d.differences == 1 && d.firstDiffStep == 2);
}
