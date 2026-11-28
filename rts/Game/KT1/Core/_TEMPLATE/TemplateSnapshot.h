/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: the value a synced system publishes for the draw side (Host/Snapshot<T>). No engine include.
// A value type lives in a small header of its own, in Core/, so the publisher (Rules/) and the reader (Draw/, UI/) share
// it without including each other. Plain values only: no pointer, no id of a live object that outlives the step.

#ifndef KT1_TEMPLATE_SNAPSHOT_H
#define KT1_TEMPLATE_SNAPSHOT_H

namespace KT1::Core::Template {

struct Dot {
	float x = 0, y = 0, z = 0;   // elmos, the sim position at the step's end
	float charge = 0;            // the rule's state (TemplateRuleCore.h)
};

} // namespace KT1::Core::Template

#endif
