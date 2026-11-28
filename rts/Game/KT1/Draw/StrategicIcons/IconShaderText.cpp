/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the icon shaders' text (see IconShaderText.h).

#include <cstdio>

#include "IconShader.h"
#include "IconShaderText.h"

#include "System/creg/creg_cond.h"

#include "Game/KT1/Core/IconLexicon/IconClass.h"
#include "Rendering/Models/ModelsMemStorageDefs.h"   // after creg_cond: it declares a creg struct

namespace KT1::Icons {

namespace IL = Core::IconLexicon;

namespace {

constexpr float HALF = 0.5f;   // DESIGN: a half size
constexpr int NUMBER = 32;     // DESIGN: room for one number's text

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

} // namespace

std::string Literal(float value) {
	char text[NUMBER];
	std::snprintf(text, sizeof(text), "%.6f", value);
	return text;
}

void Replace(std::string& source, const std::string& mark, const std::string& with) {
	for (size_t at = source.find(mark); at != std::string::npos; at = source.find(mark, at + with.size()))
		source.replace(at, mark.size(), with);
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

std::string FragmentSource(const IL::IconConfig& config, const std::string& decl, const std::string& branch) {
	const IL::IconStyleConfig& style = config.style;
	std::string source = ICON_FS;
	Replace(source, "@DECL@", decl);
	Replace(source, "@BRANCH@", branch);
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

} // namespace KT1::Icons
