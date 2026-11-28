/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Capture, the engine-free half (LUA-FREE-WORKPACKAGES F07): what a JSON capture script says, how its pictures are
// named, and which drawn frame does what. No engine include. The adapter (Host/Capture) reads the file, moves the
// camera, holds the sim and reads pixels.
//
// A run takes the script's shots in order. For each shot the sim is held at the shot's frame (no later frame is made)
// and the same frame is drawn again and again: `settle` drawn frames with the pose and the side applied, then one
// read-back. With an A/B system the shot is taken twice at that held frame, side "off" then side "on". The hold moves to
// the next shot's frame only after the last read-back, so the sim never passes a shot that is still to be taken.

#ifndef KT1_CAPTURE_CORE_H
#define KT1_CAPTURE_CORE_H

#include <climits>
#include <string>
#include <utility>
#include <vector>

namespace KT1::CaptureCore {
	constexpr int NO_HOLD = INT_MAX;                                       // no frame limit
	inline constexpr const char* PICTURE_DIR = "screenshots/kt1capture";   // in the engine's write folder

	// One camera pose: the engine camera controller's state keys (dist, ry ...), and optionally the look-at point as a
	// fraction of the map's width and depth (becomes px, pz).
	struct Pose {
		std::vector<std::pair<std::string, float>> state;
		bool hasAt = false;
		float atX = 0, atZ = 0;
	};

	struct Shot {
		std::string name;     // file name stem: letters, digits, '-' and '_'
		int frame = 0;        // sim frame the picture is of
		Pose pose;
	};

	struct Script {
		int settle = 0;               // drawn frames before each read-back (2 or more: a switch takes one drawn frame)
		float timeOffset = 0;         // the interpolation offset every capture frame is drawn at, 0 to 1 of a step
		int seed = 0;                 // seed of the draw side's random numbers, set again before every drawn capture frame
		bool hideInterface = true;
		bool quit = false;            // end the game after the last shot
		int plantX = -1, plantY = -1; // tooling check: change this pixel (from the top left) of the "on" side before reading it
		std::vector<Shot> shots;
	};

	enum class Side { One, Off, On };
	const char* SideName(Side side);

	// "" when the script can be run, else the first reason it cannot.
	std::string Validate(const Script& script);
	// "screenshots/kt1capture/<shot>_<side>.png"
	std::string PicturePath(const Shot& shot, Side side);

	// What one drawn frame has to do.
	struct Step {
		int shot = -1;              // index into the script's shots; -1: not at a capture frame, draw as usual
		Side side = Side::One;      // the side to draw (apply the pose and the side before drawing)
		bool grab = false;          // read this drawn frame back after it is drawn
		bool missed = false;        // the sim is already past this shot's frame: it is skipped and reported
	};

	class Sequence {
	public:
		Sequence() = default;
		Sequence(Script script, bool ab);

		// The last sim frame that may be made now; NO_HOLD when every shot is taken or skipped.
		int HoldFrame() const;
		// Call once per drawn frame, before drawing, with the sim frame being drawn.
		Step Draw(int simFrame);
		// Call after a step with grab = true was read back (or could not be: the run goes on either way).
		void Grabbed();

		bool Done() const { return next >= script.shots.size(); }
		int Taken() const { return taken; }        // shots with every side read back
		int Missed() const { return missed; }
		bool AB() const { return ab; }
		const Script& Get() const { return script; }

	private:
		void Advance();

		Script script;
		bool ab = false;
		size_t next = 0;
		Side side = Side::One;
		int drawn = 0, taken = 0, missed = 0;
		bool grabbing = false;
	};
}

#endif
