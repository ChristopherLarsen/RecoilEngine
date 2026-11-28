/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 registry core: what a system's switch is, from its own mod option and its family's (LUA-FREE H01). No engine include.
//
//   kt1_<system> given          that value (always wins: the way to bisect inside a family)
//   else kt1_fam_<family> given  that value; "check" reaches synced systems only: a draw, interface or audio system of
//                                the family stays off under it
//   else                         the system's own default (RuleInfo.defaultOn)
//
// "Given" means present and not empty. "1" / "true": on; "check": check; anything else: off (case ignored).
// kt1_fam_<family>=check is refused (the engine stops at start) unless every synced system of the family declares
// checkSafe, whatever its own option says: check never reaches a rule that has not declared it.

#ifndef KT1_CORE_REGISTRY_SWITCH_H
#define KT1_CORE_REGISTRY_SWITCH_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace KT1::Core::Registry {

enum class SwitchMode : uint8_t { Off, On, Check };

inline const char* SwitchName(SwitchMode m) { return m == SwitchMode::On ? "on" : (m == SwitchMode::Check ? "check" : "off"); }

// the family option's key, "kt1_fam_orders"
inline std::string FamilyOption(std::string_view family) { return "kt1_fam_" + std::string(family); }

inline SwitchMode ParseSwitch(std::string_view value) {
	std::string v(value);
	for (char& c : v)
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	if (v == "1" || v == "true") return SwitchMode::On;
	return v == "check" ? SwitchMode::Check : SwitchMode::Off;
}

struct SystemSwitch {
	std::string name;
	std::string family;   // empty: in no family
	bool synced = false;
	bool checkSafe = false;
	bool defaultOn = false;
};

struct Resolved {
	SwitchMode mode = SwitchMode::Off;
	bool fromFamily = false;   // the family option decided it
};

// own, family: the option's value, empty when it is not given
inline Resolved Resolve(const SystemSwitch& s, std::string_view own, std::string_view family) {
	if (!own.empty()) return {ParseSwitch(own), false};
	if (!s.family.empty() && !family.empty()) {
		const SwitchMode m = ParseSwitch(family);
		return {(m == SwitchMode::Check && !s.synced) ? SwitchMode::Off : m, true};
	}
	return {s.defaultOn ? SwitchMode::On : SwitchMode::Off, false};
}

// With the family option "check": the first synced system of `family` that does not declare checkSafe (the engine must
// stop); nullptr when the family may be checked, or the option is not "check".
inline const SystemSwitch* FamilyCheckBlocker(const std::vector<SystemSwitch>& systems, std::string_view family, std::string_view value) {
	if (value.empty() || ParseSwitch(value) != SwitchMode::Check) return nullptr;
	for (const SystemSwitch& s : systems)
		if (s.family == family && s.synced && !s.checkSafe) return &s;
	return nullptr;
}

} // namespace KT1::Core::Registry

#endif
