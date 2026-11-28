/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Capture Core, engine-free (LUA-FREE F07): the script, the file names and the order of drawn frames. No rate cases:
// shots are listed sim frames, not periods.

#include "../KT1Test.h"
#include "Game/KT1/Core/Capture/CaptureCore.h"
#include "Game/KT1/Host/Capture/CaptureScript.h"

#include <string>
#include <vector>

using namespace KT1::CaptureCore;

namespace {
	Script Three() {
		Script s;
		s.settle = 3;
		s.shots = {{"L1", 100, {}}, {"L4", 100, {}}, {"L7", 130, {}}};
		return s;
	}

	// Draws as the engine would: the sim advances one frame a draw but never past the hold. "name:side:frame" per read-back.
	std::vector<std::string> Play(Sequence& q, int last) {
		std::vector<std::string> out;
		int frame = 0;
		for (int n = 0; n < 1000 && !(q.Done() && frame >= last); ++n) {
			if (frame < q.HoldFrame() && frame < last) ++frame;
			const Step s = q.Draw(frame);
			if (!s.grab) continue;
			out.push_back(q.Get().shots[s.shot].name + ":" + SideName(s.side) + ":" + std::to_string(frame));
			q.Grabbed();
		}
		return out;
	}

	std::string Parse(const char* text) {
		Script s;
		s.settle = 8;
		return ParseScript(text, s);
	}
}

KT1_CASE("CAPTURE-ORDER-AB", "each shot off then on at its own held frame, in script order; the hold ends after the last") {
	Sequence q(Three(), true);
	const std::vector<std::string> want = {"L1:off:100", "L1:on:100", "L4:off:100", "L4:on:100", "L7:off:130", "L7:on:130"};
	KT1_CHECK((Play(q, 140) == want));
	KT1_CHECK(q.Done());
	KT1_CHECK_EQ(q.Taken(), 3);
	KT1_CHECK_EQ(q.Missed(), 0);
	KT1_CHECK_EQ(q.HoldFrame(), NO_HOLD);
}

KT1_CASE("CAPTURE-ORDER-ONE", "without A/B each shot is read once") {
	Sequence q(Three(), false);
	const std::vector<std::string> want = {"L1:one:100", "L4:one:100", "L7:one:130"};
	KT1_CHECK((Play(q, 140) == want));
}

KT1_CASE("CAPTURE-HOLD", "nothing is applied before a shot; then settle drawn frames, a read-back, and the hold stays for the other side") {
	Sequence q(Three(), true);
	for (int f = 1; f < 100; ++f) {
		KT1_CHECK_EQ(q.Draw(f).shot, -1);
		KT1_CHECK_EQ(q.HoldFrame(), 100);
	}
	int draws = 1;
	while (!q.Draw(100).grab && draws < 50) ++draws;
	KT1_CHECK_EQ(draws, 4);
	q.Grabbed();
	KT1_CHECK_EQ(q.HoldFrame(), 100);
	KT1_CHECK(q.Draw(100).side == Side::On);
}

KT1_CASE("CAPTURE-MISSED", "a frame already passed is reported and never read; the run goes on") {
	Sequence q(Three(), false);
	const Step s = q.Draw(120);
	KT1_CHECK(s.missed);
	KT1_CHECK(!s.grab);
	q.Draw(120);
	KT1_CHECK_EQ(q.Missed(), 2);
	KT1_CHECK_EQ(q.Taken(), 0);
	KT1_CHECK_EQ(q.HoldFrame(), 130);
}

KT1_CASE("CAPTURE-GRABBED", "Grabbed after a draw without a read-back changes nothing") {
	Sequence q(Three(), false);
	q.Draw(100);
	q.Grabbed();
	KT1_CHECK_EQ(q.Taken(), 0);
	KT1_CHECK_EQ(q.HoldFrame(), 100);
}

KT1_CASE("CAPTURE-VALIDATE", "a script with bad shots is refused") {
	KT1_CHECK_EQ(Validate(Three()), std::string());
	Script s = Three(); s.shots[1].name = "L1";  KT1_CHECK(!Validate(s).empty());
	s = Three(); s.shots[2].frame = 99;          KT1_CHECK(!Validate(s).empty());
	s = Three(); s.shots[0].frame = 0;           KT1_CHECK(!Validate(s).empty());
	s = Three(); s.shots[0].name = "a/b";        KT1_CHECK(!Validate(s).empty());
	s = Three(); s.shots[0].name = "";           KT1_CHECK(!Validate(s).empty());
	s = Three(); s.settle = 1;                   KT1_CHECK(!Validate(s).empty());
	s = Three(); s.settle = 2;                   KT1_CHECK(Validate(s).empty());
	s = Three(); s.timeOffset = 1.5f;            KT1_CHECK(!Validate(s).empty());
	s = Three(); s.timeOffset = -0.5f;           KT1_CHECK(!Validate(s).empty());
	s = Three(); s.shots.clear();                KT1_CHECK(!Validate(s).empty());
}

KT1_CASE("CAPTURE-PARSE", "every key is read; the config's settle stays when the script names none") {
	Script s;
	s.settle = 8;
	const std::string why = ParseScript(R"({"_sources": "x", "seed": 7, "time_offset": 0.25, "hide_interface": false, "quit": true, "plant_pixel": [3, 4],
		"shots": [{"name": "L1", "frame": 600, "at": [0.5, 0.25], "camera": {"dist": 600, "ry": -1.5}}, {"name": "L2", "frame": 700}]})", s);
	KT1_CHECK_EQ(why, std::string());
	KT1_CHECK_EQ(s.settle, 8);
	KT1_CHECK_EQ(s.seed, 7);
	KT1_CHECK(s.timeOffset == 0.25f);
	KT1_CHECK(!s.hideInterface);
	KT1_CHECK(s.quit);
	KT1_CHECK_EQ(s.plantX, 3);
	KT1_CHECK_EQ(s.plantY, 4);
	KT1_CHECK_EQ(s.shots.size(), size_t(2));
	if (s.shots.size() != 2) return;
	KT1_CHECK_EQ(s.shots[0].frame, 600);
	KT1_CHECK(s.shots[0].pose.hasAt);
	KT1_CHECK(s.shots[0].pose.atX == 0.5f && s.shots[0].pose.atZ == 0.25f);
	KT1_CHECK((s.shots[0].pose.state == std::vector<std::pair<std::string, float>>{{"dist", 600.0f}, {"ry", -1.5f}}));
	KT1_CHECK(!s.shots[1].pose.hasAt);
	KT1_CHECK_EQ(PicturePath(s.shots[0], Side::Off), std::string("screenshots/kt1capture/L1_off.png"));
	KT1_CHECK_EQ(Parse(R"({"settle_draws": 2, "shots": [{"name": "a", "frame": 1}]})"), std::string());
}

KT1_CASE("CAPTURE-PARSE-REFUSE", "unknown keys, wrong types and broken text are refused") {
	KT1_CHECK(!Parse("").empty());
	KT1_CHECK(!Parse("[1]").empty());
	KT1_CHECK(!Parse(R"({"shots": [{"name": "a", "frame": 1}], "setle_draws": 2})").empty());
	KT1_CHECK(!Parse(R"({"shots": [{"name": "a", "frame": 1, "zoom": 2}]})").empty());
	KT1_CHECK(!Parse(R"({"shots": [{"name": "a", "frame": "1"}]})").empty());
	KT1_CHECK(!Parse(R"({"shots": [{"name": "a", "frame": 1, "at": [0.5]}]})").empty());
	KT1_CHECK(!Parse(R"({"shots": [{"name": "a", "frame": 1, "camera": {"dist": "far"}}]})").empty());
	KT1_CHECK(!Parse(R"({"shots": [{"name": "a", "frame": 1}], "quit": 1})").empty());
	KT1_CHECK(!Parse(R"({"shots": {}})").empty());
	KT1_CHECK(!Parse(R"({"shots": [{"name": "a", "frame": 1}], "time_offset": 2})").empty());
}
