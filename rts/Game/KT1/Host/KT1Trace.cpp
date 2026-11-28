/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the trace recorder. See KT1Trace.h.

#include "KT1Trace.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <string>

#include "Game/GameSetup.h"
#include "Game/GameVersion.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/Trace/TraceReplay.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/FileSystem/DataDirsAccess.h"
#include "System/FileSystem/FileQueryFlags.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace {

using Core::Trace::Writer;

// DESIGN: seconds between coverage lines, so a run that ends without a game over still has recent counts
constexpr float COVERAGE_EVERY_S = 30.0f;   // KT1 lint: allow frame-literal seconds, converted by Rate

std::string Lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

// letters, digits, - _ . ; anything else becomes _
std::string PathPart(const std::string& s) {
	std::string r;
	for (const char c : s)
		r += (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') ? c : '_';
	return (r.empty() || r == "." || r == "..") ? std::string("_") : r;
}

struct File {
	Writer writer;
	Core::Trace::Coverage coverage;
	std::ofstream out;
	std::string system, path;
	uint64_t bytes = 0;

	void Drain() {
		const auto pending = writer.Pending();
		if (pending.empty() || !out.is_open()) return;
		out.write(reinterpret_cast<const char*>(pending.data()), static_cast<std::streamsize>(pending.size()));
		bytes += pending.size();
		writer.Drained();
	}
	// the end record; safe at process exit (no engine call)
	void Finish() {
		if (!out.is_open()) return;
		writer.End();
		Drain();
		out.close();
	}
	~File() { Finish(); }
};

struct State {
	bool read = false;                         // the options of this game are read
	std::map<std::string, std::string> wanted; // lower-case name -> "" until its file is made
	std::map<std::string, File> files;         // by lower-case name
	std::string scene;
};

State state;

void ReadOptions() {
	if (gameSetup == nullptr) return;
	state.read = true;
	const auto& opts = CGameSetup::GetModOptions();
	const auto list = opts.find("kt1_trace");
	if (list == opts.end()) return;
	std::string name;
	for (const char c : std::string(list->second.c_str()) + ",") {
		if (c != ',' && c != ' ') {
			name += c;
			continue;
		}
		if (!name.empty() && name != "0") state.wanted[Lower(name)] = "";
		name.clear();
	}
	const auto scene = opts.find("kt1_trace_scene");
	state.scene = PathPart(scene == opts.end() ? std::string("scene") : std::string(scene->second.c_str()));
}

// the system's file, made on its first row; nullptr when it cannot be written
File* FileOf(const char* system) {
	const std::string key = Lower(system);
	File& f = state.files[key];
	if (f.out.is_open()) return &f;
	if (!f.path.empty()) return nullptr;   // made already: failed, or closed
	f.system = system;
	const std::string engine = PathPart(SpringVersion::GetSync());
	const std::string relative = "traces/" + engine + "/" + state.scene + "/" + PathPart(system) + ".kt1t";
	f.path = dataDirsAccess.LocateFile(relative, FileQueryFlags::WRITE | FileQueryFlags::CREATE_DIRS);
	f.out.open(f.path, std::ios::binary | std::ios::trunc);
	if (!f.out.is_open()) {
		LOG_L(L_ERROR, "[KT1TRACE] %s cannot write %s", system, f.path.c_str());
		return nullptr;
	}
	f.writer.Begin({system, engine, state.scene, static_cast<uint32_t>(Core::Rate::StepsPerSecond())});
	LOG("[KT1TRACE] %s recording %s", system, f.path.c_str());
	return &f;
}

void Report(const File& f) {
	LOG("[KT1TRACE] %s rows=%u bytes=%llu", f.system.c_str(), f.writer.Rows(), static_cast<unsigned long long>(f.bytes));
	f.coverage.Lines(f.system, [](const std::string& line) { LOG("%s", line.c_str()); });
}

} // namespace

bool Trace::On(const char* system) {
	if (!state.read) ReadOptions();
	if (state.wanted.empty()) return false;
	return state.wanted.find(Lower(system)) != state.wanted.end();
}

void Trace::Input(const char* system, const char* column, std::span<const uint8_t> bytes) {
	if (!On(system)) return;
	File* f = FileOf(system);
	if (f == nullptr) return;
	f->writer.Row(f->writer.ColumnOf(column, Core::Trace::INPUT), gs->frameNum, bytes);
	f->coverage.Row(column, gs->frameNum);
}

void Trace::Output(const char* system, const char* column, const Core::Trace::Key& key, std::span<const float> values,
		uint8_t flags) {
	if (!On(system)) return;
	File* f = FileOf(system);
	if (f == nullptr) return;
	f->writer.Output(column, flags, gs->frameNum, key, values);
	f->coverage.Row(column, gs->frameNum);
}

void Trace::CloseStep(int frame) {
	if (state.files.empty()) return;
	const bool report = frame > 0 && frame % Core::Rate::Every(COVERAGE_EVERY_S) == 0;
	for (auto& [name, f] : state.files) {
		f.Drain();
		if (report && f.out.is_open()) Report(f);
	}
}

void Trace::Close() {
	for (auto& [name, f] : state.files) {
		if (!f.out.is_open()) continue;
		f.Finish();
		Report(f);
	}
}

void Trace::Reset() {
	Close();
	state.files.clear();
	state.wanted.clear();
	state.read = false;
}

} // namespace KT1
