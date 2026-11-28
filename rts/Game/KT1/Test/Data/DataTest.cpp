/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data, engine-free: the JSON config loader (Config, Schema) and the hot reload decisions (Reload).
// The loader holds no sim time, so no case here depends on the step rate. The engine builds with
// -fsingle-precision-constant: a bare literal such as 0.1 is a float.

#include "../KT1Test.h"
#include "Game/KT1/Data/Config.h"
#include "Game/KT1/Data/Reload.h"

#include <cstdlib>
#include <string>
#include <vector>

using namespace KT1::Data;

namespace {
	struct Cap { float scale = 1; int ribbons = 200; };

	struct Sample {
		bool on = false;
		int count = 7;
		float gain = 0.5f;
		std::string name = "plain";
		float colour[3] = {0.1f, 0.2f, 0.3f};
		int extras = 40;
		std::vector<Cap> caps = {{0.5f, 60}, {1, 200}};

		void Describe(Schema& s) {
			s.Field("on", on);
			s.Field("count", count, AtLeast(1));
			s.Field("gain", gain, Between(0, 4));
			s.Field("name", name);
			s.Field("colour", colour);
			s.Object("heavy", [this](Schema& h) { h.Field("extras", extras); });
			s.List("caps", caps, [](Schema& c, Cap& cap) {
				c.Field("scale", cap.scale);
				c.Field("ribbons", cap.ribbons);
			});
		}
	};

	struct Empty { void Describe(Schema&) {} };

	// True when the report has an issue at `where` of that severity whose text holds `part`.
	bool Says(const Report& r, const char* where, bool error, const char* part) {
		for (const Issue& i : r.issues)
			if (i.where == where && i.error == error && i.text.find(part) != std::string::npos) return true;
		return false;
	}

	bool IsDefault(const Sample& c) {
		return !c.on && c.count == 7 && c.gain == 0.5f && c.name == "plain" && c.colour[0] == 0.1f && c.colour[2] == 0.3f &&
		       c.extras == 40 && c.caps.size() == 2 && c.caps[0].ribbons == 60;
	}

	const char* const UNKNOWN = R"({"count": 3, "cuont": 4, "heavy": {"extra": 1}, "caps": [{"point": 2}]})";
	const char* const WRONG = R"({"on": 1, "count": 2.5, "gain": "high", "name": 4, "colour": [1, 2], "heavy": 3,
		"caps": [{"scale": 1}, 5]})";
}

KT1_CASE("DATA-DEFAULTS", "an absent key keeps its default, on a first load and on a reload") {
	Sample c;
	Report r = ParseInto("{}", Strictness::Dev, c);
	KT1_CHECK(r.accepted && r.issues.empty() && IsDefault(c));
	r = ParseInto(R"({"count": 9, "heavy": {}})", Strictness::Dev, c);
	KT1_CHECK(r.accepted && c.count == 9 && c.gain == 0.5f && c.extras == 40);
	r = ParseInto("{}", Strictness::Dev, c);
	KT1_CHECK(r.accepted && c.count == 7);
}

KT1_CASE("DATA-DEFAULTS-NOTES-ONLY", "a file with only notes is accepted") {
	Empty e;
	const Report r = ParseInto(R"({"_sources": ["REQ X"], "_doc": "no values"})", Strictness::Dev, e);
	KT1_CHECK(r.accepted && r.issues.empty());
}

KT1_CASE("DATA-VALUES", "every kind of value is read, and a list replaces the default list") {
	Sample c;
	const Report r = ParseInto(R"({"on": true, "count": 3, "gain": 2, "name": "x", "colour": [1, 0.45, 0],
		"heavy": {"extras": 5}, "caps": [{"scale": 2, "ribbons": 600}]})", Strictness::Dev, c);
	KT1_CHECK(r.accepted && r.issues.empty());
	KT1_CHECK(c.on && c.count == 3 && c.gain == 2.0f && c.name == "x" && c.extras == 5);
	KT1_CHECK(c.colour[0] == 1.0f && c.colour[1] == 0.45f);
	KT1_CHECK(c.caps.size() == 1 && c.caps[0].scale == 2.0f && c.caps[0].ribbons == 600);
}

KT1_CASE("DATA-FLOAT-BITS", "a float has the bits the Lua reader gave it (strtod, then float)") {
	for (const char* t : {"0.45", "0.38", "0.41", "0.005", "0.19", "0.6", "0.29999998", "0.16666667", "0.9", "1.5", "1e-3"}) {
		Sample s;
		ParseInto(std::string("{\"gain\": ") + t + "}", Strictness::Dev, s);
		KT1_CHECK_MSG(s.gain == static_cast<float>(std::strtod(t, nullptr)), t);
	}
}

KT1_CASE("DATA-UNKNOWN-DEV", "an unknown key is an error in dev, also inside objects and lists, and nothing applies") {
	Sample c;
	const Report r = ParseInto(UNKNOWN, Strictness::Dev, c);
	KT1_CHECK(!r.accepted && IsDefault(c));
	KT1_CHECK(Says(r, "cuont", true, "unknown key"));
	KT1_CHECK(Says(r, "heavy.extra", true, "unknown key"));
	KT1_CHECK(Says(r, "caps[1].point", true, "unknown key"));
}

KT1_CASE("DATA-UNKNOWN-RELEASE", "an unknown key is a warning in release and the rest applies") {
	Sample c;
	const Report r = ParseInto(UNKNOWN, Strictness::Release, c);
	KT1_CHECK(r.accepted && c.count == 3 && !r.HasErrors());
	KT1_CHECK(Says(r, "cuont", false, "unknown key"));
}

KT1_CASE("DATA-TYPE-DEV", "a wrong type rejects the file in dev and each wrong value is named") {
	Sample c;
	const Report r = ParseInto(WRONG, Strictness::Dev, c);
	KT1_CHECK(!r.accepted && IsDefault(c));
	KT1_CHECK(Says(r, "on", true, "true or false"));
	KT1_CHECK(Says(r, "count", true, "whole number"));
	KT1_CHECK(Says(r, "gain", true, "a number"));
	KT1_CHECK(Says(r, "name", true, "a string"));
	KT1_CHECK(Says(r, "colour", true, "expected 3"));
	KT1_CHECK(Says(r, "heavy", true, "an object"));
	KT1_CHECK(Says(r, "caps[2]", true, "an object"));
}

KT1_CASE("DATA-TYPE-RELEASE", "release keeps the default of each bad value, never half a colour") {
	Sample c;
	Report r = ParseInto(WRONG, Strictness::Release, c);
	KT1_CHECK(r.accepted && r.HasErrors() && IsDefault(c));
	r = ParseInto(R"({"colour": [0.5, "red", 0.5], "count": 3})", Strictness::Release, c);
	KT1_CHECK(c.colour[0] == 0.1f && c.colour[2] == 0.3f && c.count == 3);
	KT1_CHECK(Says(r, "colour[2]", true, "a number"));
}

KT1_CASE("DATA-RANGE", "a value out of range, or too large for its type, is an error and keeps the default") {
	Sample c;
	Report r = ParseInto(R"({"count": 0, "gain": 5})", Strictness::Release, c);
	KT1_CHECK(c.count == 7 && c.gain == 0.5f);
	KT1_CHECK(Says(r, "count", true, "at least 1"));
	KT1_CHECK(Says(r, "gain", true, "from 0 to 4"));
	r = ParseInto(R"({"count": 3000000000})", Strictness::Release, c);
	KT1_CHECK(c.count == 7 && Says(r, "count", true, "too large"));
	r = ParseInto(R"({"gain": 1e39})", Strictness::Release, c);
	KT1_CHECK(c.gain == 0.5f && r.HasErrors());
}

KT1_CASE("DATA-RANGE-DOUBLE", "a range is judged on the file's number as a double, before it is rounded to float") {
	Sample c;
	// 4.00000001 is above 4 as a double and equal to 4 as a float
	const Report r = ParseInto(R"({"gain": 4.00000001})", Strictness::Release, c);
	KT1_CHECK(c.gain == 0.5f);
	KT1_CHECK(Says(r, "gain", true, "out of range"));
}

KT1_CASE("DATA-NOTES", "notes are allowed and are not values") {
	Sample c;
	const Report r = ParseInto(R"({"_sources": ["ORD-010"], "_doc": "d", "_label": {"count": "DESIGN"}, "count": 2,
		"heavy": {"_label": {"extras": "PLACEHOLDER"}, "extras": 1}})", Strictness::Dev, c);
	KT1_CHECK(r.accepted && r.issues.empty() && c.count == 2);
}

KT1_CASE("DATA-NOTES-BAD", "a misspelt, misplaced or wrongly typed note, or a label for a missing key, is found") {
	Sample c;
	Report r = ParseInto(R"({"_label": {"cuont": "DESIGN"}})", Strictness::Dev, c);
	KT1_CHECK(!r.accepted && Says(r, "_label.cuont", true, "does not have"));
	r = ParseInto(R"({"_labels": {}, "heavy": {"_sources": []}})", Strictness::Dev, c);
	KT1_CHECK(Says(r, "_labels", true, "unknown note"));
	KT1_CHECK(Says(r, "heavy._sources", true, "unknown note"));
	r = ParseInto(R"({"_sources": "x", "_label": {"on": 1}, "on": true})", Strictness::Dev, c);
	KT1_CHECK(!r.accepted && Says(r, "_sources", true, "list of strings"));
	KT1_CHECK(Says(r, "_label.on", true, "a string"));
}

KT1_CASE("DATA-BROKEN", "a broken or half-saved file keeps the running values, in release too") {
	Sample c;
	c.count = 42;
	Report r = ParseInto(R"({"count": 3,)", Strictness::Release, c);
	KT1_CHECK(!r.accepted && c.count == 42 && Says(r, "", true, "not valid JSON"));
	r = ParseInto("[1, 2]", Strictness::Release, c);
	KT1_CHECK(!r.accepted && c.count == 42 && Says(r, "", true, "one object"));
	r = ParseInto("", Strictness::Release, c);
	KT1_CHECK(!r.accepted && c.count == 42);
	r = ParseInto(R"({"count": 3, "count": 4})", Strictness::Dev, c);
	KT1_CHECK(!r.accepted && c.count == 42 && Says(r, "count", true, "twice"));
}

KT1_CASE("DATA-LINE", "an issue prints with its file") {
	KT1_CHECK_EQ(Line("orders", Issue{true, "a.b", "bad"}), std::string("data/kt1/orders.json: a.b: bad"));
}

KT1_CASE("DATA-RELOAD-POLL", "the files are looked at once a poll period, well within a second") {
	PollClock clock;
	const bool first = clock.Due(100.0);
	const bool early = clock.Due(100.0 + POLL_SECONDS * 0.5);
	const bool due = clock.Due(100.0 + POLL_SECONDS);
	KT1_CHECK(first && !early && due);
	KT1_CHECK(POLL_SECONDS <= 0.5);
}

KT1_CASE("DATA-RELOAD-WATCH", "each new text of a seen file is read once") {
	Watch w;
	KT1_CHECK(!w.Changed("farview", "a"));
	w.Seen("farview", "a");
	KT1_CHECK(!w.Changed("farview", "a"));
	KT1_CHECK(w.Changed("farview", "b"));
	KT1_CHECK(!w.Changed("farview", "b"));
}

KT1_CASE("DATA-RELOAD-JUDGE", "only an unsynced, enabled system reloads live") {
	KT1_CHECK(Judge(true, true, false) == Verdict::Live);
	KT1_CHECK(Judge(true, true, true) == Verdict::NextStart);
	KT1_CHECK(Judge(true, false, false) == Verdict::Off);
	KT1_CHECK(Judge(false, false, false) == Verdict::Unknown);
}

KT1_CASE("DATA-RELOAD-COMMAND", "the command names one system") {
	KT1_CHECK_EQ(ReloadTarget("reload FarView"), std::string("farview"));
	KT1_CHECK_EQ(ReloadTarget("  reload   ribbons "), std::string("ribbons"));
	KT1_CHECK(ReloadTarget("reload").empty());
	KT1_CHECK(ReloadTarget("reload a b").empty());
	KT1_CHECK(ReloadTarget("load a").empty());
}
