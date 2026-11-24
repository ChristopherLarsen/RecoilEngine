/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: IconCull (version 1). The engine's unit icon passes skip units whose icon draws nothing.
//
// Why:        the game draws its own icons (the Lua icon lexicon) and gives every unit an engine icon type with a fully transparent
//             bitmap (game-build gamedata/icontypes.lua lex3_none, icons/none.png). The engine still projects and emits an invisible
//             icon for every unit each frame: 2.5-3.8 ms (Draw::Screen::Icons) with 0-5 icons visible
//             (game-build docs/engine/DRAW-BREAKDOWN-FIGHT.md, option 7). Skipping them changes no pixel.
// Replaces:   nothing in Lua (an engine pass made cheaper); its "Lua" side is the engine's own unculled loop when off.
// Switch:     mod option kt1_iconcull (default on: no visual change). Off: the stock loops, for A/B.
// Config:     gamedata/kt1/iconcull.lua: alpha_max (an icon is empty when every pixel's alpha is at most this; DESIGN 0)
// Unit data:  none: decided per icon type from its bitmap, never by unit name.
// Hooks:      rts/Rendering/Units/UnitDrawer.cpp, one line in DrawUnitIcons and in DrawUnitIconsScreen (units and dead ghosts):
//             `if (KT1::IconCull::Skip(idx)) continue;` before the unit's position is read.
// Timer:      none of its own (the hook is a per-unit test); measure with Draw::Screen::Icons. KT1::IconCull times the per-frame refresh.
// Lua:        Spring.KT1.IconCull.Stats() -> { emptyIcons, checkedIcons, skipped (units skipped in the last drawn frame) }
// Refresh:    icons can be replaced at run time (Spring.AddUnitIcon / FreeUnitIcon): each drawn frame the checked icons' file names
//             are compared with the icon handler's (a few hundred string compares); a changed one is checked again.
// Determinism: draw only.

#include "KT1IconCull.h"

#include <memory>
#include <string>

#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Rendering/IconHandler.h"
#include "Rendering/Textures/Bitmap.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace IconCull {
	bool active = false;
	std::vector<uint8_t> empty;
	size_t skipped = 0;

	namespace {
		std::vector<std::string> checkedName;   // per icon index: the file name its state was read from
		int alphaMax = 0;
		size_t emptyCount = 0, checkedCount = 0, lastSkipped = 0;
	}

	bool Check(size_t iconIdx) {
		if (iconIdx >= empty.size()) { empty.resize(iconIdx + 1, 2); checkedName.resize(iconIdx + 1); }
		const std::string& file = icon::iconHandler.GetIconData(iconIdx).GetFileName();
		bool isEmpty = false;
		CBitmap bm;
		if (!file.empty() && bm.Load(file) && bm.channels == 4 && bm.GetDataTypeSize() == 1) {
			const uint8_t* p = bm.GetRawMem();
			const size_t n = (size_t)bm.xsize * bm.ysize;
			isEmpty = n > 0;
			for (size_t i = 0; i < n && isEmpty; ++i) isEmpty = p[i * 4 + 3] <= alphaMax;
		}
		if (empty[iconIdx] == 1) emptyCount -= 1;
		if (empty[iconIdx] == 2) checkedCount += 1;
		empty[iconIdx] = isEmpty ? 1 : 0;
		emptyCount += isEmpty ? 1 : 0;
		checkedName[iconIdx] = file;
		if (isEmpty) LOG("[KT1] IconCull: icon %d (%s) is empty: its units are skipped", (int)iconIdx, file.c_str());
		return isEmpty;
	}

	namespace {
	class IconCullRule : public KT1::Rule {
	public:
		IconCullRule() : KT1::Rule({"IconCull", 1, false, "(engine icon pass, unculled)", true}) {}

		void LoadConfig(const LuaTable& cfg) override { alphaMax = cfg.GetInt("alpha_max", 0); }
		void Start() override { active = true; }

		void DrawWorld() override {           // once per drawn frame, before Draw::Screen: refresh changed icons, reset the counter
			for (size_t i = 0; i < empty.size(); ++i) {
				if (empty[i] == 2) continue;
				if (icon::iconHandler.GetIconData(i).GetFileName() != checkedName[i]) Check(i);
			}
			lastSkipped = skipped;
			skipped = 0;
		}

		void PushLua(lua_State* L) override { LuaPushRawNamedCFunc(L, "Stats", LStats); }

	private:
		static int LStats(lua_State* L) {
			lua_newtable(L);
			LuaPushNamedNumber(L, "emptyIcons", (lua_Number)emptyCount);
			LuaPushNamedNumber(L, "checkedIcons", (lua_Number)checkedCount);
			LuaPushNamedNumber(L, "skipped", (lua_Number)lastSkipped);
			LuaPushNamedBool(L, "active", active);
			return 1;
		}
	};
	}
}

std::unique_ptr<Rule> MakeIconCull() { return std::make_unique<IconCull::IconCullRule>(); }

} // namespace KT1
