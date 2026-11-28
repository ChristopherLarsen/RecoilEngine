/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free Core tests: reads the JSON case files (format in KT1CaseLoader.h).

#include "KT1CaseLoader.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>

namespace fs = std::filesystem;

namespace KT1::Test {
	namespace {
		std::map<std::string, JsonHandler>& Handlers() {
			static std::map<std::string, JsonHandler> handlers;
			return handlers;
		}

		void AddBroken(std::vector<CaseDef>& cases, const std::string& id, const std::string& why) {
			cases.push_back({id, "case data", [why](Case& t) { t.Fail(why); }});
		}

		std::vector<fs::path> Sorted(const fs::path& dir) {
			std::vector<fs::path> paths;
			for (const fs::directory_entry& entry: fs::directory_iterator(dir))
				paths.push_back(entry.path());

			std::sort(paths.begin(), paths.end());
			return paths;
		}

		// returns the number of cases the file gave; throws when the file or an entry is malformed
		int LoadFile(const fs::path& file, JsonHandler handler, std::vector<CaseDef>& cases) {
			// the parser owns the document its elements point into, so each case keeps it alive
			const auto parser = std::make_shared<simdjson::dom::parser>();
			const simdjson::dom::element document = parser->load(file.string());

			int count = 0;
			for (const simdjson::dom::element entry: simdjson::dom::array(document["cases"])) {
				const std::string_view id = entry["id"];
				const std::string_view text = entry["text"];

				cases.push_back({std::string(id), std::string(text),
					[parser, handler, entry](Case& t) { handler(t, entry); }});
				++count;
			}
			return count;
		}

		int LoadSystem(const fs::path& dir, JsonHandler handler, std::vector<CaseDef>& cases) {
			int count = 0;
			for (const fs::path& file: Sorted(dir)) {
				if (file.extension() != ".json")
					continue;

				const std::string name = dir.filename().string() + "/" + file.filename().string();
				try {
					count += LoadFile(file, handler, cases);
				} catch (const std::exception& error) {
					AddBroken(cases, name, error.what());
					++count;   // reported here, not again as "no case"
				}
			}
			return count;
		}
	}

	JsonRegistrar::JsonRegistrar(const char* system, JsonHandler handler) {
		if (!Handlers().emplace(system, handler).second)
			Registry().push_back({std::string(system) + "/handler", "case data",
				[](Case& t) { t.Fail("two KT1_JSON_CASES handlers for this system"); }});
	}

	void LoadJsonCases(const std::string& dir, std::vector<CaseDef>& cases) {
		std::map<std::string, int> loaded;

		if (fs::is_directory(dir)) {
			for (const fs::path& sub: Sorted(dir)) {
				if (!fs::is_directory(sub))
					continue;

				const std::string system = sub.filename().string();
				const auto handler = Handlers().find(system);

				if (handler == Handlers().end())
					AddBroken(cases, system + "/handler", "no KT1_JSON_CASES handler for this folder");
				else
					loaded[system] = LoadSystem(sub, handler->second, cases);
			}
		}

		for (const auto& [system, handler]: Handlers())
			if (loaded[system] == 0)
				AddBroken(cases, system + "/files", "a handler but no case under " + dir);
	}
}
