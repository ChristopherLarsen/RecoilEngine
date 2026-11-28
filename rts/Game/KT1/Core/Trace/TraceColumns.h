/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Trace core: a column row's compared values to a replay, under the keys the check helper uses, so the row a family
// declares once (<Name>Columns.h, game-build tools/engine/kt1_scaffold.py) serves the check, the recorder and the
// replay. In a replay's EndStep:
//
//   Columns::OrdersRow::Visit(row, ReplayColumns{out, unitID});
//
// The keys are those of Host/CheckColumns.h, and of the Lua twin's Spring.KT1.Check.Answer(system, column, id, index,
// count, value):
//   a single value      index 0, count 0
//   an array            index 0 to N-1, count 0
//   a list              (id, 0, 1) with its length, then (id, index, 0) per element
// So an output column is a list when it has a row of count 1, and then is compared as the others: by key, with the rows of
// one key in the order given. No engine include.

#ifndef KT1_TRACE_COLUMNS_H
#define KT1_TRACE_COLUMNS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "TraceReplay.h"

namespace KT1::Core::Trace {

struct ReplayColumns {
	Outputs& out;
	int32_t id;

	void operator()(const char* column, bool v, bool compare) const { One(column, v ? 1.0f : 0.0f, compare); }
	void operator()(const char* column, uint8_t v, bool compare) const { One(column, static_cast<float>(v), compare); }
	void operator()(const char* column, uint16_t v, bool compare) const { One(column, static_cast<float>(v), compare); }
	void operator()(const char* column, int32_t v, bool compare) const { One(column, static_cast<float>(v), compare); }
	void operator()(const char* column, float v, bool compare) const { One(column, v, compare); }

	template<size_t N> void operator()(const char* column, const std::array<float, N>& v, bool compare) const {
		if (!compare) return;
		for (size_t i = 0; i < N; ++i) Give(column, static_cast<int32_t>(i), 0, v[i]);
	}
	template<class T> void operator()(const char* column, const std::vector<T>& v, bool compare) const {
		if (!compare) return;
		Give(column, 0, 1, static_cast<float>(v.size()));
		for (size_t i = 0; i < v.size(); ++i) Give(column, static_cast<int32_t>(i), 0, static_cast<float>(v[i]));
	}

private:
	void One(const char* column, float v, bool compare) const {
		if (compare) Give(column, 0, 0, v);
	}
	void Give(const char* column, int32_t index, int32_t count, float value) const {
		out.Give(column, {id, index, count}, {{value}});
	}
};

} // namespace KT1::Core::Trace

#endif
