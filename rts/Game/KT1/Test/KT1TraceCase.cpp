/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free replay cases. See KT1TraceCase.h.

#include "KT1TraceCase.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace KT1::Test {
namespace {
	struct TraceDef { std::string system, file; MakeReplayFn make; };

	std::vector<TraceDef>& Traces() {
		static std::vector<TraceDef> traces;
		return traces;
	}

	bool ReadAll(const std::filesystem::path& path, std::vector<uint8_t>& out) {
		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in) return false;
		out.resize(static_cast<size_t>(in.tellg()));
		in.seekg(0);
		in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
		return static_cast<bool>(in);
	}

	void RunTrace(Case& t, const std::string& id, const std::filesystem::path& path, MakeReplayFn make) {
		std::vector<uint8_t> data;
		if (!ReadAll(path, data)) {
			t.Fail("cannot read " + path.string());
			return;
		}
		const auto replay = make();
		const auto start = std::chrono::steady_clock::now();
		const Core::Trace::ReplayResult r = Core::Trace::Replay(data, *replay);
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		std::printf("[TRACE] %s bytes=%zu steps=%u inputs=%llu compared=%llu differences=%llu first_diff_step=%d ms=%lld\n",
			id.c_str(), data.size(), r.steps, static_cast<unsigned long long>(r.inputs),
			static_cast<unsigned long long>(r.compared), static_cast<unsigned long long>(r.differences), r.firstDiffStep,
			static_cast<long long>(ms));
		for (const std::string& d : r.described) std::printf("[TRACE] %s diff %s\n", id.c_str(), d.c_str());
		t.CountCheck();
		if (!r.error.empty()) t.Fail(r.error);
		else if (r.compared == 0) t.Fail("the trace has nothing to compare");
		else if (r.differences > 0) t.Fail(std::to_string(r.differences) + " differences, first: " + r.described.front());
	}

	// the files named `name` under dir, in path order
	std::vector<std::filesystem::path> Find(const std::string& dir, const std::string& name) {
		std::vector<std::filesystem::path> found;
		std::error_code error;
		for (std::filesystem::recursive_directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
			if (it->is_regular_file(error) && it->path().filename().string() == name) found.push_back(it->path());
		std::sort(found.begin(), found.end());
		return found;
	}
}

TraceRegistrar::TraceRegistrar(const char* system, const char* file, MakeReplayFn make) {
	Traces().push_back({system, file, make});
}

void AddTraceCases(const std::string& dir, std::vector<CaseDef>& cases) {
	for (const TraceDef& def : Traces()) {
		const std::vector<std::filesystem::path> found = Find(dir, def.file);
		if (found.empty()) {
			const std::string what = "no " + def.file + " under " + dir;
			cases.push_back({"TRACE-" + def.system, "replay of " + def.file, [what](Case& t) { t.Fail(what); }});
			continue;
		}
		for (const std::filesystem::path& path : found) {
			// <engine>/<scene>/<system>.kt1t
			const std::string id = "TRACE-" + def.system + "-" + path.parent_path().parent_path().filename().string() + "-" +
				path.parent_path().filename().string();
			const MakeReplayFn make = def.make;
			cases.push_back({id, "replay of " + def.file, [id, path, make](Case& t) { RunTrace(t, id, path, make); }});
		}
	}
}

} // namespace KT1::Test
