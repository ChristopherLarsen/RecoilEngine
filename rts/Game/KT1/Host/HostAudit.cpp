/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: the draw host's audit of itself (see HostAudit.h).

#include "HostAudit.h"

#ifndef HEADLESS
#include <array>
#include <iterator>

#include "Rendering/GL/myGL.h"
#endif

namespace KT1 {

namespace {

bool on = false;
HostAuditCounts idleCounts, busyCounts;

#ifndef HEADLESS
constexpr int VALUES = 4;    // DESIGN: the widest integer state compared (viewport, colour mask)
constexpr int MATRIX = 16;   // DESIGN: a 4x4 matrix

const GLenum INT_STATE[] = {
	GL_CURRENT_PROGRAM, GL_VERTEX_ARRAY_BINDING, GL_ARRAY_BUFFER_BINDING, GL_ELEMENT_ARRAY_BUFFER_BINDING,
	GL_DRAW_FRAMEBUFFER_BINDING, GL_ACTIVE_TEXTURE, GL_TEXTURE_BINDING_2D, GL_BLEND, GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB,
	GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_DEPTH_TEST, GL_DEPTH_WRITEMASK, GL_DEPTH_FUNC, GL_CULL_FACE,
	GL_CULL_FACE_MODE, GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_ALPHA_TEST, GL_COLOR_WRITEMASK, GL_VIEWPORT, GL_MATRIX_MODE,
	GL_MODELVIEW_STACK_DEPTH, GL_PROJECTION_STACK_DEPTH,
};
const GLenum MATRIX_STATE[] = {GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX};

struct State {
	std::array<GLint, std::size(INT_STATE) * VALUES> ints{};
	std::array<GLfloat, std::size(MATRIX_STATE) * MATRIX> matrices{};

	void Read() {
		for (size_t i = 0; i < std::size(INT_STATE); i++) glGetIntegerv(INT_STATE[i], &ints[i * VALUES]);
		for (size_t i = 0; i < std::size(MATRIX_STATE); i++) glGetFloatv(MATRIX_STATE[i], &matrices[i * MATRIX]);
	}
	bool operator==(const State& o) const { return ints == o.ints && matrices == o.matrices; }
};

State before;   // phase calls do not nest
GLuint query = 0;
#endif

} // namespace

void HostAudit::Set(bool enable) {
	on = enable;
	if (enable) idleCounts = busyCounts = HostAuditCounts{};
#ifndef HEADLESS
	if (!enable && query != 0) {
		glDeleteQueries(1, &query);
		query = 0;
	}
#endif
}

bool HostAudit::CanRead() {
#ifdef HEADLESS
	return false;
#else
	return true;
#endif
}

const HostAuditCounts& HostAudit::Counts(bool busy) { return busy ? busyCounts : idleCounts; }

HostAudit::Scope::Scope(bool busy) {
	if (!on) return;
	counts = busy ? &busyCounts : &idleCounts;
	counts->calls++;
#ifndef HEADLESS
	before.Read();
	GLint running = 0;
	glGetQueryiv(GL_PRIMITIVES_GENERATED, GL_CURRENT_QUERY, &running);
	if (running != 0) {
		counts->uncounted++;
		return;
	}
	if (query == 0) glGenQueries(1, &query);
	glBeginQuery(GL_PRIMITIVES_GENERATED, query);
	counting = true;
#endif
}

HostAudit::Scope::~Scope() {
#ifndef HEADLESS
	if (counts == nullptr) return;
	// the audit may be switched off inside the call: the query is then gone
	if (counting && query != 0) {
		glEndQuery(GL_PRIMITIVES_GENERATED);
		GLuint generated = 0;
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &generated);
		counts->primitives += generated;
	}
	State after;
	after.Read();
	if (!(after == before)) counts->stateChanges++;
#endif
}

} // namespace KT1
