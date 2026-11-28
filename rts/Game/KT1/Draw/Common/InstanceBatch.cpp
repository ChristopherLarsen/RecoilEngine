/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the GL half of InstanceBatch (see InstanceBatch.h).

#include "InstanceBatch.h"

#include <cstring>

#include "Rendering/GL/StreamBuffer.h"
#include "Rendering/GL/VAO.h"
#include "Rendering/GL/VBO.h"
#include "Rendering/GL/myGL.h"

namespace KT1 {

namespace {
constexpr int QUAD_VERTICES = 6;   // DESIGN: two triangles
constexpr float QUAD[QUAD_VERTICES * 2] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};

// the instance attributes of one region, read from `base` bytes into the bound array buffer
void SetInstanceAttribs(const std::vector<InstanceAttrib>& attribs, size_t stride, size_t base) {
	for (const InstanceAttrib& a : attribs) {
		glEnableVertexAttribArray(a.location);
		glVertexAttribDivisor(a.location, 1);
		glVertexAttribPointer(a.location, a.components, a.bytes ? GL_UNSIGNED_BYTE : GL_FLOAT, a.bytes ? GL_TRUE : GL_FALSE,
		                      static_cast<GLsizei>(stride), reinterpret_cast<const void*>(base + a.offset));
	}
}
} // namespace

InstanceBuffer::InstanceBuffer() = default;
InstanceBuffer::~InstanceBuffer() { Kill(); }

bool InstanceBuffer::Init(const char* name, size_t instanceStride, int instanceCapacity, const std::vector<InstanceAttrib>& attribs) {
	Kill();
	if (instanceStride == 0 || instanceCapacity <= 0) return false;
	stride = instanceStride;
	capacity = instanceCapacity;

	const bool persistent = GLAD_GL_ARB_buffer_storage && GLAD_GL_ARB_sync;
	IStreamBufferConcept::StreamBufferCreationParams p;
	p.target = GL_ARRAY_BUFFER;
	p.numElems = static_cast<uint32_t>(stride * capacity);
	p.name = name;
	p.type = persistent ? IStreamBufferConcept::SB_PERSISTENTMAP : IStreamBufferConcept::SB_BUFFERSUBDATA;
	p.optimizeForStreaming = false;
	stream = IStreamBuffer<uint8_t>::CreateInstance(p);
	if (stream == nullptr || !stream->IsValid()) {
		Kill();
		return false;
	}
	regions = persistent ? static_cast<int>(IStreamBufferConcept::DEFAULT_NUM_BUFFERS) : 1;

	corners = std::make_unique<VBO>(GL_ARRAY_BUFFER, false);
	corners->Bind();
	corners->New(sizeof(QUAD), GL_STATIC_DRAW, QUAD);
	corners->Unbind();

	for (int r = 0; r < regions; r++) {
		vaos.push_back(std::make_unique<VAO>());
		vaos.back()->Bind();
		corners->Bind();
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		corners->Unbind();
		stream->Bind();
		SetInstanceAttribs(attribs, stride, static_cast<size_t>(r) * stream->GetByteSize());
		stream->Unbind();
		vaos.back()->Unbind();
		for (const InstanceAttrib& a : attribs) glDisableVertexAttribArray(a.location);
		glDisableVertexAttribArray(0);
	}
	return true;
}

void InstanceBuffer::Kill() {
	vaos.clear();
	corners.reset();
	stream.reset();
	regions = 1;
}

int InstanceBuffer::Region() const {
	if (stream == nullptr || regions == 1) return 0;
	return static_cast<int>(stream->BufferElemOffset() / (stride * capacity));
}

void InstanceBuffer::Upload(const void* data, const std::vector<SlotRange>& ranges) {
	const uint8_t* src = static_cast<const uint8_t*>(data);
	for (const SlotRange& r : ranges) {
		const size_t first = r.first * stride, bytes = r.count * stride;
		uint8_t* dst = stream->Map(nullptr, static_cast<uint32_t>(first), static_cast<uint32_t>(bytes));
		std::memcpy(dst, src + first, bytes);
		stream->Unmap();
	}
}

int InstanceBuffer::Differences(const void* data, int count) {
	const size_t bytes = count * stride;
	readBack.resize(bytes);
	stream->Bind();
	glGetBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(Region() * static_cast<size_t>(stream->GetByteSize())), bytes, readBack.data());
	stream->Unbind();
	int differing = 0;
	for (int i = 0; i < count; i++)
		differing += std::memcmp(readBack.data() + i * stride, static_cast<const uint8_t*>(data) + i * stride, stride) != 0;
	return differing;
}

void InstanceBuffer::Draw(int count) {
	VAO& vao = *vaos[Region()];
	vao.Bind();
	glDrawArraysInstanced(GL_TRIANGLES, 0, QUAD_VERTICES, count);
	vao.Unbind();
	stream->SwapBuffer();   // fences this region; nothing to do for the single region of the fallback
}

} // namespace KT1
