/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the strategic icon shader (LUA-FREE D01): icon_lex's VS_GPU and the procedural branch of its FS, line for line,
// so the pixels are the same. The words between @ signs are the config's numbers, written in as literals. The vertex
// shader turns the batch's corner (-1..1) into the corner and the diagonal of the Lua's triangle strip. Private to this
// folder.

#ifndef KT1_DRAW_ICON_SHADER_H
#define KT1_DRAW_ICON_SHADER_H

namespace KT1::Icons {

// @UNITS@ is the engine's per-unit uniform block
constexpr const char* ICON_VS = R"(#version 430
layout (location = 0) in vec2 quad;
layout (location = 1) in vec4 rect;      // lift (px the box sits above the unit), unit slot, w, h
layout (location = 2) in vec4 uvr;       // shape, tier bars, flags, scale
layout (location = 3) in vec4 col;
layout (location = 4) in vec4 wpos;      // a radar contact's world position and w = 1; 0: the unit's drawn position
layout (location = 5) in vec4 par;       // angle, fade
@UNITS@
uniform mat4 camView;
uniform mat4 camProj;
uniform vec2 viewSize;
out vec4 vcol;
flat out float vfade;
out vec2 lp;
flat out vec4 vshape;
flat out vec2 vbox;
const float HALO = @HALO@;
void main() {
	vec2 corner = vec2(0.5 - 0.5 * quad.x, 0.5 + 0.5 * quad.y);
	float pad = ((int(uvr.z + 0.5) & 1) != 0) ? HALO + 1.0 : 0.0;   // selected: room for the halo (screen px)
	vec2 l = (corner - 0.5) * (rect.zw + 2.0 * pad);
	lp = l; vshape = uvr; vbox = rect.zw; vcol = col; vfade = par.y;
	vec4 cp = camProj * camView * vec4(wpos.w > 0.5 ? wpos.xyz : uni[uint(rect.y + 0.5)].drawPos.xyz, 1.0);
	if (cp.w <= 0.0 || rect.z <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }   // behind the camera, or hidden
	vec2 c = (cp.xy / cp.w * 0.5 + 0.5) * viewSize - vec2(0.0, rect.x);
	float cs = cos(par.x), sn = sin(par.x);
	vec2 p = c + vec2(cs * l.x - sn * l.y, sn * l.x + cs * l.y);
	gl_Position = vec4(p / viewSize * 2.0 - 1.0, 0.0, 1.0);
}
)";

// @BODY@ is the chain of body sizes by shape number
constexpr const char* ICON_FS = R"(#version 430
in vec4 vcol;
flat in float vfade;
in vec2 lp;
flat in vec4 vshape;
flat in vec2 vbox;
out vec4 frag;
// signed distance (px, < 0 inside) of the body shape about its centre
float body(vec2 p, int sh, vec2 b) {
	vec2 h = b * 0.5;
	if (sh == 0) return (abs(p.x) / h.x + abs(p.y) / h.y - 1.0) * min(h.x, h.y) * 0.7071;   // diamond
	if (sh == 3) return length(p) - h.x;                                                    // circle
	if (sh == 4) {                                                                           // triangle, point up
		vec2 n = normalize(vec2(2.0 * h.y, h.x));
		return max(-h.y - p.y, dot(vec2(abs(p.x), p.y - h.y), n));
	}
	vec2 d = abs(p) - h; return max(d.x, d.y);                                               // square, wide rectangle
}
void main() {
	float o = 1.0 - vfade;                           // premultiplied: scale all four
	float sc = vshape.w;
	int sh = int(vshape.x + 0.5);
	int tier = int(vshape.y + 0.5);
	int fl = int(vshape.z + 0.5);
	vec2 bsz = @BODY@;
	bsz *= sc;
	float rim = @RIM@ * sc;
	vec2 c = vec2(0.0, vbox.y * 0.5 - rim - bsz.y * 0.5);       // body centre in box px
	vec2 p = lp - c;
	float d = body(p, sh, bsz);
	vec3 rgb = vcol.rgb;
	if ((fl & 2) != 0) rgb = mix(rgb, vec3(1.0), @HOVER@);        // hovered: lighter body
	float aIn = clamp(0.5 - (d + rim), 0.0, 1.0);                 // inside the 1 px outline
	float aShape = clamp(0.5 - d, 0.0, 1.0);                      // the shape with its outline
	vec4 col = vec4(0.0);
	col = mix(col, vec4(0.0, 0.0, 0.0, 1.0), aShape);             // the black outline (and under the fill)
	col = mix(col, vec4(rgb * vcol.a, vcol.a), aIn);              // the team colour body (faded when cloaked)
	if ((fl & 4) != 0) {                                          // the role mark: a small black dot
		float m = clamp(0.5 - (length(p) - @MARK@ * sc), 0.0, 1.0);
		col = mix(col, vec4(0.0, 0.0, 0.0, 1.0), m * aIn);
	}
	if ((fl & 1) != 0) {                                          // selected: a hard white rim just outside the outline
		float dpx = d / max(length(vec2(dFdx(d), dFdy(d))), 1e-4);   // distance outside the shape in screen px
		float r = (dpx > 0.0 && dpx <= rim) ? 1.0 : 0.0;        // hard band, no blur
		r *= 1.0 - aShape;
		col = col + vec4(1.0) * r;
	}
	float top = c.y - bsz.y * 0.5;                                // the bars: below the body's outline
	for (int i = 0; i < @BARS@; i++) {
		if (i >= tier) break;
		float yc = top - (@PITCH@ * float(i) + 0.5) * sc;
		vec2 q = abs(lp - vec2(0.0, yc)) - vec2(@BAR_HALF_W@ * sc, @BAR_HALF_H@ * sc);
		float a = clamp(0.5 - max(q.x, q.y), 0.0, 1.0);
		col = mix(col, vec4(0.0, 0.0, 0.0, 1.0), a);
	}
	if (col.a < 0.02) discard;                    // nothing outside the shape, whatever the blend state
	frag = vec4(col.rgb, col.a) * o;
}
)";

} // namespace KT1::Icons

#endif
