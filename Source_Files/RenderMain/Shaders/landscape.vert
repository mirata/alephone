R"(

varying vec3 relDir;
varying vec4 vertexColor;
void main(void) {
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
#ifndef DISABLE_CLIP_VERTEX
	gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;
#endif
	// relDir drives the (desktop) flat-projection UV in landscape.frag; it must be the classic
	// modelview direction. The VR frag path ignores relDir (it ray-casts per pixel), so there is
	// no reason to substitute a head-tracking-free matrix here -- doing so broke desktop landscapes.
	relDir = (gl_ModelViewMatrix * gl_Vertex).xyz;
	vertexColor = gl_Color;
}

)"

