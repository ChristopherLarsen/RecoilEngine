/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxFields: what the dev scenes ask of the shields, cloaks, blasts and depth effects (replaces GG.FxShields,
// GG.ShieldFxStats, GG.FxCloak, GG.CombatFxUnsynced and GG.DepthFx; EFFECTS 4, step D14). No native system calls into
// this one: it is fed by events and snapshots.
//
// Lua (unsynced), Spring.KT1.FxFields:
//   ShieldStats()      -> { drawn, draws, ripples, skipped, collapses, fades, breaker, cpu }
//   ShieldsDrawn()     -> n
//   ShieldCount()      -> n tracked
//   Shield(unitID)     -> { up, frac, kind, radius, faction, downKind, ripples } | nil
//   SetShieldsHidden(bool)                    draw nothing (the cost measure)
//   CloakStats()       -> { drawn, haze, outline, flashes, skipped, refract }
//   Cloak(unitID)      -> { cloaked, mode } | nil
//   BlastStats()       -> { blasts, smoke, debris, bubbles, skipped = { guard, switch, ["effects off"], far, view, cap } }
//   BlastState()       -> { effects, on, limiting, guard }
//   DepthStats()       -> { hits, launches, entries, intercepts, particles, dome, smokeUnder, skipped }

#ifndef KT1_FX_FIELDS_API_H
#define KT1_FX_FIELDS_API_H

struct lua_State;

namespace KT1::FxFields {

bool Active();

void PushFxFieldsBridge(lua_State* L);   // fills Spring.KT1.FxFields (Rule::PushLua)

} // namespace KT1::FxFields

#endif
