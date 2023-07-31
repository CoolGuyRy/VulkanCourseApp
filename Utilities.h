#pragma once

const int MAX_FRAMES_DRAWS = 2;
const int MAX_OBJECTS = 20;

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
	glm::vec2 tex; // Texture UV Coords (U, V)
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
	std::vector<VkSurfaceFormatKHR>	formats;				// Surface Image Formats, e.g. rgba and size of each channel
	std::vector<VkPresentModeKHR> presentationModes;		// How Images should be presented to screen

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

static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool) {
	// Command Buffer to hold transfer commands
	VkCommandBuffer commandBuffer;

	// Command Buffer Details
	VkCommandBufferAllocateInfo allocInfo = { };
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	// Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	// Information to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo = { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// We are ony using the command buffer once, so set it up accordingly

	VkResult result;

	// Begin recording transfer commands
	result = vkBeginCommandBuffer(commandBuffer, &beginInfo);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to begin Tranfer Command Buffer");
	}

	return commandBuffer;
}

static void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer) {
	// End recording transfer commands
	VkResult result;
	result = vkEndCommandBuffer(commandBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to end Tranfer Command Buffer");
	}

	// Queue Submission Information
	VkSubmitInfo submitInfo = { };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Submit transfer command to transfer queue and wait until it finishes
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

	// Wait for it to finish
	vkQueueWaitIdle(queue);


	// Free temporary command buffer back to pool
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize) {
		// Create Buffer
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	// Region of Data to copy from and to
	VkBufferCopy bufferCopyRegion = { };
	bufferCopyRegion.srcOffset = 0;			// from the beginning
	bufferCopyRegion.dstOffset = 0;			// also from the beginning
	bufferCopyRegion.size = bufferSize;

	// Command to copy src buffer to dst buffer
	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height) {
	// Create Buffer
	VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

	VkBufferImageCopy imageRegion = { };
	imageRegion.bufferOffset = 0;											// Offset into data
	imageRegion.bufferRowLength = 0;										// Row length of data to calculate data spacing
	imageRegion.bufferImageHeight = 0;										// Image height to calculate data spacing
	imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageRegion.imageSubresource.mipLevel = 0;								// Mipmap level to copy
	imageRegion.imageSubresource.baseArrayLayer = 0;						// Starting array layer (if array)
	imageRegion.imageSubresource.layerCount = 1;							// Number of layers to copy starting at baseArrayLayer
	imageRegion.imageOffset = { 0, 0, 0 };									// Offset into image (as opposed to raw data in buffer offset)
	imageRegion.imageExtent = { width, height, 1 };							// Size of region to copy as (x, y, z) values

	// Copy buffer to given image
	vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

	endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

	VkImageMemoryBarrier imageMemoryBarrier = { };
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = oldLayout;									// Layout to transition from
	imageMemoryBarrier.newLayout = newLayout;									// Layout to transition to
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;			// Queue Family to transition from
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;			// Queue Family to transition to
	imageMemoryBarrier.image = image;											// Image being accessed and modified as part of barrier
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// aspect of image being altered
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;						// first mip level to start alterations on
	imageMemoryBarrier.subresourceRange.levelCount = 1;							// number of mip levels to alter starting from baseMipLevel
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;						// first layer to start alterations on
	imageMemoryBarrier.subresourceRange.layerCount = 1;							// Number of layers to alter starting from baseArrayLayer

	VkPipelineStageFlags srcStage = {}, dstStage = {};

	// If transitioning from new image to image ready to receive data...
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		imageMemoryBarrier.srcAccessMask = 0;										// Memory access stage transition must happen after this point
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;				// Memory access stage transition must happen before this point

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	// If transition from transfer destination to shader readable...
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		srcStage, dstStage,					// Pipeline Stages (Match to src and dst access masks)
		0,						// Dependency Flags
		0, nullptr,				// Memory Barrier count + data
		0, nullptr,				// Buffer Memory Barrier + data
		1, &imageMemoryBarrier	// Image Memory Barrier count + data
	);

	endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}