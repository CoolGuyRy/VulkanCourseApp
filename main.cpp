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

float angle = 0.0f;
float deltaTime = 0.0f;
float lastTime = 0.0f;

int main() {
	// Create a window
	initWindow("Vulkan Window", 800, 600);

	// Create Vulkan Renderer instance
	if (vulkanRenderer.init(gWindow) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	while (!glfwWindowShouldClose(gWindow)) {
		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		glfwPollEvents();

		angle = angle + 30.0f * deltaTime;

		vulkanRenderer.updateModel(glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0, 0.0, 1.0)));
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