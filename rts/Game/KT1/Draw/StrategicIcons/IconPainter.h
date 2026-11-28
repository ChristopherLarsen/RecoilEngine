/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the GPU half of the strategic icons (LUA-FREE D01, icon_lex's VS_GPU and its procedural fragment path): the
// set's instances in one buffer in draw order, drawn by one instanced call. Private to this folder.

#ifndef KT1_DRAW_ICON_PAINTER_H
#define KT1_DRAW_ICON_PAINTER_H

#include <string>

#include "Game/KT1/Core/IconLexicon/IconState.h"
#include "Game/KT1/Draw/Common/InstanceBatch.h"

namespace Shader {
struct IProgramObject;
}

namespace KT1 {
class FrameFacts;
}

namespace KT1::Icons {

class IconPainter {
public:
	// once (GL context); false: why() says what failed, and nothing is drawn
	bool Init(const Core::IconLexicon::IconConfig& config);
	void Kill();
	bool Valid() const { return shader != nullptr; }
	const std::string& Why() const { return why; }

	void Sync(Core::IconLexicon::IconState& state);   // copies what changed and consumes state.dirty
	void MarkAll() { batch.Instances().MarkAll(); }
	void Draw(const FrameFacts& facts, bool check);   // `check`: also count the GPU buffer's differences
	const InstanceBatchStats& Stats() const { return batch.Stats(); }

private:
	bool Fail(const std::string& reason);

	InstanceBatch<Core::IconLexicon::IconInstance> batch;
	Shader::IProgramObject* shader = nullptr;
	std::string why;
};

} // namespace KT1::Icons

#endif
