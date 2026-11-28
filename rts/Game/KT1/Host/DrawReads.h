/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: the values DrawView's reads give (DrawView.h). Plain values: no Sim/ include, no pointer to a live object.
// The string views point into unit and feature defs, which live for the whole game (a def's icon type: until it is set
// again, DrawWrites.h).
//
// Fog of war: "the viewer" is the local player's view as LuaUI reads it (a full view, else the local ally team). A value
// the viewer may not read keeps its default and, where it has one, its "has" flag is false: what the matching Spring.*
// call gives a widget.

#ifndef KT1_HOST_DRAWREADS_H
#define KT1_HOST_DRAWREADS_H

#include <cstdint>
#include <string_view>

#include "System/float3.h"

namespace KT1 {

// Spring.GetMyTeamID, GetMyAllyTeamID, GetSpectatingState. It can change mid-game: compare it each frame.
struct ViewerView {
	int team = 0;
	int allyTeam = 0;
	bool spectating = false;
	bool fullView = false;
	bool fullSelect = false;
};

// a unit the viewer sees: allied, in sight or on radar, or any with a full view
struct UnitStatus {
	int id = -1;
	int defId = 0;             // GetUnitDefID: the type as far as known (an enemy decoy: its decoy type); 0: unknown
	int team = 0;
	int allyTeam = 0;
	int uniformSlot = 0;       // its place in the engine's per-object uniform block (VBO:InstanceDataFromUnitIDs)
	// GetUnitLosState; a full view has all three
	bool readAsAlly = false;   // allied, or the view is full: everything below is readable
	bool inLos = false;
	bool inRadar = false;
	bool seenBefore = false;   // the previous-sight bit; given only while the type is still known (both "previous" bits)
	// GetUnitPosition: a unit not read as an ally carries the radar error
	float3 pos;
	float3 midPos;
	float3 drawPos;            // the engine's drawn position of this frame, with the same error
	// in sight (GetUnitHealth, GetUnitIsBeingBuilt, GetUnitDirection)
	bool hasFront = false;
	float3 front;
	bool hasBuild = false;
	float buildProgress = 0.0f;
	bool beingBuilt = false;
	bool hasHealth = false;    // false too for an enemy whose type hides its damage
	float health = 0.0f;
	float maxHealth = 0.0f;
	// read as an ally (GetUnitIsCloaked, GetUnitIsBuilding, GetUnitCommands)
	bool hasOrders = false;
	bool cloaked = false;
	bool building = false;     // a builder or factory with something under construction
	int buildTarget = -1;      // that unit
	int commandCount = 0;      // the queue's size (a factory: the orders for its new units)
	bool idle = false;         // finished, and the queue is empty (whether the type can move is the def's)
};

// the local draw state of a unit or feature
struct ObjectDrawState {
	uint8_t drawMask = 0xff;   // DESIGN: the engine's default, every pass (GetUnitEngineDrawMask)
	uint8_t drawFlag = 0;      // GetUnitDrawFlag
	bool noDraw = false;
	bool inVoid = false;       // out of the world (transported inside)
	bool icon = false;         // units: the engine draws the icon in place of the model (IsUnitIcon)
};

struct UnitDefView {
	int id = 0;
	std::string_view name;
	std::string_view modelName;
	std::string_view iconType;   // the engine icon type now (DrawWrite::SetUnitDefIcon changes it)
	int xsize = 0, zsize = 0;    // footprint, map squares
	bool canMove = false;
};

struct FeatureDefView {
	int id = 0;
	std::string_view name;
	std::string_view modelName;
	int xsize = 0, zsize = 0;
};

// a feature the viewer sees (IsInLos for the ally team under the game's feature visibility rule, or a full view)
struct FeatureView {
	int id = -1;
	int defId = 0;
	int team = -1;             // GetFeatureTeam: -1 for no ally team
	int allyTeam = -1;
	float3 pos;
	float3 midPos;
	float radius = 0.0f;
	float3 colVolScales;       // GetFeatureCollisionVolumeData's first three
};

} // namespace KT1

#endif
