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
varying vec3 relDir;
varying vec4 vertexColor;
const float zoom = 1.2;
const float pitch_adjust = 0.96;
void main(void) {
	vec3 facev = vec3(cos(yaw), sin(yaw), sin(pitch));
	vec3 relv;
	if (vrMode > 0.5) {
		// Per-pixel view direction: UV is position-independent.
		// bodyYawFromEye = kViewBase * zUpToYUp^T * mat3(vrView)^T, no yaw term.
		// Snap turns and head rotation are both already encoded in eyeDir (via the
		// rendering matrix), so the atan(facev) centering term handles yaw identically
		// for both — no special-casing needed.
		vec2 ndc = (gl_FragCoord.xy - vec2(vpX, vpY)) / vec2(vpW, vpH) * 2.0 - 1.0;
		vec3 eyeDir = normalize(vec3(
			ndc.x * projXScale + projXOff,
			ndc.y * projYScale + projYOff,
			-1.0));
		relv = bodyYawFromEye * eyeDir;
	} else {
		relv = normalize(relDir);
	}
	float x = atan(-relv.x, -relv.z) / zoom + atan(facev.x, facev.y);
	float horizDist = length(relv.xz);
	float y = -atan(relv.y, max(horizDist, 0.0001)) / zoom - (facev.z * pitch_adjust);
	float v = offsety - y * scaley;
	if (v < 0.0 || v > 1.0) {
		// Sample the sky edge at the player's current forward azimuth (offsetx).
		// Gives a solid fill that matches the actual sky colour at that elevation.
		vec4 color = texture2D(texture0, vec2(offsetx, clamp(v, 0.0, 1.0)));
		gl_FragColor = vec4(mix(color.rgb, gl_Fog.color.rgb, fogMix), 1.0);
		return;
	}
	vec4 color = texture2D(texture0, vec2(offsetx - x * scalex, v));
	vec3 intensity = mix(color.rgb, gl_Fog.color.rgb, fogMix);
	gl_FragColor = vec4(intensity, 1.0);
}

)"
