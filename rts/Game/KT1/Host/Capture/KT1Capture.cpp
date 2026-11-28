/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 system: Capture (version 1). Pictures of the drawn frame at the frames and camera poses of a JSON capture script.
//
// Why:        a draw system moved into the engine is proved pixel for pixel (game-build docs/engine/rules/capture.md).
// Replaces:   game-build code/LuaUI/lib/pixel_harness.lua for new proofs (gl.ReadPixels in a widget).
// Switch:     mod option kt1_capture (default off). Options: kt1_capture_script=<name> reads
//             data/kt1/captures/<name>.json from the game archive; kt1_capture_ab=<system> takes every shot twice at
//             one held sim frame, with that unsynced KT1 system off and then on (Registry::SetEnabled). In capture mode
//             its Lua stays loaded and draws the off side (KT1.StandDown, LUA-FREE F01).
// Config:     gamedata/kt1/capture.lua: settle_draws (a script's own value wins).
// Phases:     a draw system (Host/DrawPhases, F06): PreDraw applies the shot's camera, side, clock, time offset and
//             random seed before the frame's world update; FrameEnd reads the frame back before the buffer swap.
// Hooks:      KT1Capture.h HoldAt: one line in the server's frame loop, in its own commit.
// Runs only:  on a windowed engine that hosts a solo game (one player; AI opponents are fine).
// Output:     <data folder>/screenshots/kt1capture/<shot>_<off|on|one>.png and the log lines
//             "[KT1CAPTURE] shot=S side=... frame=F file=PATH size=WxH" and "[KT1CAPTURE] done shots=N missed=M".
// Lua:        Spring.KT1.Capture.Side() -> "off" | "on" | nil: the side a Lua path under A/B draws for.
// Determinism: draw only. The hold delays sim frames as a pause does and changes nothing in them.
// Logic:      Core/Capture (the script, the picture names, the order of shots, sides and settle frames).

#include "KT1Capture.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "Game/Camera/CameraController.h"
#include "Game/CameraHandler.h"
#include "Game/Game.h"
#include "Game/GameSetup.h"
#include "Game/GameVersion.h"
#include "Game/GlobalUnsynced.h"
#include "Game/KT1/Host/DrawPhases.h"
#include "Game/KT1/Core/Capture/CaptureCore.h"
#include "Game/KT1/Host/Capture/CaptureScript.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/KT1Registry.h"
#include "Game/Players/PlayerBase.h"
#include "Game/UI/UnitTracker.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Map/ReadMap.h"
#include "Net/GameServer.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Textures/Bitmap.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"

namespace KT1 {
namespace Capture {
	namespace {
		using CaptureCore::Pose;
		using CaptureCore::Script;
		using CaptureCore::Shot;
		using CaptureCore::Side;

		constexpr float PLANT_COLOUR[4] = {1.0f, 0.0f, 1.0f, 1.0f};

		std::string ModOption(const char* key) {
			const auto& opts = CGameSetup::GetModOptions();
			const auto it = opts.find(key);
			return (it == opts.end()) ? std::string() : std::string(it->second.c_str());
		}

		constexpr int LAST_DRAW_ORDER = std::numeric_limits<int>::max();   // DESIGN: after every other draw system, so the read-back sees their work

		class CaptureRule : public Rule, public DrawSystem {
		public:
			CaptureRule();
			~CaptureRule() override;

			void LoadConfig(const LuaTable& cfg) override;
			void Start() override;
			void PushLua(lua_State* L) override;

			const char* Name() const override { return "Capture"; }
			unsigned Phases() const override { return PhaseBit(DrawPhase::PreDraw) | PhaseBit(DrawPhase::FrameEnd); }
			int Order() const override { return LAST_DRAW_ORDER; }
			bool Active() const override { return running; }
			void Draw(const DrawContext& ctx) override { (ctx.phase == DrawPhase::PreDraw) ? Before() : After(); }
			void Kill() override { Stop(); }

			const char* SideNow() const { return (running && step.shot >= 0 && sequence.AB()) ? CaptureCore::SideName(step.side) : nullptr; }

		private:
			void Before();
			void After();
			void Stop();
			bool SetSide(Side side);
			void Apply(const Shot& shot);
			void Restore();
			void ReadBack(const Shot& shot);
			void Finish();

			CaptureCore::Sequence sequence;
			CaptureCore::Step step;
			std::string abSystem;
			bool running = false;
			bool applied = false;
			int abOn = -1;     // what the A/B system was last switched to (-1: not switched yet)
			// the values the capture frame overrides, put back after it
			float timeOffset = 0.0f, gameTime = 0.0f, modGameTime = 0.0f;
			spring_time drawTime;
			bool hidden = false;
		};

		CaptureRule* active = nullptr;

		CaptureRule::CaptureRule(): Rule({"Capture", 1, false, "LuaUI/lib/pixel_harness.lua", false}) {}

		CaptureRule::~CaptureRule() {
			Stop();
			if (active == this) active = nullptr;
		}

		void CaptureRule::Stop() {
			running = false;
			holdStep = INT_MAX;
		}

		void CaptureRule::Start() {
			if (running) DrawPhases::Add(this);
		}

		void CaptureRule::LoadConfig(const LuaTable& cfg) {
			if (SpringVersion::IsHeadless()) {
				LOG_L(L_ERROR, "[KT1CAPTURE] refused: a headless engine draws nothing to capture");
				return;
			}
			// the hold stops this machine's server: the host of a game no other player is waiting in
			if (gameServer == nullptr || gameSetup->GetPlayerStartingDataCont().size() != 1) {
				LOG_L(L_ERROR, "[KT1CAPTURE] refused: a capture run is a solo game on its own host");
				return;
			}
			const std::string name = ModOption("kt1_capture_script");
			const std::string path = "data/kt1/captures/" + name + ".json";
			CFileHandler file(path, SPRING_VFS_ZIP);
			std::string text;
			if (name.empty() || !file.FileExists() || !file.LoadStringData(text)) {
				LOG_L(L_ERROR, "[KT1CAPTURE] refused: no capture script '%s' (mod option kt1_capture_script)", path.c_str());
				return;
			}
			Script script;
			script.settle = cfg.GetInt("settle_draws", 0);
			const std::string why = CaptureCore::ParseScript(text, script);
			if (!why.empty()) {
				LOG_L(L_ERROR, "[KT1CAPTURE] refused: %s: %s", path.c_str(), why.c_str());
				return;
			}
			abSystem = ModOption("kt1_capture_ab");
			sequence = CaptureCore::Sequence(std::move(script), !abSystem.empty());
			holdStep = sequence.HoldFrame();
			running = true;
			active = this;
			LOG("[KT1CAPTURE] script %s: %d shot(s), ab=%s", name.c_str(), (int)sequence.Get().shots.size(), abSystem.empty() ? "none" : abSystem.c_str());
		}

		// switched only when the side changes (each switch is logged by the registry)
		bool CaptureRule::SetSide(Side side) {
			const int on = (side == Side::On) ? 1 : 0;
			if (!sequence.AB() || abOn == on) return true;
			if (!Registry::Get().SetEnabled(abSystem.c_str(), on != 0)) {
				LOG_L(L_ERROR, "[KT1CAPTURE] refused: kt1_capture_ab=%s is not an unsynced KT1 system switched on or check", abSystem.c_str());
				Stop();
				return false;
			}
			abOn = on;
			return true;
		}

		void CaptureRule::Apply(const Shot& shot) {
			const Script& script = sequence.Get();
			if (!SetSide(step.side)) return;
			hidden = game->hideInterface;
			timeOffset = globalRendering->timeOffset;
			drawTime = globalRendering->grTime;
			gameTime = gu->gameTime;
			modGameTime = gu->modGameTime;
			applied = true;

			// every capture frame of a run, and the same shot in another run, draws the same instant
			const float seconds = shot.frame * INV_GAME_SPEED;
			game->hideInterface = script.hideInterface;
			globalRendering->timeOffset = script.timeOffset;   // held, the engine's own value drifts with the wall clock
			globalRendering->grTime = spring_time::fromNanoSecs(static_cast<int64_t>(seconds * 1.0e9));   // DESIGN: nanoseconds in a second
			gu->gameTime = seconds;
			gu->modGameTime = seconds;
			guRNG.SetSeed(script.seed);
			unitTracker.Disable();   // it moves the camera after this call

			const Pose& pose = shot.pose;
			CCameraController::StateMap state;
			for (const auto& [key, value] : pose.state) state[key] = value;
			if (pose.hasAt) {
				state["px"] = pose.atX * mapDims.mapx * SQUARE_SIZE;
				state["pz"] = pose.atZ * mapDims.mapy * SQUARE_SIZE;
			}
			if (!state.empty()) camHandler->SetState(state);
		}

		void CaptureRule::Restore() {
			if (!applied) return;
			applied = false;
			game->hideInterface = hidden;
			globalRendering->timeOffset = timeOffset;
			globalRendering->grTime = drawTime;
			gu->gameTime = gameTime;
			gu->modGameTime = modGameTime;
		}

		void CaptureRule::Before() {
			if (!running) return;
			step = sequence.Draw(gs->frameNum);
			holdStep = sequence.HoldFrame();
			if (step.shot < 0) return;
			const Shot& shot = sequence.Get().shots[step.shot];
			if (step.missed) {
				LOG_L(L_ERROR, "[KT1CAPTURE] missed shot=%s: the sim is at frame %d, past %d", shot.name.c_str(), gs->frameNum, shot.frame);
				if (sequence.Done()) Finish();
				return;
			}
			Apply(shot);
		}

		void CaptureRule::ReadBack(const Shot& shot) {
			const Script& script = sequence.Get();
			const int w = globalRendering->winSizeX;
			const int h = globalRendering->winSizeY;
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			if (step.side == Side::On && script.plantX >= 0 && script.plantX < w && script.plantY >= 0 && script.plantY < h) {
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				glPushAttrib(GL_SCISSOR_BIT | GL_COLOR_BUFFER_BIT);
				glEnable(GL_SCISSOR_TEST);
				glScissor(script.plantX, h - 1 - script.plantY, 1, 1);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				glClearColor(PLANT_COLOUR[0], PLANT_COLOUR[1], PLANT_COLOUR[2], PLANT_COLOUR[3]);
				glClear(GL_COLOR_BUFFER_BIT);
				glPopAttrib();
			}
			std::vector<uint8_t> pixels((size_t)w * h * 4);   // DESIGN: bytes of an RGBA pixel
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glReadBuffer(GL_BACK);
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			glPixelStorei(GL_PACK_ALIGNMENT, 4);   // DESIGN: GL's default, restored

			const std::string path = CaptureCore::PicturePath(shot, step.side);
			CBitmap bmp(pixels.data(), w, h);
			bmp.ReverseYAxis();
			if (!FileSystem::CreateDirectory(CaptureCore::PICTURE_DIR) || !bmp.Save(path, true, false)) {
				LOG_L(L_ERROR, "[KT1CAPTURE] could not write %s", path.c_str());
				return;
			}
			LOG("[KT1CAPTURE] shot=%s side=%s frame=%d file=%s size=%dx%d", shot.name.c_str(), CaptureCore::SideName(step.side),
			    gs->frameNum, path.c_str(), w, h);
		}

		void CaptureRule::After() {
			if (!running || !applied) return;
			if (step.grab) {
				ReadBack(sequence.Get().shots[step.shot]);
				sequence.Grabbed();
				holdStep = sequence.HoldFrame();
			}
			Restore();
			if (sequence.Done()) Finish();
		}

		void CaptureRule::Finish() {
			SetSide(Side::On);
			Stop();
			LOG("[KT1CAPTURE] done shots=%d missed=%d", sequence.Taken(), sequence.Missed());
			if (sequence.Get().quit) gu->globalQuit = true;
		}

		int LuaSide(lua_State* L) {
			const char* side = (active != nullptr) ? active->SideNow() : nullptr;
			if (side == nullptr) return 0;
			lua_pushstring(L, side);
			return 1;
		}

		void CaptureRule::PushLua(lua_State* L) {
			LuaPushRawNamedCFunc(L, "Side", LuaSide);
		}
	}
}

using CaptureRuleType = Capture::CaptureRule;
KT1_REGISTER_SYSTEM(Capture, 9900, CaptureRuleType)
}
