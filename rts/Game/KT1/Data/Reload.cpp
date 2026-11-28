/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: hot reload decisions (Reload.h). Engine-free.

#include "Reload.h"

#include <cctype>

namespace KT1::Data {

bool PollClock::Due(double nowSeconds) {
	if (started && nowSeconds < next) return false;
	started = true;
	next = nowSeconds + POLL_SECONDS;
	return true;
}

void Watch::Seen(const std::string& system, std::string_view text) { texts[system].assign(text); }

bool Watch::Changed(const std::string& system, std::string_view text) {
	const auto it = texts.find(system);
	if (it == texts.end() || it->second == text) return false;
	it->second.assign(text);
	return true;
}

Verdict Judge(bool known, bool enabled, bool synced) {
	if (!known) return Verdict::Unknown;
	if (!enabled) return Verdict::Off;
	return synced ? Verdict::NextStart : Verdict::Live;
}

// Takes the next word off the front of `rest`.
static std::string_view Word(std::string_view& rest) {
	const auto space = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
	size_t from = 0;
	while (from < rest.size() && space(rest[from])) ++from;
	size_t to = from;
	while (to < rest.size() && !space(rest[to])) ++to;
	const std::string_view word = rest.substr(from, to - from);
	rest.remove_prefix(to);
	return word;
}

std::string ReloadTarget(std::string_view args) {
	if (Word(args) != "reload") return "";
	std::string system(Word(args));
	if (!Word(args).empty()) return "";
	for (char& c : system) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return system;
}

} // namespace KT1::Data
