#pragma once

const int MAX_FRAMES_DRAWS = 2;

#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex Data Representation
struct Vertex {
	glm::vec3 pos; // Vertex Position (X, Y, Z)
	glm::vec3 col; // Vertex Color (R, G, B)
};

// Indices (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices {
	int graphicsFamily = -1;		// Location of Graphics Queue Family
	int presentationFamily = -1;	// Location of Presentation Queue Family

	// Check if queue families are valid
	bool isValid() {
		return graphicsFamily != -1 && presentationFamily != -1;
	}
};

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR surfaceCapabilities = {};		// Surface Properties, e.g. image size / extent
	std::vector<VkSurfaceFormatKHR>	formats;			// Surface Image Formats, e.g. rgba and size of each channel
	std::vector<VkPresentModeKHR> presentationModes;	// How Images should be presented to screen

	SwapChainDetails() {}
};

struct SwapchainImage {
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& fileName) {
	// Open stream from given file
	// std::ios::binary tells stream to read file as binary
	// std::ios::ate tells stream to start reading from end of file
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);
	
	// Check if file stream successfully opened
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open a file!");
	}

	// Get current read position and use it to resize file buffer
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	// Move read posisiton (seek to) the start of the file
	file.seekg(0);

	// read the file data into the buffer (stream "filesize" in total)
	file.read(fileBuffer.data(), fileSize);

	// close stream
	file.close();

	return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties) {
	// Get the properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if ((allowedTypes & (1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)) {						// Index of memory type must match corresponding bit in allowedTypes AND desired property bit flas are part of of memory type's property flags
			return i; // Valid Memory type, so return its index
		}
	}

	throw std::runtime_error("Failed to find an allowed type");

	return 0;	// bad code (0 is a possible return)
}


static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
	// INformation to create a buffer (doesnt include assigning memory)
	VkBufferCreateInfo bufferInfo = { };
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;		// Size of whole buffer (size of a vertex * number of vertices)
	bufferInfo.usage = bufferUsage;		// Multipe types of buffer possible, we want vertex buffer
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// Similar to swapchain images, can share vertex buffers

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Vertex Buffer!");
	}

	// Get Buffer Memory Requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

	// Allocate Memory to buffer
	VkMemoryAllocateInfo memoryAllocInfo = { };
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits, bufferProperties);

	// Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, bufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate Vertex Buffer Memory!");
	}

	// Allocate memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize) {
	// Command Buffer to hold transfer commands
	VkCommandBuffer transferCommandBuffer;
	
	// Command Buffer Details
	VkCommandBufferAllocateInfo allocInfo = { };
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = transferCommandPool;
	allocInfo.commandBufferCount = 1;

	// Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);

	// Information to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo = { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// We are ony using the command buffer once, so set it up accordingly

	VkResult result;

	// Begin recording transfer commands
	result = vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to begin Tranfer Command Buffer");
	}

		// Region of Data to copy from and to
		VkBufferCopy bufferCopyRegion = { };
		bufferCopyRegion.srcOffset = 0;			// from the beginning
		bufferCopyRegion.dstOffset = 0;			// also from the beginning
		bufferCopyRegion.size = bufferSize;

		// Command to copy src buffer to dst buffer
		vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

		// End recording transfer commands
	result = vkEndCommandBuffer(transferCommandBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to end Tranfer Command Buffer");
	}

	// Queue Submission Information
	VkSubmitInfo submitInfo = { };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;
	
	// Submit transfer command to transfer queue and wait until it finishes
	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);

	// Wait for it to finish
	vkQueueWaitIdle(transferQueue);


	// Free temporary command buffer back to pool
	vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}