/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the GPU half of the strategic icons (see IconPainter.h). Drawn as icon_lex drew them: premultiplied over the
// frame, no depth, view pixels. One instance per place of the draw order, so the buffer order is the draw order.

#include <algorithm>
#include <cstddef>
#include <cstdio>

#include "IconPainter.h"
#include "IconShader.h"

#include "System/creg/creg_cond.h"

#include "Game/Camera.h"
#include "Game/KT1/Core/IconLexicon/IconClass.h"
#include "Game/KT1/Draw/Common/FrameFacts.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/Models/ModelsMemStorageDefs.h"   // after creg_cond: it declares a creg struct
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"

namespace KT1::Icons {

namespace IL = Core::IconLexicon;

namespace {

constexpr const char* NAME = "Icons";
constexpr float HALF = 0.5f;   // DESIGN: a half size
constexpr int NUMBER = 32;     // DESIGN: room for one number's text

// a GLSL float literal
std::string Literal(float value) {
	char text[NUMBER];
	std::snprintf(text, sizeof(text), "%.6f", value);
	return text;
}

void Replace(std::string& source, const std::string& mark, const std::string& with) {
	for (size_t at = source.find(mark); at != std::string::npos; at = source.find(mark, at + with.size()))
		source.replace(at, mark.size(), with);
}

std::string Body(IL::Shape shape, const IL::IconConfig& config) {
	float body[2] = {0.0f, 0.0f};
	IL::BodyOf(shape, config, body);
	return "vec2(" + Literal(body[0]) + ", " + Literal(body[1]) + ")";
}

// the body size by shape number, the diamond last (icon_lex ShaderText)
std::string BodyChain(const IL::IconConfig& config) {
	std::string chain;
	for (const IL::Shape shape: {IL::Shape::Square, IL::Shape::Rect, IL::Shape::Circle, IL::Shape::Triangle})
		chain += "sh == " + std::to_string(static_cast<int>(shape)) + " ? " + Body(shape, config) + " : ";
	return chain + Body(IL::Shape::Diamond, config);
}

// the engine's per-unit uniform block as an array: its members become a struct (LuaUI/Include/gl4_units.lua); empty:
// the engine's definition is not the expected one
std::string UnitBlock() {
	const std::string& def = ModelUniformData::GetGLSLDefinition();
	const size_t buffer = def.find("buffer");
	const size_t open = buffer == std::string::npos ? buffer : def.find('{', buffer);
	const size_t close = def.rfind('}');
	if (open == std::string::npos || close == std::string::npos || close < open) return {};
	const std::string members = def.substr(open, close - open + 1);
	if (members.find("drawPos") == std::string::npos) return {};
	return "struct UnitUniforms " + members
	       + ";\nlayout (std140, binding = 1) readonly buffer UnitUniformsBuffer { UnitUniforms uni[]; };\n";
}

std::string FragmentSource(const IL::IconConfig& config) {
	const IL::IconStyleConfig& style = config.style;
	std::string source = ICON_FS;
	Replace(source, "@BODY@", BodyChain(config));
	Replace(source, "@RIM@", Literal(config.sizes.selected_rim_px));
	Replace(source, "@HOVER@", Literal(style.hover_lighten));
	Replace(source, "@MARK@", Literal(style.mark_radius));
	Replace(source, "@BARS@", std::to_string(style.bars));
	Replace(source, "@PITCH@", Literal(style.bar_pitch));
	Replace(source, "@BAR_HALF_W@", Literal(style.bar_size[0] * HALF));
	Replace(source, "@BAR_HALF_H@", Literal(style.bar_size[1] * HALF));
	return source;
}

} // namespace

bool IconPainter::Fail(const std::string& reason) {
	why = reason;
	Kill();
	return false;
}

bool IconPainter::Init(const IL::IconConfig& config) {
	Kill();
	const std::string units = UnitBlock();
	if (units.empty()) return Fail("no engine unit uniform block with drawPos");
	std::string vertex = ICON_VS;
	Replace(vertex, "@UNITS@", units);
	Replace(vertex, "@HALO@", Literal(config.style.halo_px));
	const std::string fragment = FragmentSource(config);
	shader = shaderHandler->CreateProgramObject("[KT1]", NAME);
	shader->AttachShaderObject(shaderHandler->CreateShaderObject(vertex, "", GL_VERTEX_SHADER));
	shader->AttachShaderObject(shaderHandler->CreateShaderObject(fragment, "", GL_FRAGMENT_SHADER));
	shader->Link();
	if (!shader->IsValid()) return Fail("shader: " + std::string(shader->GetLog()));
	InstanceBatch<IL::IconInstance>::Config c;
	c.name = "KT1Icons";
	c.idCapacity = config.view.max_instances;
	// DESIGN: the shader's locations 1..5, IconInstance's five vec4
	const std::vector<InstanceAttrib> attribs = {{1, 4, false, offsetof(IL::IconInstance, lift)},
	                                             {2, 4, false, offsetof(IL::IconInstance, shape)},
	                                             {3, 4, false, offsetof(IL::IconInstance, colour)},
	                                             {4, 4, false, offsetof(IL::IconInstance, world)},
	                                             {5, 4, false, offsetof(IL::IconInstance, angle)}};
	if (!batch.Init(c, attribs)) return Fail("no instance buffer");
	why.clear();
	return true;
}

void IconPainter::Kill() {
	batch.Kill();
	if (shader != nullptr) shaderHandler->ReleaseProgramObject("[KT1]", NAME);
	shader = nullptr;
}

// the batch's ids are the places of the draw order, added in order and removed from the end only: slot and place agree
// KT1 hot
void IconPainter::Sync(IL::IconState& state) {
	InstanceStore<IL::IconInstance>& store = batch.Instances();
	const int count = static_cast<int>(state.instances.size());
	while (store.Size() > count) store.Remove(store.Size() - 1);
	const int held = store.Size();
	for (const SlotRange& range: state.dirty) {
		const int end = std::min(range.first + range.count, held);
		for (int place = range.first; place < end; ++place) store.Set(place, state.instances[place]);
	}
	for (int place = held; place < count; ++place) store.Set(place, state.instances[place]);
	state.dirty.clear();
}

void IconPainter::Draw(const FrameFacts& facts, bool check) {
	if (shader == nullptr) return;
	const CameraFacts& cam = facts.Camera();
	using namespace GL::State;
	auto sub = GL::SubState(Blending(GL_TRUE), BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA), DepthTest(GL_FALSE),
	                        DepthMask(GL_FALSE), Culling(GL_FALSE));
	shader->Enable();
	shader->SetUniformMatrix4x4("camView", false, &CCamera::GetActive()->GetViewMatrix().m[0]);
	shader->SetUniformMatrix4x4("camProj", false, &CCamera::GetActive()->GetProjectionMatrix().m[0]);
	shader->SetUniform("viewSize", static_cast<float>(cam.viewW), static_cast<float>(cam.viewH));
	batch.Draw(check);
	shader->Disable();
}

} // namespace KT1::Icons
