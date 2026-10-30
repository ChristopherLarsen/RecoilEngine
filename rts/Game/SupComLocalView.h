/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_LOCAL_VIEW_H
#define SUPCOM_LOCAL_VIEW_H

// supcom: springsetting SupComLocalFullView (default 1; 0 is stock). A game started with fog of war off (the game's
// setup option, mod option `fog` set and not "1"/"true") lets every player's own client draw the whole map: the
// clients run with the spectators' full view (units, features, minimap, projectiles; LuaUI reads everything), while
// the synced LOS states, targeting and the simulation stay as they are. Spectators already run the simulation with
// full view on and stay in sync, so drawing with it cannot change the game. Paths that turn what the player sees into
// orders or messages (unit picking, default commands, the minimap's unit pick, decoy names, allied-only Lua
// messages) keep using real LOS through OrderFullView(), which is the stock value whenever the local full view is off.
namespace SupComLocalView {
	/// the local client is a player (not a spectator) of a game with fog of war off, and the setting is on
	bool Active();

	/// sets gu->spectatingFullView and LuaUI's read access for the local client (GlobalUnsynced::SetMyPlayer)
	void Apply();

	/// gu->spectatingFullView for the paths that make orders or messages: true only for a spectator's full view
	bool OrderFullView();
}

#endif
