/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the GPU half of the minimap's unit layer (LUA-FREE D02, icon_minimap's gpu path): atlas silhouettes placed
// from the engine's per-unit uniforms and the radar contacts' shapes at their points, in one instanced draw in draw
// order. Private to this folder.

#ifndef KT1_DRAW_MINIMAP_PAINTER_H
#define KT1_DRAW_MINIMAP_PAINTER_H

#include <string>

#include "Game/KT1/Core/IconLexicon/IconMinimapState.h"
#include "Game/KT1/Draw/Common/InstanceBatch.h"

namespace Shader {
struct IProgramObject;
}

namespace KT1::MinimapUnits {

class MinimapPainter {
public:
	// once (GL context); false: Why() says what failed, and nothing is drawn
	bool Init(const Core::IconLexicon::IconConfig& config);
	void Kill();
	bool Valid() const { return shader != nullptr; }
	const std::string& Why() const { return why; }

	// copies the layer (what is unchanged is not uploaded) and draws it; `check`: also count the GPU's differences
	void Draw(const Core::IconLexicon::MinimapState& state, const Core::IconLexicon::IconConfig& config,
	          const Core::IconLexicon::MinimapView& view, bool check);
	const InstanceBatchStats& Stats() const { return batch.Stats(); }

private:
	bool Fail(const std::string& reason);

	InstanceBatch<Core::IconLexicon::MinimapInstance> batch;
	Shader::IProgramObject* shader = nullptr;
	std::string why;
};

} // namespace KT1::MinimapUnits

#endif
