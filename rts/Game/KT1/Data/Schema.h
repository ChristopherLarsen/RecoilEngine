/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: how a config struct names its keys. Engine-free (game-build docs/engine/rules/data.md).
//
// A config struct holds its defaults as member initialisers and describes its keys once:
//     struct CensusConfig {
//         int period = 15;                                        // DESIGN: ...
//         void Describe(KT1::Data::Schema& s) { s.Field("period", period, KT1::Data::AtLeast(1)); }
//     };
// A Schema stands for one JSON object being read. Each Field call reads its key if the file has it and leaves the default
// if not. A key no call asked for is unknown. Keys beginning "_" are notes, never values:
//     "_label"    an object: key name -> the source label of that key's value
//     "_doc"      a string
//     "_sources"  a list of strings: where the file's values come from (the top object only)

#ifndef KT1_DATA_SCHEMA_H
#define KT1_DATA_SCHEMA_H

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "Report.h"

namespace KT1::Data {

struct Node;   // Json.h

// The values a number may take, both ends included.
struct Range {
	double min = -std::numeric_limits<double>::infinity();
	double max = std::numeric_limits<double>::infinity();
};
inline Range AtLeast(double min) { return Range{min, std::numeric_limits<double>::infinity()}; }
inline Range Between(double min, double max) { return Range{min, max}; }

class Schema {
public:
	// Reads `object`, found at `path` ("" for the top object). Used by Config.cpp and by Schema itself.
	// `object` must outlive the Schema.
	Schema(const Node& object, std::string path, Report& report);
	~Schema();
	Schema(const Schema&) = delete;
	Schema& operator=(const Schema&) = delete;

	void Field(const char* key, bool& value);
	void Field(const char* key, int& value, Range range = {});
	void Field(const char* key, float& value, Range range = {});
	void Field(const char* key, std::string& value);
	// A list of exactly N numbers (a colour, a ramp end).
	template <std::size_t N> void Field(const char* key, float (&values)[N], Range range = {}) { Floats(key, values, N, range); }
	template <std::size_t N> void Field(const char* key, std::array<float, N>& values, Range range = {}) { Floats(key, values.data(), N, range); }

	// A nested object: describe(Schema&) names its keys.
	template <class Describe> void Object(const char* key, Describe describe) {
		const std::unique_ptr<Schema> child = OpenObject(key);
		if (!child) return;
		describe(*child);
		child->Finish();
	}

	// A list of objects. When the file has the key, the list is replaced (not merged): each item starts as T{} and
	// describe(Schema&, T&) reads it.
	template <class T, class Describe> void List(const char* key, std::vector<T>& items, Describe describe) {
		std::size_t count = 0;
		if (!OpenList(key, count)) return;
		std::vector<T> read(count);
		for (std::size_t i = 0; i < count; ++i) {
			const std::unique_ptr<Schema> child = OpenItem(key, i);
			if (!child) continue;
			describe(*child, read[i]);
			child->Finish();
		}
		items.swap(read);
	}

	// After the last Field call: reports unknown and repeated keys and checks the notes. Config.cpp calls it on the top object.
	void Finish();

private:
	struct Impl;

	const Node* Find(const char* key);                          // marks the key known
	void Floats(const char* key, float* values, std::size_t count, Range range);
	bool Number(const std::string& where, const Node& node, float& value, const Range& range);
	void WrongType(const std::string& where, const char* wanted, const Node& got);
	std::string Where(const std::string& key) const;
	std::unique_ptr<Schema> OpenObject(const char* key);             // nullptr: absent, or not an object (reported)
	bool OpenList(const char* key, std::size_t& count);
	std::unique_ptr<Schema> OpenItem(const char* key, std::size_t index);
	void CheckNote(const std::string& key, const Node& value);

	std::unique_ptr<Impl> impl;
};

} // namespace KT1::Data

#endif
