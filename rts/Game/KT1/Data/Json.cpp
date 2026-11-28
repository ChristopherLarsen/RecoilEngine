/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free: also built into kt1_core_tests (KT1/Test/README.md).
// KT1 Data: walking a file with simdjson's on-demand reader (Json.h). It checks only what is asked of it, so every value
// is asked for. Numbers are taken as raw text; simdjson never converts one.

#include "Json.h"

#include "Number.h"
#include "simdjson.h"

namespace KT1::Data {

namespace od = simdjson::ondemand;

const Node* Node::Find(std::string_view key) const {
	for (std::size_t i = 0; i < keys.size(); ++i)
		if (keys[i] == key) return &items[i];
	return nullptr;
}

static constexpr int MAX_DEPTH = 64;   // DESIGN: config files are a few levels deep

static bool Fail(std::string& error, const char* what) {
	error = std::string("not valid JSON: ") + what;
	return false;
}
static bool Fail(std::string& error, simdjson::error_code code) { return Fail(error, simdjson::error_message(code)); }

static bool ReadValue(od::value value, Node& out, int depth, std::string& error);

static bool ReadMembers(od::object object, Node& out, int depth, std::string& error) {
	out.kind = Node::Kind::Object;
	for (auto entry : object) {
		od::field field;
		if (const auto err = std::move(entry).get(field)) return Fail(error, err);
		std::string_view key;
		if (const auto err = field.unescaped_key().get(key)) return Fail(error, err);
		out.keys.emplace_back(key);
		out.items.emplace_back();
		if (!ReadValue(field.value(), out.items.back(), depth + 1, error)) return false;
	}
	return true;
}

static bool ReadValue(od::value value, Node& out, int depth, std::string& error) {
	if (depth > MAX_DEPTH) return Fail(error, "nested too deeply");
	od::json_type type;
	if (const auto err = value.type().get(type)) return Fail(error, err);
	switch (type) {
		case od::json_type::object: {
			od::object object;
			if (const auto err = value.get_object().get(object)) return Fail(error, err);
			return ReadMembers(object, out, depth, error);
		}
		case od::json_type::array: {
			out.kind = Node::Kind::List;
			od::array array;
			if (const auto err = value.get_array().get(array)) return Fail(error, err);
			for (auto entry : array) {
				od::value item;
				if (const auto err = std::move(entry).get(item)) return Fail(error, err);
				out.items.emplace_back();
				if (!ReadValue(item, out.items.back(), depth + 1, error)) return false;
			}
			return true;
		}
		case od::json_type::string: {
			out.kind = Node::Kind::String;
			std::string_view s;
			if (const auto err = value.get_string().get(s)) return Fail(error, err);
			out.text.assign(s);
			return true;
		}
		case od::json_type::boolean: {
			out.kind = Node::Kind::Bool;
			if (const auto err = value.get_bool().get(out.boolean)) return Fail(error, err);
			return true;
		}
		case od::json_type::null: {
			bool null = false;
			if (const auto err = value.is_null().get(null)) return Fail(error, err);
			return null || Fail(error, "a value that is not null, true or false");
		}
		case od::json_type::number: {
			out.kind = Node::Kind::Number;
			std::string_view token = value.raw_json_token();   // runs up to the next mark, spaces included
			token = token.substr(0, token.find_last_not_of(" \t\r\n") + 1);
			double checked{};
			if (ReadNumber(token, checked) == NumberError::Syntax) return Fail(error, "a badly written number");
			out.text.assign(token);
			return true;
		}
		default:
			return Fail(error, "a value was expected");
	}
}

bool ReadObject(std::string_view text, Node& root, std::string& error) {
	od::parser parser;
	const simdjson::padded_string padded(text.data(), text.size());
	od::document doc;
	if (const auto err = parser.iterate(padded).get(doc)) return Fail(error, err);
	od::json_type type;
	if (const auto err = doc.type().get(type)) return Fail(error, err);
	if (type != od::json_type::object) {
		error = "the file must hold one object: { ... }";
		return false;
	}
	od::object object;
	if (const auto err = doc.get_object().get(object)) return Fail(error, err);
	root = Node{};
	if (!ReadMembers(object, root, 0, error)) return false;
	return doc.at_end() || Fail(error, "text after the object");
}

} // namespace KT1::Data
