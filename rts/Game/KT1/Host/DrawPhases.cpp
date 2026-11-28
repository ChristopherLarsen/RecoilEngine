/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: calling the draw systems (see DrawPhases.h).

#include "DrawPhases.h"
#include "DisplaySink.h"

#include <deque>
#include <memory>
#include <string>

#include "Game/UI/MiniMap.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/GlobalRendering.h"
#include "System/EventClient.h"
#include "System/EventHandler.h"
#include "System/Log/ILog.h"
#include "System/StringHash.h"
#include "System/TimeProfiler.h"

namespace KT1 {

namespace {
constexpr int FIRST_ORDER = 10;          // DESIGN: before every Lua handle, so native drawing lies under script drawing
// KT1 lint: allow frame-literal a bit shift, not a frame count
constexpr int LAST_ORDER = 1 << 30;      // DESIGN: after every other client

void RunPhase(DrawPhase phase);

// the phases the engine's event handler already calls
class PhaseClient : public CEventClient {
public:
	PhaseClient() : CEventClient("[KT1DrawPhases]", FIRST_ORDER, false) {}
	bool WantsEvent(const std::string& e) override {
		return e == "Update" || e == "DrawGenesis" || e == "DrawScreenEffects" || e == "DrawScreen";
	}
	bool GetFullRead() const override { return true; }
	int GetReadAllyTeam() const override { return AllAccessTeam; }

	void Update() override { RunPhase(DrawPhase::Update); }
	void DrawGenesis() override { RunPhase(DrawPhase::Genesis); }
	void DrawScreenEffects() override { RunPhase(DrawPhase::ScreenEffects); }
	void DrawScreen() override { RunPhase(DrawPhase::Screen); }
};

class LastUpdateClient : public CEventClient {
public:
	LastUpdateClient() : CEventClient("[KT1DrawPhasesLast]", LAST_ORDER, false) {}
	bool WantsEvent(const std::string& e) override { return e == "Update"; }
	bool GetFullRead() const override { return true; }
	int GetReadAllyTeam() const override { return AllAccessTeam; }

	void Update() override { RunPhase(DrawPhase::PreDraw); }
};

struct HostState {
	PhaseList<DrawSystem> systems;
	DrawView view;
	FrameFacts facts;
	std::unique_ptr<PhaseClient> client;
	std::unique_ptr<LastUpdateClient> lastClient;
	std::deque<std::string> timers;   // never cleared: the profiler keeps the names
	uint32_t endedFrame = 0;
};

HostState& State() {
	static HostState state;
	return state;
}

const std::string TIMER_PREFIX = "KT1::";

// "KT1::<system>", registered with the profiler the first time; no allocation after that
const char* TimerName(const std::string& system) {
	HostState& s = State();
	for (const std::string& t : s.timers) {
		if (t.compare(TIMER_PREFIX.size(), std::string::npos, system) == 0) return t.c_str();
	}
	s.timers.push_back(TIMER_PREFIX + system);
	CTimeProfiler::RegisterTimer(s.timers.back().c_str());
	return s.timers.back().c_str();
}

void RefreshFacts() {
	HostState& s = State();
	s.view.BeginFrame(globalRendering->drawFrame);
	s.facts.Refresh(globalRendering->drawFrame, s.view);
}

// the switches take effect between frames: a system is in a whole frame or not at all
void SyncSwitches() {
	HostState& s = State();
	for (const auto& e : s.systems.Entries()) {
		const bool on = e.system->Active();
		if (on == e.enabled) continue;
		if (on) e.system->Resumed();
		s.systems.SetEnabled(e.name, on);
	}
}

void AddClients() {
	HostState& s = State();
	if (s.client != nullptr) return;
	s.client = std::make_unique<PhaseClient>();
	s.lastClient = std::make_unique<LastUpdateClient>();
	eventHandler.AddClient(s.client.get());
	eventHandler.AddClient(s.lastClient.get());
}

void CallSystems(DrawContext& ctx) {
	State().systems.ForPhase(ctx.phase, [&ctx](const PhaseList<DrawSystem>::Entry& e) {
		ScopedTimer timer(hashString(TimerName(e.name.c_str())));
		e.system->Draw(ctx);
	});
}

// Lua's DrawInMiniMap matrices (LuaOpenGL::ResetMiniMapMatrices), for the native systems of the phase
void CallMiniMapSystems(DrawContext& ctx) {
	ctx.miniMapW = minimap->GetSizeX();
	ctx.miniMapH = minimap->GetSizeY();
	glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
	glOrtho(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f);
	minimap->ApplyConstraintsMatrix();
	glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
	glScalef(1.0f / ctx.miniMapW, 1.0f / ctx.miniMapH, 1.0f);
	CallSystems(ctx);
	glMatrixMode(GL_PROJECTION); glPopMatrix();
	glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

void RunPhase(DrawPhase phase) {
	HostState& s = State();
	s.view.BeginFrame(globalRendering->drawFrame);
	if (phase == DrawPhase::Update) DisplaySink::Replay();   // the steps since the last frame, before any system
	if (phase == DrawPhase::PreDraw) SyncSwitches();
	// Genesis is where the frame's camera and positions are settled: the facts are made there for every later phase
	if (phase == DrawPhase::Genesis && s.systems.AnyEnabled()) RefreshFacts();
	if (!s.systems.Wants(phase)) return;
	DrawContext ctx{phase, s.view, s.facts};
	if (phase == DrawPhase::MiniMap) {
		if (minimap != nullptr) CallMiniMapSystems(ctx);
		return;
	}
	CallSystems(ctx);
}
} // namespace

void DrawPhases::Init() {
	if (DisplaySink::Present()) AddClients();   // the sim's display events are played with or without a draw system
}

void DrawPhases::Add(DrawSystem* system) {
	HostState& s = State();
	if (system == nullptr || !s.systems.Add(system, system->Name(), system->Order(), system->Phases())) return;
	TimerName(system->Name());
	AddClients();
	LOG("[KT1] draw system %s added", system->Name());
}

void DrawPhases::Run(DrawPhase phase) {
	if (State().client == nullptr) return;   // no draw system and no display events in this game
	RunPhase(phase);
}

void DrawPhases::WorldAlpha(bool aboveWater) {
	if (aboveWater) Run(DrawPhase::WorldAlpha);
}

void DrawPhases::FrameEnd() {
	HostState& s = State();
	// a swap that draws no new game frame (a load screen, a script's own swap) does not end one twice
	if (s.client == nullptr || s.endedFrame == globalRendering->drawFrame) return;
	s.endedFrame = globalRendering->drawFrame;
	RunPhase(DrawPhase::FrameEnd);
}

void DrawPhases::Kill() {
	HostState& s = State();
	for (const auto& e : s.systems.Entries()) e.system->Kill();
	s.systems.Clear();
	s.client.reset();       // ~CEventClient removes it from the event handler
	s.lastClient.reset();
	s.facts.Reset();
	s.view.Reset();
	s.endedFrame = 0;
	SnapshotBoard::Get().Clear();
}

const FrameFacts& DrawPhases::Facts() {
	// with an active system, DrawGenesis fills them (an earlier fill in the frame would hold the old camera)
	if (State().client == nullptr || !State().systems.AnyEnabled()) RefreshFacts();
	return State().facts;
}

void DrawPhases::SetPointer(int x, int y) { State().facts.SetPointer(x, y); }
void DrawPhases::ClearPointer() { State().facts.ClearPointer(); }

} // namespace KT1
