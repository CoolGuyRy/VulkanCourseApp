#version 450

#extension GL_KHR_vulkan_glsl : enable

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inputColor; // Color output from Subpass 1
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth; // Depth output from Subpass 1

layout (location = 0) out vec4 color;

void main(void) {
	int xHalf = 1280 / 2;
	if (gl_FragCoord.x > xHalf) {
		float lowerBound = 0.998;
		float upperBound = 1.;

		float depth = subpassLoad(inputDepth).r;

		float depthColorScaled = 1.0f - ((depth - lowerBound) / (upperBound - lowerBound));
		color = vec4(subpassLoad(inputColor).rgb * depthColorScaled, 1.0f);
	} else {
		color = subpassLoad(inputColor);
	}
}