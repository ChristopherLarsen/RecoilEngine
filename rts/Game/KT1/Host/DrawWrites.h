/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: the only writes draw code makes into the engine's objects: this client's own draw state, what the unsynced
// Lua calls named below write. Nothing here is read by the simulation, saved or synced. No Sim/ include.
//
// An id is not restricted to what the viewer sees or controls (as SetUnitEngineDrawMask, SetFeatureEngineDrawMask and
// gl.SetUnitBufferUniforms; the other Lua calls are for controlled teams only): a write tells the caller nothing but
// that the id is alive, so never show that answer to the player. A false or 0 answer: no such object.
// Draw masks belong to whoever writes them: there is one per object, so systems that share it restore with care.

#ifndef KT1_HOST_DRAWWRITES_H
#define KT1_HOST_DRAWWRITES_H

#include <cstdint>
#include <span>
#include <string>

namespace KT1::DrawWrite {

bool SetUnitNoDraw(int unitId, bool noDraw);                  // Spring.SetUnitNoDraw
bool SetUnitDrawMask(int unitId, uint8_t mask);               // Spring.SetUnitEngineDrawMask
bool SetUnitAlwaysUpdateMatrix(int unitId, bool on);          // Spring.SetUnitAlwaysUpdateMatrix
bool SetUnitNoMinimap(int unitId, bool noMinimap);            // Spring.SetUnitNoMinimap
bool SetUnitIconDraw(int unitId, bool draw);                  // Spring.SetUnitIconDraw
// gl.SetUnitBufferUniforms: values into the user slots from `offset`, cut at the last slot; returns how many were
// written. The slots are shared: see Draw/UnitMaterial/UnitMaterialValues.h.
int SetUnitUniforms(int unitId, int offset, std::span<const float> values);

bool SetFeatureNoDraw(int featureId, bool noDraw);            // Spring.SetFeatureNoDraw
bool SetFeatureDrawMask(int featureId, uint8_t mask);         // Spring.SetFeatureEngineDrawMask (0 leaves the drawer's lists)
bool SetFeatureAlwaysUpdateMatrix(int featureId, bool on);    // Spring.SetFeatureAlwaysUpdateMatrix
bool SetFeatureFade(int featureId, bool allow);               // Spring.SetFeatureFade
int SetFeatureUniforms(int featureId, int offset, std::span<const float> values);   // gl.SetFeatureBufferUniforms

// engine unit icon types
bool IconTypeExists(const std::string& name);
// Spring.AddUnitIcon (replaces a type of the same name); false: the texture file is missing (the type is added anyway)
bool AddIconType(const std::string& name, const std::string& texture, float size, float distance, bool radiusAdjust);
// Spring.SetUnitDefIcon: the def, its decoys, and every unit of it; false: no such def or icon type. Keep the old name
// (DrawView::UnitDefInfo) to give it back.
bool SetUnitDefIcon(int defId, const std::string& iconType);
// Spring.SetUnitIcon: one unit's own type; empty: back to its def's. false: no such unit or icon type.
bool SetUnitIcon(int unitId, const std::string& iconType);

} // namespace KT1::DrawWrite

#endif
