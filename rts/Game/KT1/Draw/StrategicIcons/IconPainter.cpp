/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the GPU half of the strategic icons (see IconPainter.h). Drawn as icon_lex drew them: premultiplied over the
// frame, no depth, view pixels. One instance per place of the draw order, so the buffer order is the draw order.

#include <algorithm>
#include <cstddef>
#include "IconPainter.h"
#include "IconShader.h"
#include "IconShaderText.h"

#include "Game/Camera.h"
#include "Game/KT1/Draw/Common/FrameFacts.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"

namespace KT1::Icons {

namespace IL = Core::IconLexicon;

namespace {

constexpr const char* NAME = "Icons";

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
	const std::string fragment = FragmentSource(config, "", "");
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
