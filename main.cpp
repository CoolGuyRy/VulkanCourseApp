#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include "VulkanRenderer.h"

GLFWwindow* gWindow;
VulkanRenderer vulkanRenderer;

void initWindow(std::string wName = "Vulkan Window", const int width = 800, const int height = 600) {
	// Initialize GLFW
	glfwInit();

	// Set GLFW to NOT work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// Do not allow resizing of the window
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// Create a window
	gWindow = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

double angle = 0.0;
double deltaTime = 0.0;
double lastTime = 0.0;

int main() {
	// Create a window
	initWindow("Vulkan Window", 800, 600);

	// Create Vulkan Renderer instance
	if (vulkanRenderer.init(gWindow) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	int modelLoc = vulkanRenderer.createMeshModel("Models/scene.gltf");

	while (!glfwWindowShouldClose(gWindow)) {
		double now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		glfwPollEvents();

		angle = angle + 30.0f * deltaTime;

		glm::mat4 testMat(1.0f);
		testMat = glm::rotate(testMat, glm::radians(180 + -(float)angle), glm::vec3(0.0, 1.0, 0.0)); 
		testMat = glm::rotate(testMat, glm::radians(15.0f * 9), glm::vec3(1.0, 0.0, 0.0));

		vulkanRenderer.updateModel(modelLoc, testMat);

		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanup();

	// Destroy the window
	glfwDestroyWindow(gWindow);
	
	// Terminate GLFW
	glfwTerminate();

	std::cout << "Enter a nice message to leave the program: "; std::cin.get();

	return 0;
}