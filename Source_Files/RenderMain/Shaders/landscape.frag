R"(

uniform sampler2D texture0;
uniform float fogMix;
uniform float scalex;
uniform float scaley;
uniform float offsetx;
uniform float offsety;
uniform float yaw;
uniform float pitch;
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
const float zoom = 1.2;
const float pitch_adjust = 0.96;
void main(void) {
	vec3 facev = vec3(cos(yaw), sin(yaw), sin(pitch));

	if (vrMode <= 0.5) {
		// Desktop / non-VR: byte-for-byte the upstream flat-projection landscape sampling.
		// The VR spherical rewrite below MUST stay out of this path -- its atan reprojection
		// plus the out-of-range edge-clamp remapped V off the valid texture region and rendered
		// substitute (.dds) landscapes as solid black.
		vec3 relv = relDir;
		float x = relv.x / (relv.z * zoom) + atan(facev.x, facev.y);
		float y = relv.y / (relv.z * zoom) - (facev.z * pitch_adjust);
		vec4 color = texture2D(texture0, vec2(offsetx - x * scalex, offsety - y * scaley));
		vec3 intensity = mix(color.rgb, gl_Fog.color.rgb, fogMix);
		gl_FragColor = vec4(intensity, 1.0);
		return;
	}

	// VR: per-pixel spherical view direction; UV is position-independent.
	// bodyYawFromEye = kViewBase * zUpToYUp^T * mat3(vrView)^T, no yaw term. Snap turns and head
	// rotation are both already encoded in eyeDir (via the rendering matrix), so the atan(facev)
	// centering term handles yaw identically for both.
	vec2 ndc = (gl_FragCoord.xy - vec2(vpX, vpY)) / vec2(vpW, vpH) * 2.0 - 1.0;
	vec3 eyeDir = normalize(vec3(
		ndc.x * projXScale + projXOff,
		ndc.y * projYScale + projYOff,
		-1.0));
	vec3 relv = bodyYawFromEye * eyeDir;
	// The per-pixel ray-cast is already spherical, so the flat-projection zoom correction is
	// unnecessary and actively wrong: it would scale the head-turn contribution by 1/zoom while
	// leaving the snap-turn contribution (atan(facev)) unscaled. With zoom removed both inputs
	// scale identically and the landscape stays world-stationary for head motion and snap turns.
	float horizDist = length(relv.xz);
	float x = atan(-relv.x, -relv.z) + atan(facev.x, facev.y);
	float y = -atan(relv.y, max(horizDist, 0.0001)) - (facev.z * pitch_adjust);
	float v = offsety - y * scaley;
	// Cap the parts of the sky that have no image: for stock horizon-band landscapes that's above/below
	// the v in [0,1] band; for full-sphere .dds panoramas the horizon sits at v<0, so the panorama shows
	// in the wrap region and it's the v>=0 band that mirrors up high -- cap that instead.
	bool cap;
	float capEdge;
	if (landscapeSubstitute > 0.5) {
		cap = (v > 0.0);
		capEdge = 0.0;                  // the panorama's edge
	} else {
		cap = (v < 0.0 || v > 1.0);
		capEdge = clamp(v, 0.0, 1.0);   // nearest band edge (0 or 1)
	}
	if (cap) {
		// Average a few taps near the edge (stepping inward, plus a little azimuth spread) so stars and
		// other detail don't speckle the cap -- gives a clean representative sky-edge colour.
		float dir = (capEdge < 0.5) ? 1.0 : -1.0;
		vec3 capRGB = (
			texture2D(texture0, vec2(offsetx,        capEdge)).rgb +
			texture2D(texture0, vec2(offsetx,        capEdge + dir * 0.03)).rgb +
			texture2D(texture0, vec2(offsetx,        capEdge + dir * 0.06)).rgb +
			texture2D(texture0, vec2(offsetx + 0.15, capEdge + dir * 0.03)).rgb +
			texture2D(texture0, vec2(offsetx - 0.15, capEdge + dir * 0.03)).rgb
		) * 0.2;
		gl_FragColor = vec4(mix(capRGB, gl_Fog.color.rgb, fogMix), 1.0);
		return;
	}
	vec4 color = texture2D(texture0, vec2(offsetx - x * scalex, v));
	vec3 intensity = mix(color.rgb, gl_Fog.color.rgb, fogMix);
	gl_FragColor = vec4(intensity, 1.0);
}

)"
