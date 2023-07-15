#pragma once

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