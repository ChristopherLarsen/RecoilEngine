/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Capture, the engine-free half: see CaptureCore.h.

#include "CaptureCore.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string_view>

#include "simdjson.h"

namespace KT1::CaptureCore {
	namespace {
		using simdjson::dom::element;
		using simdjson::dom::object;

		bool ReadInt(element v, int& out) {
			int64_t n = 0;
			if (v.get(n) != simdjson::SUCCESS || n < INT_MIN || n > INT_MAX) return false;
			out = static_cast<int>(n);
			return true;
		}

		bool ReadFloat(element v, float& out) {
			double d = 0;
			if (v.get(d) != simdjson::SUCCESS) return false;
			out = static_cast<float>(d);
			return true;
		}

		// the two entries of a [a, b] list
		bool ReadPair(element v, element& a, element& b) {
			simdjson::dom::array list;
			return v.get(list) == simdjson::SUCCESS && list.size() == 2 && list.at(0).get(a) == simdjson::SUCCESS &&
				list.at(1).get(b) == simdjson::SUCCESS;
		}

		std::string ReadCamera(element v, Pose& pose) {
			object camera;
			if (v.get(camera) != simdjson::SUCCESS) return "shot camera: not an object";
			for (const auto field : camera) {
				float value = 0;
				if (!ReadFloat(field.value, value)) return "shot camera " + std::string(field.key) + ": not a number";
				pose.state.emplace_back(std::string(field.key), value);
			}
			return "";
		}

		std::string ReadShot(element v, Shot& shot) {
			object fields;
			if (v.get(fields) != simdjson::SUCCESS) return "shots: an entry is not an object";
			for (const auto field : fields) {
				const std::string_view key = field.key;
				if (key == "name") {
					std::string_view name;
					if (field.value.get(name) != simdjson::SUCCESS) return "shot name: not a string";
					shot.name = name;
				} else if (key == "frame") {
					if (!ReadInt(field.value, shot.frame)) return "shot frame: not a whole number";
				} else if (key == "at") {
					element x, z;
					if (!ReadPair(field.value, x, z) || !ReadFloat(x, shot.pose.atX) || !ReadFloat(z, shot.pose.atZ))
						return "shot at: two numbers (fractions of the map's width and depth)";
					shot.pose.hasAt = true;
				} else if (key == "camera") {
					const std::string why = ReadCamera(field.value, shot.pose);
					if (!why.empty()) return why;
				} else {
					return "shot: unknown key " + std::string(key);
				}
			}
			return "";
		}

		std::string ReadField(std::string_view key, element v, Script& script) {
			if (key == "_sources" || key == "_label") return "";
			if (key == "settle_draws") return ReadInt(v, script.settle) ? "" : "settle_draws: not a whole number";
			if (key == "time_offset") return ReadFloat(v, script.timeOffset) ? "" : "time_offset: not a number";
			if (key == "seed") return ReadInt(v, script.seed) ? "" : "seed: not a whole number";
			if (key == "hide_interface") return v.get(script.hideInterface) == simdjson::SUCCESS ? "" : "hide_interface: not true or false";
			if (key == "quit") return v.get(script.quit) == simdjson::SUCCESS ? "" : "quit: not true or false";
			if (key == "plant_pixel") {
				element x, y;
				const bool ok = ReadPair(v, x, y) && ReadInt(x, script.plantX) && ReadInt(y, script.plantY);
				return ok ? "" : "plant_pixel: two whole numbers";
			}
			if (key == "shots") {
				simdjson::dom::array list;
				if (v.get(list) != simdjson::SUCCESS) return "shots: not a list";
				for (const element item : list) {
					Shot shot;
					const std::string why = ReadShot(item, shot);
					if (!why.empty()) return why;
					script.shots.push_back(shot);
				}
				return "";
			}
			return "unknown key " + std::string(key);
		}

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

	std::string ParseScript(const std::string& text, Script& script) {
		simdjson::dom::parser parser;
		object root;
		const simdjson::padded_string padded(text);
		if (parser.parse(padded).get(root) != simdjson::SUCCESS) return "not a JSON object";
		for (const auto field : root) {
			const std::string why = ReadField(field.key, field.value, script);
			if (!why.empty()) return why;
		}
		return Validate(script);
	}

	std::string Validate(const Script& script) {
		if (script.shots.empty()) return "no shot";
		if (script.settle < 2) return "settle_draws must be 2 or more";
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
