/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_LUA_MAP_SHADOWS_H
#define SUPCOM_LUA_MAP_SHADOWS_H

namespace Shader { struct IProgramObject; }

// supcom: shadows for a Lua map shader (Spring.SetMapShader). The engine's own SMF shader gets the shadow depth texture
// with its compare mode on (CShadowHandler::SetupShadowTexSampler) only while it draws; a Lua program binding "$shadow"
// itself gets the texture with compare mode off, so its sampler2DShadow reads 0 everywhere (RND-001, 2026-11-22).
// With shadows on, Enable binds the depth texture (compare on) to unit 4 and the shadow colour texture to unit 19, and sets
// the program's uniforms shadowTex = 4, shadowColorTex = 19 and shadowMat (the drawing shadow matrix), each only if the
// program declares it. Render only; no synced state.
namespace SupComLuaMapShadows {
	void Enable(Shader::IProgramObject* prog);
	void Disable(Shader::IProgramObject* prog);
}

#endif
