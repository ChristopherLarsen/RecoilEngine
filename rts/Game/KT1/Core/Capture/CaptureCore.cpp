/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Capture, the engine-free half: see CaptureCore.h.

#include "CaptureCore.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace KT1::CaptureCore {
	namespace {
		bool GoodName(const std::string& s) {
			return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
		}
	}

	const char* SideName(Side side) {
		switch (side) {
			case Side::Off: return "off";
			case Side::On:  return "on";
			default:        return "one";
		}
	}

	std::string Validate(const Script& script) {
		if (script.shots.empty()) return "no shot";
		if (script.settle < 2) return "settle_draws must be 2 or more";   // DESIGN: a side switch shows one drawn frame later
		if (!(script.timeOffset >= 0 && script.timeOffset <= 1)) return "time_offset must be from 0 to 1";
		int last = 1;
		for (size_t i = 0; i < script.shots.size(); ++i) {
			const Shot& s = script.shots[i];
			if (!GoodName(s.name)) return "shot name '" + s.name + "': letters, digits, - and _ only";
			if (s.frame < last) return "shot " + s.name + ": frames start at 1 and never go back";
			last = s.frame;
			for (size_t j = 0; j < i; ++j)
				if (script.shots[j].name == s.name) return "two shots named " + s.name;
		}
		return "";
	}

	std::string PicturePath(const Shot& shot, Side side) {
		return std::string(PICTURE_DIR) + "/" + shot.name + "_" + SideName(side) + ".png";
	}

	Sequence::Sequence(Script s, bool abRun): script(std::move(s)), ab(abRun), side(abRun ? Side::Off : Side::One) {}

	int Sequence::HoldFrame() const { return Done() ? NO_HOLD : script.shots[next].frame; }

	Step Sequence::Draw(int simFrame) {
		Step step;
		grabbing = false;
		if (Done() || simFrame < script.shots[next].frame) return step;
		step.shot = static_cast<int>(next);
		step.side = side;
		if (simFrame > script.shots[next].frame) {   // the hold was late: a picture of another frame would not be this shot
			step.missed = true;
			++missed;
			Advance();
			return step;
		}
		if (++drawn > script.settle) {
			step.grab = true;
			grabbing = true;
		}
		return step;
	}

	void Sequence::Grabbed() {
		if (!grabbing) return;
		grabbing = false;
		if (ab && side == Side::Off) {
			side = Side::On;
			drawn = 0;
			return;
		}
		++taken;
		Advance();
	}

	void Sequence::Advance() {
		++next;
		side = ab ? Side::Off : Side::One;
		drawn = 0;
	}
}
