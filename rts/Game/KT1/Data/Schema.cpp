/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free: also built into kt1_core_tests (KT1/Test/README.md).
// KT1 Data: reading one JSON object against a config struct's keys (Schema.h). Engine-free.

#include "Schema.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string_view>

#include "simdjson.h"

namespace KT1::Data {

namespace dom = simdjson::dom;

struct Schema::Impl {
	dom::object object;
	std::string path;
	Report& report;
	std::set<std::string, std::less<>> known;   // keys a Field/Object/List call asked for
};

static const char* TypeName(const dom::element& e) {
	switch (e.type()) {
		case dom::element_type::ARRAY:  return "a list";
		case dom::element_type::OBJECT: return "an object";
		case dom::element_type::INT64:
		case dom::element_type::UINT64: return "a whole number";
		case dom::element_type::DOUBLE: return "a number with a fraction";
		case dom::element_type::STRING: return "a string";
		case dom::element_type::BOOL:   return "true or false";
		default:                        return "null";
	}
}

static std::string Short(double v) {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.9g", v);
	return buf;
}

static std::string RangeText(const Range& r) {
	if (std::isinf(r.max)) return "at least " + Short(r.min);
	if (std::isinf(r.min)) return "at most " + Short(r.max);
	return "from " + Short(r.min) + " to " + Short(r.max);
}

static bool IsStringList(const dom::element& e) {
	dom::array a;
	if (e.get(a) != simdjson::SUCCESS) return false;
	for (const dom::element item : a)
		if (!item.is_string()) return false;
	return true;
}

Schema::Schema(const dom::object& object, std::string path, Report& report) : impl(new Impl{object, std::move(path), report, {}}) {}
Schema::~Schema() = default;

std::string Schema::Where(const std::string& key) const { return impl->path.empty() ? key : impl->path + "." + key; }

void Schema::WrongType(const std::string& where, const char* wanted, const dom::element& got) {
	impl->report.Error(where, std::string("expected ") + wanted + ", got " + TypeName(got));
}

bool Schema::Find(const char* key, dom::element& out) {
	impl->known.insert(key);
	return impl->object.at_key(key).get(out) == simdjson::SUCCESS;
}

void Schema::Field(const char* key, bool& value) {
	dom::element e;
	if (!Find(key, e)) return;
	bool v = false;
	if (e.get(v) != simdjson::SUCCESS) return WrongType(Where(key), "true or false", e);
	value = v;
}

void Schema::Field(const char* key, int& value, Range range) {
	dom::element e;
	if (!Find(key, e)) return;
	if (!e.is<int64_t>() && !e.is<uint64_t>()) return WrongType(Where(key), "a whole number", e);
	int64_t v = 0;
	if (e.get(v) != simdjson::SUCCESS || v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max())
		return impl->report.Error(Where(key), "too large for a whole number");
	if (double(v) < range.min || double(v) > range.max)
		return impl->report.Error(Where(key), std::to_string(v) + " is out of range: " + RangeText(range));
	value = int(v);
}

// A float is the nearest double of the file's text, rounded to float: the same two steps Lua's reader takes (strtod into
// its float number type), so a value moved from a .lua config keeps its bits.
bool Schema::Number(const std::string& where, const dom::element& e, float& value, const Range& range) {
	double v = 0;
	if (e.get(v) != simdjson::SUCCESS) {
		WrongType(where, "a number", e);
		return false;
	}
	const float f = static_cast<float>(v);
	if (!std::isfinite(f)) {
		impl->report.Error(where, "too large for a number");
		return false;
	}
	if (v < range.min || v > range.max) {
		impl->report.Error(where, Short(v) + " is out of range: " + RangeText(range));
		return false;
	}
	value = f;
	return true;
}

void Schema::Field(const char* key, float& value, Range range) {
	dom::element e;
	if (Find(key, e)) Number(Where(key), e, value, range);
}

void Schema::Field(const char* key, std::string& value) {
	dom::element e;
	if (!Find(key, e)) return;
	std::string_view v;
	if (e.get(v) != simdjson::SUCCESS) return WrongType(Where(key), "a string", e);
	value.assign(v);
}

// All or nothing: a bad entry leaves the whole list at its default, never half a colour.
void Schema::Floats(const char* key, float* values, std::size_t count, Range range) {
	dom::element e;
	if (!Find(key, e)) return;
	dom::array a;
	if (e.get(a) != simdjson::SUCCESS) return WrongType(Where(key), "a list of numbers", e);
	std::size_t given = 0;
	for ([[maybe_unused]] const dom::element item : a) ++given;
	if (given != count)
		return impl->report.Error(Where(key), "expected " + std::to_string(count) + " numbers, got " + std::to_string(given));
	std::vector<float> read(count);
	std::size_t i = 0;
	bool ok = true;
	for (const dom::element item : a) {
		ok = Number(Where(key) + "[" + std::to_string(i + 1) + "]", item, read[i], range) && ok;
		++i;
	}
	if (!ok) return;
	for (i = 0; i < count; ++i) values[i] = read[i];
}

std::unique_ptr<Schema> Schema::OpenObject(const char* key) {
	dom::element e;
	if (!Find(key, e)) return nullptr;
	dom::object o;
	if (e.get(o) != simdjson::SUCCESS) {
		WrongType(Where(key), "an object", e);
		return nullptr;
	}
	return std::make_unique<Schema>(o, Where(key), impl->report);
}

// True when the list is there and every entry is an object (a list with one bad entry keeps the default list).
bool Schema::OpenList(const char* key, std::size_t& count) {
	dom::element e;
	if (!Find(key, e)) return false;
	dom::array a;
	if (e.get(a) != simdjson::SUCCESS) {
		WrongType(Where(key), "a list", e);
		return false;
	}
	count = 0;
	bool ok = true;
	for (const dom::element item : a) {
		++count;
		if (item.is_object()) continue;
		WrongType(Where(key) + "[" + std::to_string(count) + "]", "an object", item);
		ok = false;
	}
	return ok;
}

// Item numbers in messages start at 1.
std::unique_ptr<Schema> Schema::OpenItem(const char* key, std::size_t index) {
	dom::array a;
	dom::element e;
	dom::object o;
	if (impl->object.at_key(key).get(a) != simdjson::SUCCESS) return nullptr;
	if (a.at(index).get(e) != simdjson::SUCCESS || e.get(o) != simdjson::SUCCESS) return nullptr;
	return std::make_unique<Schema>(o, Where(key) + "[" + std::to_string(index + 1) + "]", impl->report);
}

void Schema::CheckNote(const std::string& key, const dom::element& value) {
	const std::string where = Where(key);
	if (key == "_doc") {
		if (!value.is_string()) WrongType(where, "a string", value);
		return;
	}
	if (key == "_sources" && impl->path.empty()) {
		if (!IsStringList(value)) impl->report.Error(where, "expected a list of strings");
		return;
	}
	if (key != "_label") return impl->report.Unknown(where, "unknown note (notes are _label, _doc and, at the top, _sources)");
	dom::object labels;
	if (value.get(labels) != simdjson::SUCCESS) return WrongType(where, "an object of key: label", value);
	for (const dom::key_value_pair label : labels) {
		const std::string at = where + "." + std::string(label.key);
		if (!label.value.is_string()) WrongType(at, "a string", label.value);
		dom::element target;
		if (impl->object.at_key(label.key).get(target) != simdjson::SUCCESS)
			impl->report.Unknown(at, "labels a key this object does not have");
	}
}

void Schema::Finish() {
	std::set<std::string_view> seen;
	for (const dom::key_value_pair entry : impl->object) {
		const std::string key(entry.key);
		if (!seen.insert(entry.key).second) {
			impl->report.Error(Where(key), "key given twice");
			continue;
		}
		if (!key.empty() && key[0] == '_') CheckNote(key, entry.value);
		else if (impl->known.find(key) == impl->known.end()) impl->report.Unknown(Where(key), "unknown key");
	}
}

} // namespace KT1::Data
