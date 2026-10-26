/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "LuaPathFinder.h"
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "Sim/Path/IPathManager.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/MoveTypes/MoveMath/MoveMath.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "Map/ReadMap.h"
#include "System/SpringMath.h"

#include <algorithm>
#include <vector>


struct NodeCostOverlay {
public:
	NodeCostOverlay() { Clear(); }
	NodeCostOverlay(const NodeCostOverlay& o) = delete;
	NodeCostOverlay(NodeCostOverlay&& o) noexcept {
		costs = std::move(o.costs);
		sizex = o.sizex;
		sizez = o.sizez;
	}

	void Init(unsigned int sx, unsigned int sz) {
		costs.resize(sx * sz, 0.0f);
		sizex = sx;
		sizez = sz;
	}
	void Clear() {
		costs.clear();
		sizex = 0;
		sizez = 0;
	}

	bool Empty() const { return costs.empty(); }
	unsigned int Size() const { return costs.size(); }

public:
	std::vector<float> costs;

	unsigned int sizex;
	unsigned int sizez;
};

// [true] := synced, [false] := unsynced
static std::vector<NodeCostOverlay> costOverlays[2];

static void CreatePathMetatable(lua_State* L);


/******************************************************************************/
/******************************************************************************/

bool LuaPathFinder::PushEntries(lua_State* L)
{
	// safety in case of reload
	costOverlays[ true].clear();
	costOverlays[false].clear();
	costOverlays[ true].resize(4);
	costOverlays[false].resize(4);

	CreatePathMetatable(L);

	REGISTER_LUA_CFUNC(RequestPath);
	REGISTER_LUA_CFUNC(GetUnitReachablePoint);
	REGISTER_LUA_CFUNC(InitPathNodeCostsArray);
	REGISTER_LUA_CFUNC(FreePathNodeCostsArray);
	REGISTER_LUA_CFUNC(SetPathNodeCosts);
	REGISTER_LUA_CFUNC(GetPathNodeCosts);
	REGISTER_LUA_CFUNC(SetPathNodeCost);
	REGISTER_LUA_CFUNC(GetPathNodeCost);

	return true;
}

int LuaPathFinder::PushPathNodes(lua_State* L, const int pathID)
{
	if (pathID == 0)
		return 0;

	vector<float3> points;
	vector<int>    starts;

	pathManager->GetPathWayPoints(pathID, points, starts);

	const int pointCount = points.size();
	const int startCount = starts.size();

	{
		lua_createtable(L, pointCount, 0);

		for (int i = 0; i < pointCount; i++) {
			lua_createtable(L, 3, 0); {
				const float3& p = points[i];
				lua_pushnumber(L, p.x); lua_rawseti(L, -2, 1);
				lua_pushnumber(L, p.y); lua_rawseti(L, -2, 2);
				lua_pushnumber(L, p.z); lua_rawseti(L, -2, 3);
			}
			lua_rawseti(L, -2, i + 1);
		}
	}

	{
		lua_createtable(L, startCount, 0);

		for (int i = 0; i < startCount; i++) {
			lua_pushnumber(L, starts[i] + 1);
			lua_rawseti(L, -2, i + 1);
		}
	}

	return 2;
}


/******************************************************************************/

static int path_next(lua_State* L)
{
	const int* idPtr = (int*)luaL_checkudata(L, 1, "Path");
	const int pathID = *idPtr;

	if (pathID == 0)
		return 0;

	const int args = lua_gettop(L);

	float3 callerPos;
	if (args >= 4) {
		callerPos.x = luaL_checkfloat(L, 2);
		callerPos.y = luaL_checkfloat(L, 3);
		callerPos.z = luaL_checkfloat(L, 4);
	}

	const float minDist = luaL_optfloat(L, 5, 0.0f);

	const bool synced = CLuaHandle::GetHandleSynced(L);
	const float3 point = pathManager->NextWayPoint(nullptr, pathID, 0, callerPos, minDist, synced);

	if ((point.x == -1.0f) &&
	    (point.y == -1.0f) &&
	    (point.z == -1.0f)) {
		return 0;
	}

	lua_pushnumber(L, point.x);
	lua_pushnumber(L, point.y);
	lua_pushnumber(L, point.z);

	return 3;
}


static int path_nodes(lua_State* L)
{
	const int* idPtr = (int*)luaL_checkudata(L, 1, "Path");
	const int pathID = *idPtr;

	return (LuaPathFinder::PushPathNodes(L, pathID));
}

static int path_index(lua_State* L)
{
	const int* idPtr = (int*)luaL_checkudata(L, 1, "Path");
	const int pathID = *idPtr;

	if (pathID == 0)
		return 0;

	const std::string& key = luaL_checkstring(L, 2);
	if (key == "Next") {
		lua_pushcfunction(L, path_next);
		return 1;
	}
	if (key == "GetPathWayPoints") {
		lua_pushcfunction(L, path_nodes);
		return 1;
	}
	return 0;
}

static int path_newindex(lua_State* L)
{
	return 0;
}

static int path_gc(lua_State* L)
{
	int* idPtr = (int*)luaL_checkudata(L, 1, "Path");
	const int pathID = *idPtr;

	if (pathID == 0)
		return 0;

	pathManager->DeletePath(*idPtr);
	*idPtr = 0;
	return 0;
}


static void CreatePathMetatable(lua_State* L)
{
	luaL_newmetatable(L, "Path");
	LuaPushNamedCFunc(L, "__gc",       path_gc);
	LuaPushNamedCFunc(L, "__index",    path_index);
	LuaPushNamedCFunc(L, "__newindex", path_newindex);
	lua_pop(L, 1);
}


/***
 * supcom: whether a unit can reach a goal over the ground, and the reachable point nearest the goal
 *
 * A flood fill of the squares the unit's move class can stand on (terrain speed above zero, no structure under its
 * footprint), from the unit's square, stopping at the goal's square or after maxSquares squares. Moving units do not
 * block. For PTH-003 R1: an order to an unreachable goal ends at the reachable point nearest the goal.
 *
 * @function Spring.GetUnitReachablePoint
 * @param unitID integer
 * @param goalX number
 * @param goalZ number
 * @param maxSquares integer? (Default: 65536)
 * @return boolean? reachable true when the goal's square was reached; false when the fill ended without it; nil when the limit was hit first
 * @return number nearX centre of the visited square nearest the goal
 * @return number nearZ
 * @return integer visited squares visited
 */
int LuaPathFinder::GetUnitReachablePoint(lua_State* L)
{
	const CUnit* unit = unitHandler.GetUnit(luaL_checkint(L, 1));

	if (unit == nullptr || unit->moveDef == nullptr)
		return 0;

	const MoveDef& md = *unit->moveDef;
	const float gx = luaL_checkfloat(L, 2);
	const float gz = luaL_checkfloat(L, 3);
	const int maxSquares = std::max(1, luaL_optint(L, 4, 65536));

	const int W = mapDims.mapx;
	const int H = mapDims.mapy;
	const int gsx = std::clamp(int(gx / SQUARE_SIZE), 0, W - 1);
	const int gsz = std::clamp(int(gz / SQUARE_SIZE), 0, H - 1);
	const int ssx = std::clamp(int(unit->pos.x / SQUARE_SIZE), 0, W - 1);
	const int ssz = std::clamp(int(unit->pos.z / SQUARE_SIZE), 0, H - 1);

	// a square the unit can stand on: every square of its move class footprint passable, no structure there
	const auto standable = [&](int sx, int sz) {
		for (int z = sz - md.zsizeh; z < sz - md.zsizeh + md.zsize; z++) {
			for (int x = sx - md.xsizeh; x < sx - md.xsizeh + md.xsize; x++) {
				if (x < 0 || z < 0 || x >= W || z >= H)
					return false;
				if (CMoveMath::GetPosSpeedMod(md, x, z) <= 0.0f)
					return false;
			}
		}
		return ((CMoveMath::IsBlockedStructure(md, sx, sz, unit, 0) & CMoveMath::BLOCK_STRUCTURE) == 0);
	};

	std::vector<std::uint8_t> seen(size_t(W) * H, 0);
	std::vector<int> queue;
	queue.reserve(4096);
	queue.push_back(ssz * W + ssx);
	seen[ssz * W + ssx] = 1;

	int best = ssz * W + ssx;
	float bestSq = float((ssx - gsx) * (ssx - gsx) + (ssz - gsz) * (ssz - gsz));
	bool reached = (ssx == gsx && ssz == gsz);
	bool limited = false;

	for (size_t head = 0; head < queue.size() && !reached; head++) {
		if (int(head) >= maxSquares) {
			limited = true;
			break;
		}

		const int cx = queue[head] % W;
		const int cz = queue[head] / W;

		for (int dz = -1; dz <= 1 && !reached; dz++) {
			for (int dx = -1; dx <= 1; dx++) {
				if (dx == 0 && dz == 0)
					continue;

				const int nx = cx + dx;
				const int nz = cz + dz;

				if (nx < 0 || nz < 0 || nx >= W || nz >= H)
					continue;

				const int ni = nz * W + nx;

				if (seen[ni])
					continue;

				seen[ni] = 1;

				// diagonal steps only past two open orthogonal neighbours (no corner cutting)
				if (dx != 0 && dz != 0 && (!standable(cx + dx, cz) || !standable(cx, cz + dz)))
					continue;
				if (!standable(nx, nz))
					continue;

				queue.push_back(ni);

				const float dSq = float((nx - gsx) * (nx - gsx) + (nz - gsz) * (nz - gsz));

				if (dSq < bestSq) {
					bestSq = dSq;
					best = ni;
				}
				if (nx == gsx && nz == gsz) {
					reached = true;
					break;
				}
			}
		}
	}

	if (reached)
		lua_pushboolean(L, true);
	else if (limited)
		lua_pushnil(L);
	else
		lua_pushboolean(L, false);

	lua_pushnumber(L, ((best % W) + 0.5f) * SQUARE_SIZE);
	lua_pushnumber(L, ((best / W) + 0.5f) * SQUARE_SIZE);
	lua_pushnumber(L, int(queue.size()));
	return 4;
}


/******************************************************************************/
/******************************************************************************/

/***
 * @function Spring.RequestPath
 * @param moveID integer|string
 * @param startX number
 * @param startY number
 * @param startZ number
 * @param endX number
 * @param endY number
 * @param endZ number
 * @param radius number?
 * @return userdata? path
 */
int LuaPathFinder::RequestPath(lua_State* L)
{
	const MoveDef* moveDef = nullptr;

	if (lua_israwstring(L, 1)) {
		moveDef = moveDefHandler.GetMoveDefByName(lua_tostring(L, 1));
	} else {
		const unsigned int pathType = luaL_checkint(L, 1);

		if (pathType >= moveDefHandler.GetNumMoveDefs())
			luaL_error(L, "Invalid moveID passed to RequestPath");

		moveDef = moveDefHandler.GetMoveDefByPathType(pathType);
	}

	if (moveDef == nullptr)
		return 0;

	const float3 start(luaL_checkfloat(L, 2), luaL_checkfloat(L, 3), luaL_checkfloat(L, 4));
	const float3   end(luaL_checkfloat(L, 5), luaL_checkfloat(L, 6), luaL_checkfloat(L, 7));

	const float radius = luaL_optfloat(L, 8, 8.0f);

	const bool synced = CLuaHandle::GetHandleSynced(L);
	const int pathID = pathManager->RequestPath(nullptr, moveDef, start, end, radius, synced, true);

	if (pathID == 0)
		return 0;

	int* idPtr = (int*)lua_newuserdata(L, sizeof(int));
	luaL_getmetatable(L, "Path");
	lua_setmetatable(L, -2);

	*idPtr = pathID;
	return 1;
}



/***
 * @function Spring.InitPathNodeCostsArray
 * @param overlayIndex integer
 * @param sizeX integer
 * @param sizeZ integer
 * @return boolean success
 */
int LuaPathFinder::InitPathNodeCostsArray(lua_State* L)
{
	const unsigned int overlayIndex = luaL_checkint(L, 1);
	const unsigned int overlaySizeX = luaL_checkint(L, 2);
	const unsigned int overlaySizeZ = luaL_checkint(L, 3);
	const unsigned int syncedOverlay = CLuaHandle::GetHandleSynced(L);

	std::vector<NodeCostOverlay>& overlays = costOverlays[syncedOverlay];

	// disallow creating empty overlays
	if (overlaySizeX == 0 || overlaySizeZ == 0) {
		lua_pushboolean(L, false);
		return 1;
	}

	if (overlayIndex >= overlays.size())
		overlays.resize(std::max(overlays.size() * 2, size_t(overlayIndex)));

	NodeCostOverlay& overlay = overlays[overlayIndex];

	// disallow resizing existing overlays
	if (!overlay.Empty()) {
		lua_pushboolean(L, false);
		return 1;
	}

	overlay.Init(overlaySizeX, overlaySizeZ);

	lua_pushboolean(L, true);
	return 1;
}

/***
 * @function Spring.FreePathNodeCostsArray
 * @param overlayIndex integer
 * @return boolean success
 */
int LuaPathFinder::FreePathNodeCostsArray(lua_State* L)
{
	const unsigned int overlayIndex = luaL_checkint(L, 1);
	const unsigned int syncedOverlay = CLuaHandle::GetHandleSynced(L);

	std::vector<NodeCostOverlay>& overlays = costOverlays[syncedOverlay];

	// not an existing overlay
	if (overlayIndex >= overlays.size()) {
		lua_pushboolean(L, false);
		return 1;
	}

	NodeCostOverlay& overlay = overlays[overlayIndex];

	// not an initialized overlay, i.e. freed before
	if (overlay.Empty()) {
		lua_pushboolean(L, false);
		return 1;
	}

	// nullify the active cost-overlay if we are freeing it
	if (pathManager->GetNodeExtraCosts(syncedOverlay) == &overlay.costs[0])
		pathManager->SetNodeExtraCosts(nullptr, 1, 1, syncedOverlay);

	overlay.Clear();

	lua_pushboolean(L, true);
	return 1;
}



/***
 * @function Spring.SetPathNodeCosts
 * @param overlayIndex integer
 * @return boolean success
 */
int LuaPathFinder::SetPathNodeCosts(lua_State* L)
{
	const unsigned int overlayIndex = luaL_checkint(L, 1);
	const unsigned int syncedOverlay = CLuaHandle::GetHandleSynced(L);

	std::vector<NodeCostOverlay>& overlays = costOverlays[syncedOverlay];

	if (overlayIndex >= overlays.size()) {
		lua_pushboolean(L, false);
		return 1;
	}

	NodeCostOverlay& overlay = overlays[overlayIndex];

	if (overlay.Empty()) {
		lua_pushboolean(L, false);
		return 1;
	}

	// set the active cost-overlay to <overlay>
	lua_pushboolean(L, pathManager->SetNodeExtraCosts(&overlay.costs[0], overlay.sizex, overlay.sizez, syncedOverlay));
	return 1;
}

/***
 * @function Spring.GetPathNodeCosts
 * @param overlayIndex integer
 * @return boolean|table costs
 */
int LuaPathFinder::GetPathNodeCosts(lua_State* L)
{
	const unsigned int overlayIndex = luaL_checkint(L, 1);
	const unsigned int syncedOverlay = CLuaHandle::GetHandleSynced(L);

	std::vector<NodeCostOverlay>& overlays = costOverlays[syncedOverlay];

	if (overlayIndex >= overlays.size()) {
		lua_pushboolean(L, false);
		return 1;
	}

	NodeCostOverlay& overlay = overlays[overlayIndex];

	if (overlay.Empty()) {
		lua_pushboolean(L, false);
		return 1;
	}

	lua_createtable(L, overlay.Size(), 0);

	for (unsigned int n = 0; n < overlay.Size(); n++) {
		lua_pushnumber(L, overlay.costs[n]);
		lua_rawseti(L, -2, n + 1);
	}

	return 1;
}



/***
 * @function Spring.SetPathNodeCost
 * @param overlayIndex integer
 * @param costIndex integer 0-based index in the overlay
 * @param cost number
 * @return boolean success
 */
int LuaPathFinder::SetPathNodeCost(lua_State* L)
{
	const unsigned int overlayIndex = luaL_checkint(L, 1);
	const unsigned int costValIndex = luaL_checkint(L, 2);
	const unsigned int syncedOverlay = CLuaHandle::GetHandleSynced(L);

	std::vector<NodeCostOverlay>& overlays = costOverlays[syncedOverlay];

	if (overlayIndex >= overlays.size()) {
		lua_pushboolean(L, false);
		return 1;
	}

	NodeCostOverlay& overlay = overlays[overlayIndex];

	// non-initialized array
	if (overlay.Empty()) {
		lua_pushboolean(L, false);
		return 1;
	}

	// modify only the cost-overlay (whether it is active or not), not the PFS
	// pathManager->SetNodeExtraCost(hmx, hmz, luaL_checkfloat(L, 3), syncedOverlay);
	if (costValIndex < overlay.Size())
		overlay.costs[costValIndex] = luaL_checkfloat(L, 3);

	lua_pushboolean(L, (costValIndex < overlay.Size()));
	return 1;
}

/***
 * @function Spring.GetPathNodeCost
 * @param nodeX integer Heightmap node X coordinate
 * @param nodeZ integer Heightmap node Z coordinate
 * @return number cost
 */
int LuaPathFinder::GetPathNodeCost(lua_State* L)
{
	const unsigned int hmx = luaL_checkint(L, 1);
	const unsigned int hmz = luaL_checkint(L, 2);

	// reads from the active overlay if PathNodeStateBuffer::extraCosts != NULL
	lua_pushnumber(L, pathManager->GetNodeExtraCost(hmx, hmz, CLuaHandle::GetHandleSynced(L)));
	return 1;
}

/******************************************************************************/
/******************************************************************************/
