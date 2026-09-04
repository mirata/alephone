R"(

uniform sampler2D texture0;
uniform float fogMix;
uniform float scalex;
uniform float scaley;
uniform float offsetx;
uniform float offsety;
uniform float yaw;
uniform float pitch;
uniform float bloomScale;
uniform float vrMode;
uniform mat3 bodyYawFromEye;
uniform float projXScale;
uniform float projYScale;
uniform float projXOff;
uniform float projYOff;
uniform float vpX;
uniform float vpY;
uniform float vpW;
uniform float vpH;
uniform float landscapeSubstitute;
varying vec3 relDir;
varying vec4 vertexColor;
const float zoom = 1.205;
const float pitch_adjust = 0.955;

// Bloom contribution for a sampled sky colour. Kept identical to the upstream expression so the
// desktop look is unchanged.
vec4 landscapeBloom(vec3 rgb) {
	float intensity = clamp(bloomScale, 0.0, 1.0);
#ifdef GAMMA_CORRECTED_BLENDING
	//intensity = intensity * intensity;
	rgb = (rgb - 0.01) * 1.01;
#else
	rgb = (rgb - 0.1) * 1.11;
#endif
	return vec4(rgb * intensity * (1.0 - fogMix), 1.0);
}

void main(void) {
	vec3 facev = vec3(cos(yaw), sin(yaw), sin(pitch));

	if (vrMode <= 0.5) {
		// Desktop / non-VR: byte-for-byte the upstream flat-projection landscape sampling.
		vec3 relv  = normalize(relDir);
		float x = relv.x / (relv.z * zoom) + atan(facev.x, facev.y);
		float y = relv.y / (relv.z * zoom) - (facev.z * pitch_adjust);
		gl_FragColor = landscapeBloom(texture2D(texture0, vec2(offsetx - x * scalex, offsety - y * scaley)).rgb);
		return;
	}

	// VR: must reconstruct the view direction per-pixel exactly the way landscape.frag does, or the
	// glow lands somewhere other than the sky it is supposed to be blooming. relDir is useless here
	// (the VR landscape matrix is head-tracking-free by design), so ray-cast from gl_FragCoord.
	vec2 ndc = (gl_FragCoord.xy - vec2(vpX, vpY)) / vec2(vpW, vpH) * 2.0 - 1.0;
	vec3 eyeDir = normalize(vec3(
		ndc.x * projXScale + projXOff,
		ndc.y * projYScale + projYOff,
		-1.0));
	vec3 relv = bodyYawFromEye * eyeDir;
	// No zoom correction: the per-pixel cast is already spherical (see landscape.frag).
	float horizDist = length(relv.xz);
	float x = atan(-relv.x, -relv.z) + atan(facev.x, facev.y);
	float y = -atan(relv.y, max(horizDist, 0.0001)) - (facev.z * pitch_adjust);
	float v = offsety - y * scaley;
	// Same cap regions as landscape.frag, so the glow matches the image actually on screen.
	bool cap;
	float capEdge;
	if (landscapeSubstitute > 0.5) {
		cap = (v > 0.0);
		capEdge = 0.0;
	} else {
		cap = (v < 0.0 || v > 1.0);
		capEdge = clamp(v, 0.0, 1.0);
	}
	if (cap) {
		float dir = (capEdge < 0.5) ? 1.0 : -1.0;
		vec3 capRGB = (
			texture2D(texture0, vec2(offsetx,        capEdge)).rgb +
			texture2D(texture0, vec2(offsetx,        capEdge + dir * 0.03)).rgb +
			texture2D(texture0, vec2(offsetx,        capEdge + dir * 0.06)).rgb +
			texture2D(texture0, vec2(offsetx + 0.15, capEdge + dir * 0.03)).rgb +
			texture2D(texture0, vec2(offsetx - 0.15, capEdge + dir * 0.03)).rgb
		) * 0.2;
		gl_FragColor = landscapeBloom(capRGB);
		return;
	}
	gl_FragColor = landscapeBloom(texture2D(texture0, vec2(offsetx - x * scalex, v)).rgb);
}

)"
