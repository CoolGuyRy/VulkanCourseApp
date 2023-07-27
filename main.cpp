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

	while (!glfwWindowShouldClose(gWindow)) {
		double now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		glfwPollEvents();

		angle = angle + 30.0f * deltaTime;

		glm::mat4 firstModel(1.0f);
		glm::mat4 secondModel(1.0f);

		firstModel = glm::translate(firstModel, glm::vec3(-1.0f, 0.0f, -3.0f));
		firstModel = glm::rotate(firstModel, glm::radians((float)angle), glm::vec3(0.0, 0.0, 1.0));

		secondModel = glm::translate(secondModel, glm::vec3(1.0f, 0.0f, -3.0f));
		secondModel = glm::rotate(secondModel, glm::radians((float)-angle * 2.0f), glm::vec3(0.0, 0.0, 1.0));

		vulkanRenderer.updateModel(0, firstModel);
		vulkanRenderer.updateModel(1, secondModel);

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