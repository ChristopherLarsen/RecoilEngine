/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComFogMask.h"

#include "Lua/LuaOpenGLUtils.h"
#include "Lua/LuaUtils.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/Shaders/Shader.h"

#include <algorithm>

namespace {
	LuaMatTexture maskTex;
	bool have = false;
	float rx0 = 0.0f, rz0 = 0.0f, rx1 = 1.0f, rz1 = 1.0f;
	float factor = 1.0f;

	GLuint Id() { return have ? maskTex.GetTextureID() : 0; }

	int SupComSetFogMask(lua_State* L)
	{
		if (lua_isnoneornil(L, 1)) {
			have = false;
			factor = 1.0f;
			return 0;
		}
		LuaMatTexture tex;
		if (!LuaOpenGLUtils::ParseTextureImage(L, tex, luaL_checkstring(L, 1)))
			luaL_error(L, "SupComSetFogMask: unknown texture %s", lua_tostring(L, 1));
		maskTex = tex;
		rx0 = luaL_checkfloat(L, 2);
		rz0 = luaL_checkfloat(L, 3);
		rx1 = luaL_checkfloat(L, 4);
		rz1 = luaL_checkfloat(L, 5);
		factor = std::min(1.0f, std::max(0.0f, luaL_checkfloat(L, 6)));
		have = true;
		return 0;
	}
}

bool SupComFogMask::Active() { return have && factor < 1.0f && Id() != 0; }

bool SupComFogMask::Bind(int unit)
{
	if (!Active())
		return false;
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, Id());
	glActiveTexture(GL_TEXTURE0);
	return true;
}

void SupComFogMask::Unbind(int unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
}

void SupComFogMask::SetUniforms(Shader::IProgramObject* sh, bool on)
{
	sh->SetUniform("fogMaskFactor", on ? factor : 1.0f);
	if (on)
		sh->SetUniform("fogMaskRect", rx0, rz0, 1.0f / (rx1 - rx0), 1.0f / (rz1 - rz0));
}

void SupComFogMask::PushUnsyncedCtrl(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "SupComSetFogMask", SupComSetFogMask);
}
