#pragma once

#include <fstream>

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
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