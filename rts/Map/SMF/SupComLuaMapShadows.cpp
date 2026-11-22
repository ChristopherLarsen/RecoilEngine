/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComLuaMapShadows.h"

#include "Rendering/GL/myGL.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Shaders/Shader.h"

namespace SupComLuaMapShadows {
	static bool bound = false;

	void Enable(Shader::IProgramObject* prog)
	{
		bound = false;
		if (prog == nullptr || !shadowHandler.ShadowsLoaded())
			return;

		const GLuint id = prog->GetObjID();
		const GLint locTex = glGetUniformLocation(id, "shadowTex");
		const GLint locCol = glGetUniformLocation(id, "shadowColorTex");
		const GLint locMat = glGetUniformLocation(id, "shadowMat");
		if (locTex < 0 && locCol < 0 && locMat < 0)
			return;

		// the program is in use (EnableRaw ran just before)
		shadowHandler.SetupShadowTexSampler(GL_TEXTURE4, false);
		glActiveTexture(GL_TEXTURE19);
		glBindTexture(GL_TEXTURE_2D, shadowHandler.GetColorTextureID());
		glActiveTexture(GL_TEXTURE0);

		if (locTex >= 0) glUniform1i(locTex, 4);
		if (locCol >= 0) glUniform1i(locCol, 19);
		if (locMat >= 0) glUniformMatrix4fv(locMat, 1, GL_FALSE, shadowHandler.GetShadowMatrixRaw());
		bound = true;
	}

	void Disable(Shader::IProgramObject*)
	{
		if (!bound)
			return;
		bound = false;
		shadowHandler.ResetShadowTexSampler(GL_TEXTURE4, false);
		glActiveTexture(GL_TEXTURE19);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
	}
}
