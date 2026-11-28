/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the text of the icon shaders, shared by the world icons and the minimap layer (LUA-FREE D01, D02).

#ifndef KT1_DRAW_ICON_SHADER_TEXT_H
#define KT1_DRAW_ICON_SHADER_TEXT_H

#include <string>

#include "Game/KT1/Core/IconLexicon/IconConfig.h"

namespace KT1::Icons {

std::string Literal(float value);   // a GLSL float literal
void Replace(std::string& source, const std::string& mark, const std::string& with);   // every occurrence
// the engine's per-unit uniform block as an array `uni`; empty: the engine's definition is not the expected one
std::string UnitBlock();
// the icon fragment shader: the procedural icon of the config; `decl` and `branch` add a caller's own path before it
std::string FragmentSource(const Core::IconLexicon::IconConfig& config, const std::string& decl, const std::string& branch);

} // namespace KT1::Icons

#endif
