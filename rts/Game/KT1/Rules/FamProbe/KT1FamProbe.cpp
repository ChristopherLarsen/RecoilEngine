/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the dummy family "famprobe" (LUA-FREE H01): a synced and a draw system that do nothing, so a run can prove that
// kt1_fam_famprobe and each system's own option set their modes (the load lines print family=famprobe and the mode).
// Switches: mod options kt1_famproberule, kt1_famprobedraw, kt1_fam_famprobe (all unset by default: both off).

#include "Game/KT1/KT1Register.h"

namespace KT1 {
namespace {

class FamProbeRule : public Rule {
public:
	FamProbeRule() : Rule({"FamProbeRule", 1, true, "nothing (family switch test system)", false, true}) {}
};

class FamProbeDraw : public Rule {
public:
	FamProbeDraw() : Rule({"FamProbeDraw", 1, false, "nothing (family switch test system)", false}) {}
};

} // namespace

KT1_REGISTER_SYSTEM(FamProbeRule, 9200, FamProbeRule)
KT1_REGISTER_SYSTEM(FamProbeDraw, 9300, FamProbeDraw)

} // namespace KT1
