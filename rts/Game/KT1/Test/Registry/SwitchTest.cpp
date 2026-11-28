/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 registry core tests (LUA-FREE H01): a switch from the family option, the system option, both or neither, over a
// dummy family of two systems, and when a family check is refused.

#include "Game/KT1/Core/Registry/Switch.h"
#include "Game/KT1/Test/KT1Test.h"

namespace {

using namespace KT1::Core::Registry;

const SystemSwitch SYNCED{"FamProbeRule", "famprobe", true, true, false};
const SystemSwitch DRAW{"FamProbeDraw", "famprobe", false, false, false};

bool Is(const Resolved& r, SwitchMode mode, bool fromFamily) { return r.mode == mode && r.fromFamily == fromFamily; }

} // namespace

KT1_CASE("REG-FAM-NEITHER", "with no option a system takes its own default") {
	KT1_CHECK(Is(Resolve(SYNCED, "", ""), SwitchMode::Off, false));
	SystemSwitch flipped = DRAW;
	flipped.defaultOn = true;
	KT1_CHECK(Is(Resolve(flipped, "", ""), SwitchMode::On, false));
}

KT1_CASE("REG-FAM-FAMILY", "the family option sets every system whose own option is not given") {
	KT1_CHECK(Is(Resolve(SYNCED, "", "1"), SwitchMode::On, true));
	KT1_CHECK(Is(Resolve(DRAW, "", "TRUE"), SwitchMode::On, true));
	SystemSwitch flipped = SYNCED;
	flipped.defaultOn = true;
	KT1_CHECK(Is(Resolve(flipped, "", "0"), SwitchMode::Off, true));
}

KT1_CASE("REG-FAM-SYSTEM", "a system option alone decides its system") {
	KT1_CHECK(Is(Resolve(SYNCED, "1", ""), SwitchMode::On, false));
	KT1_CHECK(Is(Resolve(DRAW, "check", ""), SwitchMode::Check, false));
}

KT1_CASE("REG-FAM-BOTH", "the system option wins over the family option") {
	KT1_CHECK(Is(Resolve(SYNCED, "0", "1"), SwitchMode::Off, false));
	KT1_CHECK(Is(Resolve(DRAW, "1", "0"), SwitchMode::On, false));
	KT1_CHECK(Is(Resolve(SYNCED, "1", "check"), SwitchMode::On, false));
}

KT1_CASE("REG-FAM-CHECK", "family check reaches synced systems and leaves draw systems off") {
	KT1_CHECK(Is(Resolve(SYNCED, "", "check"), SwitchMode::Check, true));
	KT1_CHECK(Is(Resolve(DRAW, "", "check"), SwitchMode::Off, true));
	KT1_CHECK(Is(Resolve(DRAW, "1", "check"), SwitchMode::On, false));
}

KT1_CASE("REG-FAM-NOFAMILY", "a system in no family ignores a family value") {
	const SystemSwitch loose{"Loose", "", true, true, true};
	KT1_CHECK(Is(Resolve(loose, "", "0"), SwitchMode::On, false));
}

KT1_CASE("REG-FAM-BLOCK", "family check is refused while a synced system of the family cannot check") {
	std::vector<SystemSwitch> systems{SYNCED, DRAW};
	KT1_CHECK(FamilyCheckBlocker(systems, "famprobe", "check") == nullptr);   // the draw system need not declare it
	systems.push_back({"Third", "famprobe", true, false, false});
	const SystemSwitch* blocker = FamilyCheckBlocker(systems, "famprobe", "Check");
	KT1_CHECK(blocker != nullptr && blocker->name == "Third");
	KT1_CHECK(FamilyCheckBlocker(systems, "famprobe", "1") == nullptr);
	KT1_CHECK(FamilyCheckBlocker(systems, "other", "check") == nullptr);
	KT1_CHECK_EQ(FamilyOption("famprobe"), std::string("kt1_fam_famprobe"));
}
