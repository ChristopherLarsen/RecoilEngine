/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the trace recorder (LUA-FREE H02, Core/Trace/TraceFile.h). Synced code only.
//
// Mod options:
//   kt1_trace=<system,system>   record these systems (registered names); unset or "0": nothing is recorded
//   kt1_trace_scene=<name>      the scene folder of the files (letters, digits, - _); unset: "scene"
// One file per system under the engine's write folder: traces/<engine>/<scene>/<system>.kt1t
//
// What goes in:
//   the adapter, what it feeds Core, where it feeds it:   if (Trace::On("Orders")) Trace::Input("Orders", "queue", bytes);
//   the replaced Lua, what it answers: every Spring.KT1.Check.Answer is recorded by itself (column = what), and
//     Spring.KT1.Trace.Out(system, column, id1, id2, id3, values...)    one row, compared by key
//     Spring.KT1.Trace.List(system, column, id1, id2, id3, values...)   one row of an ordered column
//     Spring.KT1.Trace.Path(system, name)                               a code path taken (coverage only)
//     Spring.KT1.Trace.On(system)                                       ask once; record nothing when false
//     Spring.KT1.Trace.Close()                                          end the files now (a test run's end)
// A column is one name: the check helper's `what`, the recorder's column and the replay's output column are the same.
//
// Coverage lines ("[KT1TRACE] <system> coverage <column> steps= rows=") are logged every COVERAGE_EVERY_S, at game over
// and on Close. Off (no kt1_trace): On() is one flag test, nothing else runs, no file is made.

#ifndef KT1_HOST_TRACE_H
#define KT1_HOST_TRACE_H

#include <cstdint>
#include <span>

#include "Game/KT1/Core/Trace/TraceFile.h"

namespace KT1 {

class Trace {
public:
	static bool On(const char* system);

	static void Input(const char* system, const char* column, std::span<const uint8_t> bytes);
	// flags: Core::Trace::ORDERED, NOT_COMPARED
	static void Output(const char* system, const char* column, const Core::Trace::Key& key, std::span<const float> values,
		uint8_t flags = 0);

	static void CloseStep(int frame);   // KT1 hook: SimHost (Phases, FrameEnd): rows to disk
	static void Close();                // game over or a test's end: the files are complete
	static void Reset();                // game shutdown: Close, and the next game reads its own options
};

} // namespace KT1

#endif
