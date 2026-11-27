/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/* SupCom (perf, 2026-11-27): profiler-only split of the unit move-type update (perf_dbg). Serial sections only (main
 * thread): per-unit times are summed in a plain array and handed to the profiler once per system update (Flush), so a
 * section costs two clock reads per unit while the profiler is on, and one predictable branch while it is off (timing
 * runs). No sim state is read or written: same-seed identical with the profiler on or off. Nested sections are not
 * subtracted from their parent (the names say which is inside which). */
#ifndef SUPCOM_MOVE_TIMERS_H
#define SUPCOM_MOVE_TIMERS_H

#include "System/TimeProfiler.h"
#include "System/Misc/SpringTime.h"

namespace SupComMoveTimers {
	enum Id {
		// ground, serial pre-collision step (GroundMoveSystem ::2c)
		GPreMove, GPrePath, GPreSlope, GPreSpeed,
		// ground, serial update (::5::UpdateGround): CGroundMoveType::Update parts, then the SupCom parts
		GBaseSync, GBaseMove, GBaseKeepInside, GBaseWaterLine, GBaseOwnerMoved,
		GSeabed, GGapQueue, GYield, GMovedEvent,
		// general view (::6::UpdateGeneral), by kind, and the flying aircraft's parts
		AFlying, ALanded, AOther, Static, AFlight, AHeading, ACollisions, AMovedEvent,
		N
	};
	inline constexpr const char* NAMES[N] = {
		"Sim::Unit::MoveType::2c::PreCollisions::MoveSync",
		"Sim::Unit::MoveType::2c::PreCollisions::Path",
		"Sim::Unit::MoveType::2c::PreCollisions::SlopeSkidDrop",
		"Sim::Unit::MoveType::2c::PreCollisions::Speed",
		"Sim::Unit::MoveType::5g::Base::SyncWaypoints",
		"Sim::Unit::MoveType::5g::Base::PushMove",
		"Sim::Unit::MoveType::5g::Base::KeepInsideMap",
		"Sim::Unit::MoveType::5g::Base::WaterLine",
		"Sim::Unit::MoveType::5g::Base::OwnerMoved",
		"Sim::Unit::MoveType::5g::Seabed",
		"Sim::Unit::MoveType::5g::GapQueue",
		"Sim::Unit::MoveType::5g::Yield",
		"Sim::Unit::MoveType::5g::UnitMovedEvent",
		"Sim::Unit::MoveType::6g::AirFlying",
		"Sim::Unit::MoveType::6g::AirLanded",
		"Sim::Unit::MoveType::6g::OtherMobile",
		"Sim::Unit::MoveType::6g::Static",
		"Sim::Unit::MoveType::6g::AirFlying::Flight",
		"Sim::Unit::MoveType::6g::AirFlying::Heading",
		"Sim::Unit::MoveType::6g::AirFlying::Collisions",
		"Sim::Unit::MoveType::6g::UnitMovedEvent",
	};
	inline spring_time acc[N];
	inline bool on = false;

	inline void Begin() { on = CTimeProfiler::GetInstance().IsEnabled(); }

	struct Section {
		int id;
		spring_time t0;
		explicit Section(int i): id(i), t0(on ? spring_gettime() : spring_notime) {}
		~Section() { if (on) acc[id] += (spring_gettime() - t0); }
	};

	inline void Flush() {
		if (!on)
			return;
		static const bool registered = []() { for (const char* n: NAMES) CTimeProfiler::RegisterTimer(n); return true; }();
		(void) registered;
		const spring_time now = spring_gettime();
		for (int i = 0; i < N; ++i) {
			if (!acc[i].isDuration())
				continue;
			CTimeProfiler::GetInstance().AddTime(hashString(NAMES[i]), now - acc[i], acc[i]);
			acc[i] = spring_notime;
		}
	}
}

#endif
