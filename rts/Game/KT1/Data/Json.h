/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: a whole JSON file as a tree whose numbers are still the file's text, for Number.h to read. Engine-free.

#ifndef KT1_DATA_JSON_H
#define KT1_DATA_JSON_H

#include <string>
#include <string_view>
#include <vector>

namespace KT1::Data {

struct Node {
	enum class Kind { Null, Bool, Number, String, List, Object };

	Kind kind = Kind::Null;
	bool boolean = false;
	std::string text;                // a string's value; a number as the file writes it
	std::vector<std::string> keys;   // Object: keys[i] names items[i], in file order, repeats kept
	std::vector<Node> items;         // List and Object

	const Node* Find(std::string_view key) const;   // the first with that key, or nullptr
};

// Reads `text`, which must be exactly one object. The whole text is checked: false, with `error` set, on anything that
// is not valid JSON.
bool ReadObject(std::string_view text, Node& root, std::string& error);

} // namespace KT1::Data

#endif
