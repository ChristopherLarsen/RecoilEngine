/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free: also built into kt1_core_tests (KT1/Test/README.md).
// KT1 Data: reading one JSON object against a config struct's keys (Schema.h). Engine-free.

#include "Schema.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string_view>

#include "Json.h"
#include "Number.h"

namespace KT1::Data {

struct Schema::Impl {
	const Node& object;
	std::string path;
	Report& report;
	std::set<std::string, std::less<>> known;   // keys a Field/Object/List call asked for
};

static const char* TypeName(const Node& n) {
	switch (n.kind) {
		case Node::Kind::List:   return "a list";
		case Node::Kind::Object: return "an object";
		case Node::Kind::Number: return IsWholeNumber(n.text) ? "a whole number" : "a number with a fraction";
		case Node::Kind::String: return "a string";
		case Node::Kind::Bool:   return "true or false";
		default:                 return "null";
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

static bool IsStringList(const Node& n) {
	if (n.kind != Node::Kind::List) return false;
	for (const Node& item : n.items)
		if (item.kind != Node::Kind::String) return false;
	return true;
}

Schema::Schema(const Node& object, std::string path, Report& report) : impl(new Impl{object, std::move(path), report, {}}) {}
Schema::~Schema() = default;

std::string Schema::Where(const std::string& key) const { return impl->path.empty() ? key : impl->path + "." + key; }

void Schema::WrongType(const std::string& where, const char* wanted, const Node& got) {
	impl->report.Error(where, std::string("expected ") + wanted + ", got " + TypeName(got));
}

const Node* Schema::Find(const char* key) {
	impl->known.insert(key);
	return impl->object.Find(key);
}

void Schema::Field(const char* key, bool& value) {
	const Node* n = Find(key);
	if (!n) return;
	if (n->kind != Node::Kind::Bool) return WrongType(Where(key), "true or false", *n);
	value = n->boolean;
}

void Schema::Field(const char* key, int& value, Range range) {
	const Node* n = Find(key);
	if (!n) return;
	if (n->kind != Node::Kind::Number) return WrongType(Where(key), "a whole number", *n);
	int v = 0;
	const NumberError err = ReadNumber(n->text, v);
	if (err == NumberError::NotWhole) return WrongType(Where(key), "a whole number", *n);
	if (err != NumberError::Ok) return impl->report.Error(Where(key), "too large for a whole number");
	if (double(v) < range.min || double(v) > range.max)
		return impl->report.Error(Where(key), std::to_string(v) + " is out of range: " + RangeText(range));
	value = v;
}

// The range is judged on the number as a double, before it is rounded to float.
bool Schema::Number(const std::string& where, const Node& node, float& value, const Range& range) {
	if (node.kind != Node::Kind::Number) {
		WrongType(where, "a number", node);
		return false;
	}
	float f{};
	double v{};
	if (ReadNumber(node.text, f) != NumberError::Ok || ReadNumber(node.text, v) != NumberError::Ok) {
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
	if (const Node* n = Find(key)) Number(Where(key), *n, value, range);
}

void Schema::Field(const char* key, std::string& value) {
	const Node* n = Find(key);
	if (!n) return;
	if (n->kind != Node::Kind::String) return WrongType(Where(key), "a string", *n);
	value = n->text;
}

// All or nothing: a bad entry leaves the whole list at its default, never half a colour.
void Schema::Floats(const char* key, float* values, std::size_t count, Range range) {
	const Node* n = Find(key);
	if (!n) return;
	if (n->kind != Node::Kind::List) return WrongType(Where(key), "a list of numbers", *n);
	if (n->items.size() != count)
		return impl->report.Error(Where(key), "expected " + std::to_string(count) + " numbers, got " + std::to_string(n->items.size()));
	std::vector<float> read(count);
	bool ok = true;
	for (std::size_t i = 0; i < count; ++i)
		ok = Number(Where(key) + "[" + std::to_string(i + 1) + "]", n->items[i], read[i], range) && ok;
	if (!ok) return;
	for (std::size_t i = 0; i < count; ++i) values[i] = read[i];
}

std::unique_ptr<Schema> Schema::OpenObject(const char* key) {
	const Node* n = Find(key);
	if (!n) return nullptr;
	if (n->kind != Node::Kind::Object) {
		WrongType(Where(key), "an object", *n);
		return nullptr;
	}
	return std::make_unique<Schema>(*n, Where(key), impl->report);
}

// True when the list is there and every entry is an object (a list with one bad entry keeps the default list).
bool Schema::OpenList(const char* key, std::size_t& count) {
	const Node* n = Find(key);
	if (!n) return false;
	if (n->kind != Node::Kind::List) {
		WrongType(Where(key), "a list", *n);
		return false;
	}
	count = n->items.size();
	bool ok = true;
	for (std::size_t i = 0; i < count; ++i) {
		if (n->items[i].kind == Node::Kind::Object) continue;
		WrongType(Where(key) + "[" + std::to_string(i + 1) + "]", "an object", n->items[i]);
		ok = false;
	}
	return ok;
}

// Item numbers in messages start at 1.
std::unique_ptr<Schema> Schema::OpenItem(const char* key, std::size_t index) {
	const Node* list = impl->object.Find(key);
	if (!list || list->kind != Node::Kind::List || index >= list->items.size()) return nullptr;
	const Node& item = list->items[index];
	if (item.kind != Node::Kind::Object) return nullptr;
	return std::make_unique<Schema>(item, Where(key) + "[" + std::to_string(index + 1) + "]", impl->report);
}

void Schema::CheckNote(const std::string& key, const Node& value) {
	const std::string where = Where(key);
	if (key == "_doc") {
		if (value.kind != Node::Kind::String) WrongType(where, "a string", value);
		return;
	}
	if (key == "_sources" && impl->path.empty()) {
		if (!IsStringList(value)) impl->report.Error(where, "expected a list of strings");
		return;
	}
	if (key != "_label") return impl->report.Unknown(where, "unknown note (notes are _label, _doc and, at the top, _sources)");
	if (value.kind != Node::Kind::Object) return WrongType(where, "an object of key: label", value);
	for (std::size_t i = 0; i < value.keys.size(); ++i) {
		const std::string at = where + "." + value.keys[i];
		if (value.items[i].kind != Node::Kind::String) WrongType(at, "a string", value.items[i]);
		if (!impl->object.Find(value.keys[i])) impl->report.Unknown(at, "labels a key this object does not have");
	}
}

void Schema::Finish() {
	std::set<std::string_view> seen;
	for (std::size_t i = 0; i < impl->object.keys.size(); ++i) {
		const std::string& key = impl->object.keys[i];
		if (!seen.insert(key).second) {
			impl->report.Error(Where(key), "key given twice");
			continue;
		}
		if (!key.empty() && key[0] == '_') CheckNote(key, impl->object.items[i]);
		else if (impl->known.find(key) == impl->known.end()) impl->report.Unknown(Where(key), "unknown key");
	}
}

} // namespace KT1::Data
