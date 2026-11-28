/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: engine-free tests of Config, Schema and Reload (run.sh). One "[CASE] <id> PASS|FAIL <text>" line a case.
// The loader holds no sim time, so there is nothing here to run at a second sim rate.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../Config.h"
#include "../Reload.h"

using namespace KT1::Data;

namespace {

int failures = 0;

void Case(const char* id, bool ok, const char* text) {
	std::printf("[CASE] %s %s %s\n", id, ok ? "PASS" : "FAIL", text);
	failures += ok ? 0 : 1;
}

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

void Defaults() {
	Sample c;
	Report r = ParseInto("{}", Strictness::Dev, c);
	Case("data-defaults-empty", r.accepted && r.issues.empty() && IsDefault(c), "an empty object keeps every default");

	r = ParseInto(R"({"count": 9, "heavy": {}})", Strictness::Dev, c);
	Case("data-defaults-partial", r.accepted && c.count == 9 && c.gain == 0.5f && c.extras == 40, "an absent key keeps its default");

	c.count = 99;
	r = ParseInto("{}", Strictness::Dev, c);
	Case("data-defaults-reload", r.accepted && c.count == 7, "a key taken out of the file returns to its default on a reload");

	Empty e;
	r = ParseInto(R"({"_sources": ["REQ X"], "_doc": "no values"})", Strictness::Dev, e);
	Case("data-defaults-novalues", r.accepted && r.issues.empty(), "a file with only notes is accepted");
}

void Values() {
	Sample c;
	const Report r = ParseInto(R"({"on": true, "count": 3, "gain": 2, "name": "x", "colour": [1, 0.45, 0],
		"heavy": {"extras": 5}, "caps": [{"scale": 2, "ribbons": 600}]})", Strictness::Dev, c);
	Case("data-values-all", r.accepted && r.issues.empty() && c.on && c.count == 3 && c.gain == 2.0f && c.name == "x" &&
	     c.colour[0] == 1.0f && c.colour[1] == 0.45f && c.extras == 5, "every kind of value is read");
	Case("data-values-list", c.caps.size() == 1 && c.caps[0].scale == 2.0f && c.caps[0].ribbons == 600, "a list replaces the default list");

	// The numbers of the old .lua configs: Lua reads the text with strtod and keeps a float.
	const char* texts[] = {"0.45", "0.38", "0.41", "0.005", "0.19", "0.6", "0.01", "0.05", "0.9", "1.5", "1e-3"};
	bool same = true;
	for (const char* t : texts) {
		Sample s;
		ParseInto(std::string("{\"gain\": ") + t + "}", Strictness::Dev, s);
		same = same && s.gain == static_cast<float>(std::strtod(t, nullptr));
	}
	Case("data-values-float-bits", same, "a float has the bits the Lua reader gave it");
}

void UnknownKey() {
	const char* text = R"({"count": 3, "cuont": 4, "heavy": {"extra": 1}, "caps": [{"point": 2}]})";
	Sample c;
	Report r = ParseInto(text, Strictness::Dev, c);
	Case("data-unknown-dev", !r.accepted && IsDefault(c) && Says(r, "cuont", true, "unknown key"),
	     "an unknown key is an error in dev and nothing of the file applies");
	Case("data-unknown-nested", Says(r, "heavy.extra", true, "unknown key") && Says(r, "caps[1].point", true, "unknown key"),
	     "unknown keys are found inside objects and lists");
	r = ParseInto(text, Strictness::Release, c);
	Case("data-unknown-release", r.accepted && c.count == 3 && !r.HasErrors() && Says(r, "cuont", false, "unknown key"),
	     "an unknown key is a warning in release and the rest applies");
}

void WrongTypes() {
	const char* text = R"({"on": 1, "count": 2.5, "gain": "high", "name": 4, "colour": [1, 2], "heavy": 3,
		"caps": [{"scale": 1}, 5]})";
	Sample c;
	Report r = ParseInto(text, Strictness::Dev, c);
	Case("data-type-dev", !r.accepted && IsDefault(c), "a wrong type rejects the file in dev");
	Case("data-type-each", Says(r, "on", true, "true or false") && Says(r, "count", true, "whole number") &&
	     Says(r, "gain", true, "a number") && Says(r, "name", true, "a string") && Says(r, "colour", true, "expected 3") &&
	     Says(r, "heavy", true, "an object") && Says(r, "caps[2]", true, "an object"), "each wrong value is named");
	r = ParseInto(text, Strictness::Release, c);
	Case("data-type-release", r.accepted && r.HasErrors() && IsDefault(c), "release keeps the default of each bad value");

	r = ParseInto(R"({"colour": [0.5, "red", 0.5], "count": 3})", Strictness::Release, c);
	Case("data-type-colour", c.colour[0] == 0.1f && c.colour[2] == 0.3f && c.count == 3 && Says(r, "colour[2]", true, "a number"),
	     "one bad entry keeps the whole colour");
}

void Ranges() {
	Sample c;
	Report r = ParseInto(R"({"count": 0, "gain": 5})", Strictness::Release, c);
	Case("data-range", c.count == 7 && c.gain == 0.5f && Says(r, "count", true, "at least 1") && Says(r, "gain", true, "from 0 to 4"),
	     "a value out of range is an error and keeps the default");
	r = ParseInto(R"({"count": 3000000000})", Strictness::Release, c);
	const bool bigInt = c.count == 7 && Says(r, "count", true, "too large");
	r = ParseInto(R"({"gain": 1e39})", Strictness::Release, c);
	Case("data-range-size", bigInt && c.gain == 0.5f && r.HasErrors(), "a number too large for its type is an error");
}

void Notes() {
	Sample c;
	Report r = ParseInto(R"({"_sources": ["ORD-010"], "_doc": "d", "_label": {"count": "DESIGN"}, "count": 2,
		"heavy": {"_label": {"extras": "PLACEHOLDER"}, "extras": 1}})", Strictness::Dev, c);
	Case("data-notes", r.accepted && r.issues.empty() && c.count == 2, "notes are allowed and are not values");
	r = ParseInto(R"({"_label": {"cuont": "DESIGN"}})", Strictness::Dev, c);
	Case("data-notes-label-key", !r.accepted && Says(r, "_label.cuont", true, "does not have"), "a label must name a key of its object");
	r = ParseInto(R"({"_labels": {}, "heavy": {"_sources": []}})", Strictness::Dev, c);
	Case("data-notes-unknown", Says(r, "_labels", true, "unknown note") && Says(r, "heavy._sources", true, "unknown note"),
	     "a misspelt or misplaced note is found");
	r = ParseInto(R"({"_sources": "x", "_label": {"on": 1}, "on": true})", Strictness::Dev, c);
	Case("data-notes-type", !r.accepted && Says(r, "_sources", true, "list of strings") && Says(r, "_label.on", true, "a string"),
	     "notes have types");
}

void Broken() {
	Sample c;
	c.count = 42;
	Report r = ParseInto(R"({"count": 3,)", Strictness::Release, c);
	Case("data-broken-json", !r.accepted && c.count == 42 && Says(r, "", true, "not valid JSON"),
	     "a broken file keeps the running values, in release too");
	r = ParseInto("[1, 2]", Strictness::Release, c);
	Case("data-broken-root", !r.accepted && c.count == 42 && Says(r, "", true, "one object"), "the file must hold an object");
	r = ParseInto("", Strictness::Release, c);
	Case("data-broken-empty", !r.accepted && c.count == 42, "an empty file (a save half done) changes nothing");
	r = ParseInto(R"({"count": 3, "count": 4})", Strictness::Dev, c);
	Case("data-broken-twice", !r.accepted && c.count == 42 && Says(r, "count", true, "twice"), "a repeated key is an error");
	Case("data-line", Line("orders", Issue{true, "a.b", "bad"}) == "data/kt1/orders.json: a.b: bad", "an issue prints with its file");
}

void ReloadLogic() {
	PollClock clock;
	const bool first = clock.Due(100.0), early = clock.Due(100.0 + POLL_SECONDS * 0.5), due = clock.Due(100.0 + POLL_SECONDS);
	Case("data-reload-poll", first && !early && due, "the files are looked at once a poll period");
	Case("data-reload-second", POLL_SECONDS <= 0.5, "a change is seen well within a second");

	Watch w;
	const bool unseen = w.Changed("farview", "a");
	w.Seen("farview", "a");
	const bool same = w.Changed("farview", "a"), changed = w.Changed("farview", "b"), again = w.Changed("farview", "b");
	Case("data-reload-watch", !unseen && !same && changed && !again, "each new text is read once");

	Case("data-reload-judge", Judge(true, true, false) == Verdict::Live && Judge(true, true, true) == Verdict::NextStart &&
	     Judge(true, false, false) == Verdict::Off && Judge(false, false, false) == Verdict::Unknown,
	     "only an unsynced, enabled system reloads live");
	Case("data-reload-command", ReloadTarget("reload FarView") == "farview" && ReloadTarget("  reload   ribbons ") == "ribbons" &&
	     ReloadTarget("reload").empty() && ReloadTarget("reload a b").empty() && ReloadTarget("load a").empty(),
	     "the command names one system");
}

} // namespace

int main() {
	Defaults();
	Values();
	UnknownKey();
	WrongTypes();
	Ranges();
	Notes();
	Broken();
	ReloadLogic();
	std::printf("%d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
