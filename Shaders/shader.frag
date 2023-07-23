#version 450

layout (location = 0) in vec3 fragCol;
layout (location = 0) out vec4 outColor;	// Final output color (must also have location)

void main(void) {
	outColor = vec4(fragCol, 1.0);
}