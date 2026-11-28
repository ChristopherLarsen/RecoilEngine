/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the GPU half of the minimap's unit layer (see MinimapPainter.h). The vertex shader is icon_minimap's, with a
// contact's point in place of the unit's; the fragment shader is the world icon's with the atlas branch of icon_lex's.
// Premultiplied over the minimap, minimap pixels.

#include <cstddef>

#include "MinimapPainter.h"

#include "Game/KT1/Draw/StrategicIcons/IconShaderText.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "Rendering/Textures/NamedTextures.h"

namespace KT1::MinimapUnits {

namespace IL = Core::IconLexicon;

namespace {

constexpr const char* NAME = "MinimapUnits";

// @UNITS@ is the engine's per-unit uniform block
constexpr const char* VS = R"(#version 430
layout (location = 0) in vec2 quad;
layout (location = 1) in vec4 uvr;       // atlas x, y, w, h px; a contact: shape, tier bars, flags, scale
layout (location = 2) in vec4 col;
layout (location = 3) in vec4 par;       // mode, size px, turns with the heading (1), unit slot
layout (location = 4) in vec4 at;        // a contact's minimap px x, y and w = 1; w 0: the unit's drawn position
@UNITS@
uniform vec2 viewSize;
uniform vec2 mapSize;
uniform vec2 atlasSize;
out vec2 uv;
out vec4 vcol;
flat out float vfade;
flat out float vmode;
out vec2 lp;
flat out vec4 vshape;
flat out vec2 vbox;
void main() {
	vec2 corner = vec2(0.5 - 0.5 * quad.x, 0.5 + 0.5 * quad.y);
	vec2 c = at.xy;
	float a = 0.0;
	if (at.w < 0.5) {
		// drawPos.xyz the drawn position, drawPos.w the heading in radians, the front being (sin, 0, cos) of it
		vec4 dp = uni[uint(par.w + 0.5)].drawPos;
		vec3 p = dp.xyz;
		vec3 f = vec3(sin(dp.w), 0.0, cos(dp.w));
		c = vec2(p.x / mapSize.x * viewSize.x, (1.0 - p.z / mapSize.y) * viewSize.y);   // origin bottom-left
		if (par.z > 0.5) a = atan(-f.x, -f.z);            // world +z is down on the minimap
	}
	vec2 l = (corner - 0.5) * par.yy;
	lp = l; vshape = uvr; vbox = par.yy; vcol = col; vfade = 0.0; vmode = par.x;
	float cs = cos(a), sn = sin(a);
	vec2 q = c + vec2(cs * l.x - sn * l.y, sn * l.x + cs * l.y);
	gl_Position = vec4(q / viewSize * 2.0 - 1.0, 0.0, 1.0);
	uv = (uvr.xy + vec2(corner.x, 1.0 - corner.y) * uvr.zw) / atlasSize;
}
)";

// the silhouettes: R team-colour coverage, G white coverage
constexpr const char* DECL = R"(uniform sampler2D icons;
in vec2 uv;
flat in float vmode;
const vec3 INK = vec3(1.0);
const vec3 DARK = vec3(0.020, 0.035, 0.051);)";

constexpr const char* BRANCH = R"(	if (vmode < 2.5) {
		vec4 s = texture(icons, uv);
		if (vmode > 1.5) { frag = vec4(vcol.rgb * s.r, s.r) * o; return; }
		float f = s.r * vcol.a;
		vec3 line = vmode > 0.5 ? DARK : INK;        // enemy in relation mode: light fill, dark linework
		frag = vec4(vcol.rgb * f + line * s.g, f + s.g) * o; // premultiplied
		return;
	})";

} // namespace

bool MinimapPainter::Fail(const std::string& reason) {
	why = reason;
	Kill();
	return false;
}

bool MinimapPainter::Init(const IL::IconConfig& config) {
	Kill();
	const std::string units = Icons::UnitBlock();
	if (units.empty()) return Fail("no engine unit uniform block with drawPos");
	std::string vertex = VS;
	Icons::Replace(vertex, "@UNITS@", units);
	const std::string fragment = Icons::FragmentSource(config, DECL, BRANCH);
	shader = shaderHandler->CreateProgramObject("[KT1]", NAME);
	shader->AttachShaderObject(shaderHandler->CreateShaderObject(vertex, "", GL_VERTEX_SHADER));
	shader->AttachShaderObject(shaderHandler->CreateShaderObject(fragment, "", GL_FRAGMENT_SHADER));
	shader->Link();
	if (!shader->IsValid()) return Fail("shader: " + std::string(shader->GetLog()));
	InstanceBatch<IL::MinimapInstance>::Config c;
	c.name = "KT1MinimapUnits";
	c.idCapacity = config.view.max_instances;
	// DESIGN: the shader's locations 1..4, MinimapInstance's four vec4
	const std::vector<InstanceAttrib> attribs = {{1, 4, false, offsetof(IL::MinimapInstance, cell)},
	                                             {2, 4, false, offsetof(IL::MinimapInstance, colour)},
	                                             {3, 4, false, offsetof(IL::MinimapInstance, mode)},
	                                             {4, 4, false, offsetof(IL::MinimapInstance, at)}};
	if (!batch.Init(c, attribs)) return Fail("no instance buffer");
	why.clear();
	return true;
}

void MinimapPainter::Kill() {
	batch.Kill();
	if (shader != nullptr) shaderHandler->ReleaseProgramObject("[KT1]", NAME);
	shader = nullptr;
}

// KT1 hot
void MinimapPainter::Draw(const IL::MinimapState& state, const IL::IconConfig& config, const IL::MinimapView& view, bool check) {
	if (shader == nullptr) return;
	// the batch's ids are the places of the draw order, added in order and removed from the end only
	InstanceStore<IL::MinimapInstance>& store = batch.Instances();
	const int count = static_cast<int>(state.instances.size());
	while (store.Size() > count) store.Remove(store.Size() - 1);
	for (int place = 0; place < count; ++place) store.Set(place, state.instances[place]);
	using namespace GL::State;
	auto sub = GL::SubState(Blending(GL_TRUE), BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA), DepthTest(GL_FALSE),
	                        DepthMask(GL_FALSE), Culling(GL_FALSE));
	glActiveTexture(GL_TEXTURE0);
	CNamedTextures::Bind(config.view.icon_texture);
	shader->Enable();
	shader->SetUniform("icons", 0);
	shader->SetUniform("viewSize", view.w, view.h);
	shader->SetUniform("mapSize", view.mapX, view.mapZ);
	shader->SetUniform("atlasSize", config.atlas.icon_size[0], config.atlas.icon_size[1]);
	batch.Draw(check);
	shader->Disable();
	glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace KT1::MinimapUnits
