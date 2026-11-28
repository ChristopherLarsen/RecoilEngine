/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Trace core: replaying a trace into Core and comparing, and the coverage of a trace. No engine include.
//
// A system's replay implements ReplaySystem: it takes the input rows in the order recorded, and at each step's end gives
// the outputs Core computed. They are compared with the rows recorded for that step, column by column:
//   an ordered column    row by row, in the order given
//   any other column     as a set keyed by the row's key (rows of one key in the order given)
// Values are compared bit for bit. A recorded row Core did not give, and a row Core gave that was not recorded, are
// differences; the columns marked "not compared" are skipped. Inputs of a step are all given before its EndStep.
// For a system that feeds back into its own inputs only the first differing step is evidence.

#ifndef KT1_TRACE_REPLAY_H
#define KT1_TRACE_REPLAY_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "TraceFile.h"

namespace KT1::Core::Trace {

constexpr size_t DESCRIBED = 10;   // DESIGN: differences described in a result, as the check helper does

struct Value {
	Key key{};
	std::vector<float> v;
};

// What Core computed in one step.
class Outputs {
public:
	void Give(std::string_view column, const Key& key, std::span<const float> values) {
		rows[std::string(column)].push_back(Value{key, std::vector<float>(values.begin(), values.end())});
	}
	std::map<std::string, std::vector<Value>, std::less<>>& Rows() { return rows; }
	void Clear() { rows.clear(); }

private:
	std::map<std::string, std::vector<Value>, std::less<>> rows;
};

class ReplaySystem {
public:
	virtual ~ReplaySystem() = default;
	virtual void Begin(const Header&) {}
	virtual void Input(std::string_view column, ByteReader& bytes, int32_t step) = 0;
	virtual void EndStep(int32_t step, Outputs& out) = 0;
};

struct ReplayResult {
	uint32_t steps = 0;
	uint64_t inputs = 0;
	uint64_t compared = 0;          // recorded and computed rows, a pair counted once
	uint64_t differences = 0;
	int32_t firstDiffStep = -1;
	std::vector<std::string> described;
	std::string error;              // the trace itself is bad

	bool Clean() const { return error.empty() && differences == 0 && compared > 0; }
};

namespace Detail {

inline bool SameBits(const std::vector<float>& a, const std::vector<float>& b) {
	return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
}

inline std::string Show(const Value* x) {
	if (x == nullptr) return "none";
	std::string s = std::to_string(x->key[0]) + "/" + std::to_string(x->key[1]) + "/" + std::to_string(x->key[2]) + ":";
	for (const float f : x->v) s += " " + std::to_string(f);
	return s;
}

inline void Differ(ReplayResult& r, int32_t step, const std::string& column, const Value* recorded, const Value* computed) {
	++r.differences;
	if (r.firstDiffStep < 0) r.firstDiffStep = step;
	if (r.described.size() < DESCRIBED)
		r.described.push_back("step " + std::to_string(step) + " " + column + " lua=" + Show(recorded) + " core=" + Show(computed));
}

inline void CompareColumn(ReplayResult& r, int32_t step, const std::string& column, bool ordered,
		std::vector<Value>& recorded, std::vector<Value>& computed) {
	if (!ordered) {
		const auto byKey = [](const Value& a, const Value& b) { return a.key < b.key; };
		std::stable_sort(recorded.begin(), recorded.end(), byKey);
		std::stable_sort(computed.begin(), computed.end(), byKey);
	}
	size_t i = 0, j = 0;
	while (i < recorded.size() || j < computed.size()) {
		const Value* a = (i < recorded.size()) ? &recorded[i] : nullptr;
		const Value* b = (j < computed.size()) ? &computed[j] : nullptr;
		// unordered: a key only one side has is its own difference, and the other side keeps its place
		if (!ordered && a != nullptr && b != nullptr && a->key != b->key) {
			if (a->key < b->key) b = nullptr;
			else a = nullptr;
		}
		++r.compared;
		if (a == nullptr || b == nullptr || a->key != b->key || !SameBits(a->v, b->v)) Differ(r, step, column, a, b);
		i += (a != nullptr);
		j += (b != nullptr);
	}
}

constexpr int32_t NO_STEP = std::numeric_limits<int32_t>::min();   // DESIGN: no step yet (a game's first is -1)

struct Step {
	int32_t step = NO_STEP;
	std::map<std::string, std::vector<Value>, std::less<>> recorded;
};

inline void CloseStep(ReplayResult& r, Step& s, const std::vector<Column>& columns, ReplaySystem& system, Outputs& out) {
	if (s.step == NO_STEP) return;
	out.Clear();
	system.EndStep(s.step, out);
	++r.steps;
	for (const Column& c : columns) {
		if (c.IsInput() || !c.Compared()) continue;
		std::vector<Value>& recorded = s.recorded[c.name];
		std::vector<Value>& computed = out.Rows()[c.name];
		CompareColumn(r, s.step, c.name, c.Ordered(), recorded, computed);
	}
	s.recorded.clear();
}

inline bool ReadValue(std::span<const uint8_t> bytes, Value& value) {
	ByteReader in(bytes);
	for (int32_t& id : value.key) id = in.Get<int32_t>();
	if (in.Failed() || in.Left() % sizeof(float) != 0) return false;
	value.v.resize(in.Left() / sizeof(float));
	for (float& f : value.v) f = in.Get<float>();
	return true;
}

} // namespace Detail

inline ReplayResult Replay(std::span<const uint8_t> trace, ReplaySystem& system) {
	ReplayResult r;
	Reader reader;
	if (!reader.Open(trace)) {
		r.error = reader.Error();
		return r;
	}
	system.Begin(reader.Head());
	Detail::Step step;
	Outputs out;
	RowView row;
	while (reader.Next(row)) {
		if (row.step != step.step) {
			if (row.step < step.step) {
				r.error = "steps go back";
				return r;
			}
			Detail::CloseStep(r, step, reader.Columns(), system, out);
			step.step = row.step;
		}
		const Column& c = reader.Columns()[static_cast<size_t>(row.column)];
		if (c.IsInput()) {
			ByteReader bytes(row.bytes);
			system.Input(c.name, bytes, row.step);
			++r.inputs;
			if (bytes.Failed()) r.error = "short input row in column " + c.name;
		} else if (Value value; Detail::ReadValue(row.bytes, value)) {
			step.recorded[c.name].push_back(std::move(value));
		} else {
			r.error = "bad output row in column " + c.name;
		}
		if (!r.error.empty()) return r;
	}
	Detail::CloseStep(r, step, reader.Columns(), system, out);
	if (!reader.Error().empty()) r.error = reader.Error();
	else if (!reader.Complete()) r.error = "the trace is cut short";
	return r;
}

// How many steps exercised each column, and each code path the Lua named (an output column "path:<name>").
class Coverage {
public:
	void Row(std::string_view column, int32_t step) {
		auto it = columns.find(column);
		if (it == columns.end()) it = columns.emplace(std::string(column), Count{}).first;
		++it->second.rows;
		if (it->second.lastStep != step) ++it->second.steps;
		it->second.lastStep = step;
	}
	// one line per column, in name order
	template<class F> void Lines(const std::string& system, F&& line) const {
		for (const auto& [name, c] : columns)
			line("[KT1TRACE] " + system + " coverage " + name + " steps=" + std::to_string(c.steps) + " rows=" + std::to_string(c.rows));
	}
	void Clear() { columns.clear(); }
	bool Empty() const { return columns.empty(); }

private:
	struct Count {
		uint64_t steps = 0, rows = 0;
		int32_t lastStep = Detail::NO_STEP;
	};
	std::map<std::string, Count, std::less<>> columns;
};

} // namespace KT1::Core::Trace

#endif
