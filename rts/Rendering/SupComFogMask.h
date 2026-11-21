/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_FOG_MASK_H
#define SUPCOM_FOG_MASK_H

#include "Rendering/GL/myGL.h"

struct lua_State;
namespace Shader { struct IProgramObject; }

// supcom: the fog of war look as a map-space mask the ground, ground decal and water shaders multiply by (drawing only).
// Unsynced Lua Spring.SupComSetFogMask(texName, x0, z0, x1, z1, factor): texName is a Lua texture (a signed distance in
// elmos to the edge of the union of the vision circles, positive inside, one float channel) covering the world rectangle
// x0,z0 to x1,z1; outside the circles the shaders multiply by factor with about one pixel of antialiasing on the edge.
// No texName (nil), or factor 1: off, the shaders skip it. Nothing here touches the simulation.
namespace SupComFogMask {
	/// the mask is set and on this frame
	bool Active();
	/// bind it on texture unit `unit` (leaves GL_TEXTURE0 active); false when off
	bool Bind(int unit);
	void Unbind(int unit);
	/// the uniforms fogMaskRect (x0, z0, 1/(x1-x0), 1/(z1-z0)) and fogMaskFactor of an enabled program (factor 1 unless the mask is bound)
	void SetUniforms(Shader::IProgramObject* sh, bool bound);

	void PushUnsyncedCtrl(lua_State* L);
}

#endif
