/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/Trace (LUA-FREE-WORKPACKAGES H02): the trace file and the replay, and SimProbe's replay case.

#include "../KT1Test.h"
#include "../KT1TraceCase.h"

#include "Game/KT1/Core/SimProbe/SimProbeCore.h"
#include "Game/KT1/Core/Trace/TraceColumns.h"
#include "Game/KT1/Core/Trace/TraceFile.h"
#include "Game/KT1/Core/Trace/TraceReplay.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace KT1::Core::Trace;
namespace SimHost = KT1::Core::SimHost;

namespace {
	// A system that sums its inputs per id: output "sum" (id) and the ordered output "order" (the ids as they came).
	struct Summer : ReplaySystem {
		std::vector<int32_t> ids;
		std::vector<float> sums = std::vector<float>(8, 0.0f);
		int32_t wrongId = -1;     // gives a wrong sum for this id

		void Input(std::string_view, ByteReader& in, int32_t) override {
			const int32_t id = in.Get<int32_t>();
			sums[id] += in.Get<float>();
			ids.push_back(id);
		}
		void EndStep(int32_t, Outputs& out) override {
			for (const int32_t id : ids) {
				const float order = static_cast<float>(id);
				out.Give("order", {0, 0, 0}, {{order}});
			}
			for (int32_t id = 0; id < 8; ++id) {
				const float v = sums[id] + (id == wrongId ? 1.0f : 0.0f);
				if (sums[id] != 0.0f) out.Give("sum", {id, 0, 0}, {{v}});
			}
			ids.clear();
		}
	};

	void Feed(Writer& w, int32_t step, int32_t id, float v) {
		Bytes b;
		b.Put(id);
		b.Put(v);
		w.Row(w.ColumnOf("add", INPUT), step, b.View());
	}

	void Lua(Writer& w, int32_t step, const char* column, uint8_t flags, int32_t id, float v) {
		w.Output(column, flags, step, {id, 0, 0}, {{v}});
	}

	// two steps of Summer as the Lua would have recorded them: ids 5 then 2, then 5 again
	std::vector<uint8_t> Recorded(bool end = true, bool swapOrder = false) {
		Writer w;
		w.Begin({"Summer", "engine-x", "scene-y", 30});
		Feed(w, 7, 5, 1.5f);
		Feed(w, 7, 2, 2.0f);
		Lua(w, 7, "order", ORDERED, 0, swapOrder ? 2.0f : 5.0f);
		Lua(w, 7, "order", ORDERED, 0, swapOrder ? 5.0f : 2.0f);
		Lua(w, 7, "sum", 0, 2, 2.0f);          // keyed: any order
		Lua(w, 7, "sum", 0, 5, 1.5f);
		Lua(w, 7, "path:added", NOT_COMPARED, 0, 0.0f);
		Feed(w, 8, 5, 0.25f);
		Lua(w, 8, "order", ORDERED, 0, 5.0f);
		Lua(w, 8, "sum", 0, 5, 1.75f);
		Lua(w, 8, "sum", 0, 2, 2.0f);
		if (end) w.End();
		const auto b = w.Pending();
		return std::vector<uint8_t>(b.begin(), b.end());
	}
}

KT1_CASE("TRACE-FORMAT", "a trace reads back as written: header, columns once, rows in order, a complete end") {
	const std::vector<uint8_t> t = Recorded();
	Reader r;
	KT1_CHECK(r.Open(t));
	KT1_CHECK_EQ(r.Head().system, std::string("Summer"));
	KT1_CHECK_EQ(r.Head().scene, std::string("scene-y"));
	KT1_CHECK_EQ(r.Head().stepsPerSecond, 30u);
	RowView row;
	int rows = 0;
	while (r.Next(row)) ++rows;
	KT1_CHECK_EQ(rows, 11);
	KT1_CHECK(r.Complete());
	KT1_CHECK_EQ(r.Columns().size(), static_cast<size_t>(4));
	KT1_CHECK(r.Columns()[0].IsInput() && r.Columns()[1].Ordered() && !r.Columns()[3].Compared());
}

KT1_CASE("TRACE-DAMAGED", "a cut or foreign file is an error, never a clean replay") {
	Summer s;
	KT1_CHECK(!Replay(Recorded(false), s).Clean());
	std::vector<uint8_t> t = Recorded();
	t.resize(t.size() - 7);
	Summer s2;
	KT1_CHECK(!Replay(t, s2).error.empty());
	Summer s3;
	KT1_CHECK(!Replay(std::vector<uint8_t>(64, 3), s3).error.empty());
	Summer s4;
	KT1_CHECK(!Replay(std::vector<uint8_t>(), s4).error.empty());
}

KT1_CASE("TRACE-REPLAY", "a replay that gives what was recorded has no difference; keyed columns match in any order") {
	Summer s;
	const ReplayResult r = Replay(Recorded(), s);
	KT1_CHECK(r.Clean());
	KT1_CHECK_EQ(r.steps, 2u);
	KT1_CHECK_EQ(r.inputs, static_cast<uint64_t>(3));
	KT1_CHECK_EQ(r.compared, static_cast<uint64_t>(7));   // 3 ordered + 4 keyed; the path is not compared
}

KT1_CASE("TRACE-DIFF", "a wrong value, a wrong order, a missing row and an extra row are each found, with the first step") {
	Summer wrong;
	wrong.wrongId = 2;
	const ReplayResult a = Replay(Recorded(), wrong);
	KT1_CHECK_EQ(a.differences, static_cast<uint64_t>(2));
	KT1_CHECK_EQ(a.firstDiffStep, 7);
	Summer ordered;
	KT1_CHECK_EQ(Replay(Recorded(true, true), ordered).differences, static_cast<uint64_t>(2));
	struct Silent : Summer { void EndStep(int32_t, Outputs&) override {} } silent;
	KT1_CHECK_EQ(Replay(Recorded(), silent).differences, static_cast<uint64_t>(7));
	struct Extra : Summer {
		void EndStep(int32_t step, Outputs& out) override {
			Summer::EndStep(step, out);
			out.Give("sum", {1, 0, 0}, {{9.0f}});   // a key the Lua never gave: one difference, the others still match
		}
	} extra;
	KT1_CHECK_EQ(Replay(Recorded(), extra).differences, static_cast<uint64_t>(2));
}

KT1_CASE("TRACE-FIRST-STEP", "a row of the step before the game starts is replayed like any other") {
	Writer w;
	w.Begin({"Summer", "e", "s", 30});
	Feed(w, -1, 3, 1.0f);
	Lua(w, -1, "order", ORDERED, 0, 3.0f);
	Lua(w, -1, "sum", 0, 3, 1.0f);
	w.End();
	Summer s;
	const ReplayResult r = Replay(w.Pending(), s);
	KT1_CHECK(r.Clean());
	KT1_CHECK_EQ(r.steps, 1u);
}

KT1_CASE("TRACE-COLUMNS", "a column row replays under the check helper's keys") {
	Outputs out;
	const ReplayColumns give{out, 9};
	give("flag", true, true);
	give("pair", std::array<float, 2>{{1.5f, 2.5f}}, true);
	give("ids", std::vector<int32_t>{4, 6}, true);
	give("kept", 3.0f, false);   // not compared: not given
	std::string seen;
	for (const auto& [column, rows] : out.Rows())
		for (const Value& v : rows)
			seen += column + " " + std::to_string(v.key[0]) + "/" + std::to_string(v.key[1]) + "/" + std::to_string(v.key[2]) +
				"=" + std::to_string(static_cast<int>(v.v[0] * 2.0f)) + ";";
	KT1_CHECK_EQ(seen, std::string("flag 9/0/0=2;ids 9/0/1=4;ids 9/0/0=8;ids 9/1/0=12;pair 9/0/0=3;pair 9/1/0=5;"));
}

KT1_CASE("TRACE-COVERAGE", "coverage counts the steps and rows of every column") {
	Coverage c;
	for (const int step : {3, 3, 4, 9}) c.Row("turn", step);
	c.Row("path:x", 3);
	std::string out;
	c.Lines("Orders", [&](const std::string& l) { out += l + ";"; });
	KT1_CHECK_EQ(out, std::string("[KT1TRACE] Orders coverage path:x steps=1 rows=1;[KT1TRACE] Orders coverage turn steps=3 rows=4;"));
}

namespace {
	// SimProbe: Rules/SimProbe records the inputs "init", "frame" (its FrameStart ran), "event" and "command";
	// dev_kt1_simprobe.lua gives "state" per unit per frame and, through the check helper, "AllowCommand".
	struct SimProbeReplay : ReplaySystem {
		KT1::Core::SimProbe::State state;
		std::vector<std::pair<int32_t, int32_t>> commands;
		int32_t deny = 0;
		bool frame = false;

		void Input(std::string_view column, ByteReader& in, int32_t) override {
			if (column == "init") {
				state.Init(in.Get<int32_t>());
				deny = in.Get<int32_t>();
				return;
			}
			if (column == "frame") {
				frame = true;
				return;
			}
			if (column == "command") {
				const int32_t unit = in.Get<int32_t>();
				commands.emplace_back(unit, in.Get<int32_t>());
				return;
			}
			SimHost::Event e;
			e.type = static_cast<SimHost::EventType>(in.Get<uint8_t>());
			e.id = in.Get<int32_t>();
			e.value = in.Get<float>();
			state.Apply(e);
		}
		void EndStep(int32_t, Outputs& out) override {
			for (const auto& [unit, cmd] : commands) {
				const float allow = KT1::Core::SimProbe::Allows(deny, cmd) ? 1.0f : 0.0f;
				out.Give("AllowCommand", {unit, cmd, 0}, {{allow}});
			}
			commands.clear();
			if (!frame) return;
			frame = false;
			for (const int32_t id : state.units.index.AscendingIds()) {
				const auto slot = state.units.index.SlotOf(id);
				const float row[2] = {state.damage[slot], static_cast<float>(state.events[slot])};
				out.Give("state", {id, 0, 0}, row);
			}
		}
	};
}

KT1_TRACE_CASE(SimProbe, "SimProbe.kt1t") { return std::make_unique<SimProbeReplay>(); }
