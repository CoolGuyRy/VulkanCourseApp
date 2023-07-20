#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer() {
	window = nullptr;
	instance = nullptr;
	graphicsQueue = nullptr;
	presentationQueue = nullptr;
	surface = nullptr;
	swapchain = nullptr;
	swapChainExtent = { };
	swapChainImageFormat = { };
	pipelineLayout = nullptr;
	renderPass = nullptr;
	graphicsPipeline = nullptr;
	graphicsCommandPool = nullptr;

	mainDevice = { };
}

VulkanRenderer::~VulkanRenderer() {

}

int VulkanRenderer::init(GLFWwindow* newWindow) {
	window = newWindow;
	
	try {
		createInstance();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		recordCommands();
		createSynchronization();
		
		// DebugInformation();
	} catch (const std::runtime_error& e) {
		std::cout << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
}

void VulkanRenderer::draw() {
	// 1. Get the next available image to draw to and set something to signal when we're finished with the image (a semaphore)

	// wait for given fence to signal open from last draw call
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	// manually reset fence
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);
	
	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	// 2. Submit our command buffer to the queue for execution, making sure it waits for the image to be signaled as available before drawing
	//	  and signals when it has finished rendering
	 
	VkSubmitInfo submitInfo = { };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;								// Number of semaphores to wait on
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];					// list of semaphores to wait on
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;						// stages to check semaphores at
	submitInfo.commandBufferCount = 1;								// number of command buffers to submit
	submitInfo.pCommandBuffers = &commandBuffers.at(imageIndex);	// command buffer to submit
	submitInfo.signalSemaphoreCount = 1;							// semaphores to signal when command buffer has finished
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];					// semaphores to signal when command buffer finished

	// Submit command buffer to queue
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit Command Buffer to Queue!");
	}
	
	// 3. Present image to screen when it has signaled finished rendering
	VkPresentInfoKHR presentInfo = { };
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to present Image!");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_DRAWS;
}

void VulkanRenderer::cleanup() {
	// wait until no actions being run on device before destroying
	vkDeviceWaitIdle(mainDevice.logicalDevice);
	//vkQueueWaitIdle(graphicsQueue);
	//vkQueueWaitIdle(presentationQueue);
	for (size_t i = 0; i < MAX_FRAMES_DRAWS; i++) {
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}
	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (auto& frameBuffer : swapChainFramebuffers) {
		vkDestroyFramebuffer(mainDevice.logicalDevice, frameBuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
	for (auto& image : swapChainImages) {
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void VulkanRenderer::createInstance() {
	// Validation Layer Check
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("Validation Layers Requested, but not available!");
	}


	// Information about the application itself
	// Most data here doesn't affect the program and is for developer convenience
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

	// Vulkan Version 1.0.0 (This does affect the program)
	appInfo.apiVersion = VK_API_VERSION_1_1;
	
	// Creation information for a VkInstance (Vulkan Instance)
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	

	// Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();

	// Set up extensions Instance will use
	uint32_t glfwExtensionCount = 0;	// GLFW may require multiple extensions
	const char** glfwExtensions;		// Extensions passed as array of cstrings, so need pointer to pointer

	// Get GLFW extensions
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Add GLFW extensions to list of extensions
	for (size_t i = 0; i < glfwExtensionCount; i++) {
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	// Check Instance Extensions supported...
	if (!checkInstanceExtensionSupport(&instanceExtensions)) {
		throw std::runtime_error("VkInstance does not support required extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// Now that we are using validation layers, set them up accordingly
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	// Create instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Vulkan Instance!");
	}
}

void VulkanRenderer::createLogicalDevice() {
	// Get the queue family indices for the chosen Physical Device
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	// Vector for queue creation information, and set for family indices
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };



	// Queues the logical device need to create and info to do so
	for (int queueFamilyIndex : queueFamilyIndices) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;				// The index of the family to create a queue from
		queueCreateInfo.queueCount = 1;										// Number of queues to create
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;						// Vulkan needs to know how to handle multiple queues, so decide priority (1 = highest priority)


		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Information to create logical device (sometimes called "device")
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());		// Number of queue create infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();								// list of queue create infos so device can create required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());	// number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();							// list of enabled logical device extensions

	// Physical Device Features the Logical Device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;					// Physical Device features Logical Device will use

	// Create the logical device for the given physical device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a logical device!");
	}

	// Queues are created at the same time as the device...
	// So we want to handle the queues
	// From given logical device, of given Queue Family, of given Queue Index (0 since only one queue), place reference in given VkQueue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::createSurface() {
	// Creates a platform-agnositc surface create info struct, creates a surface, and returns a result
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a surface!");
	}
}

void VulkanRenderer::createSwapChain() {
	// Get Swap Chain details so we can pick best settings
	SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

	// 1. Choose best Surface Format
	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
	// 2. Choose best Presentation Mode
	VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
	// 3. Choose Swap Chain Image Resolution
	VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	// How many images are in the swapchain? Get 1 more than the minmum to allow triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

	// if imagecount higher than max, then clamp down to max
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount) {
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	// Creation Information for SwapChain
	VkSwapchainCreateInfoKHR swapChainCreateInfo = { };
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;														// Swapchain surface
	swapChainCreateInfo.imageFormat = surfaceFormat.format;										// Swapchain format
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;								// Swapchain color space
	swapChainCreateInfo.presentMode = presentMode;												// Swapchain presentation mode
	swapChainCreateInfo.imageExtent = extent;													// Swapchain image extents
	swapChainCreateInfo.minImageCount = imageCount;												// Minimum images in swapchain
	swapChainCreateInfo.imageArrayLayers = 1;													// Number of layers for each image in chain
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;						// What attachment images will be used as
	swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;	// Transform to perform on swapchain images
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;						// How to handle blending images with external graphgics (e.g. other windows)
	swapChainCreateInfo.clipped = VK_TRUE;														// Whether to clip parts of images not in view (e.g. behind another window, offscreen, etc)

	// Get queue family indices
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	// if graphics and presentation families are different, then swapchain must let images be shared between families

	if (indices.graphicsFamily != indices.presentationFamily) {
		uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.graphicsFamily,
			(uint32_t)indices.presentationFamily
		};

		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;		// Image Share handling
		swapChainCreateInfo.queueFamilyIndexCount = 2;							// Number of queues to share images between
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;			// array of queues to share between
	} else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	// if old swap chain been destroyed and this one replaces it, then link old one to quickly hand over responsibilities
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create Swapchain
	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapchain);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Swapchain!");
	}

	// Store for later reference
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	// Get swap chain images
	uint32_t swapChainImageCount = 0;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);

	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());

	for (VkImage image : images) {
		// Store the image handle
		SwapchainImage swapChainImage = { };
		swapChainImage.image = image;

		// Create ImageView here
		swapChainImage.imageView = createImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		
		// Add to swapchain image list
		swapChainImages.push_back(swapChainImage);
	}
}

void VulkanRenderer::createRenderPass() {
	// Color Attachment of render pass
	VkAttachmentDescription colorAttachment = { };
	colorAttachment.format = swapChainImageFormat;											// Format to use for attachment
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;										// Number of samples to write for multisampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;									// Describes what to do with attachment before rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;									// Describes what to do with attachement after rendering
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;						// Describes what to do with stencil before rendering
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;						// Describes what to do with stencil after rendering
	
	// Framebuffer data will be stored as an image, but images can be given different data layouts
	// to give optimal use for certain operations
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;								// Image Data Layout before render pass starts
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;							// Image Data Layout after render pass (to change to)

	// Attachment Reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
	VkAttachmentReference colorAttachmentReference = { };
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Information about a particular subpass the Render Pass is using
	VkSubpassDescription subpass = { };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;							// Pipeline type subpass is to be bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;

	// Need to determine when layout transitions occur using subpass dependencies
	std::array<VkSubpassDependency, 2> subpassDependencies = { };

	// Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	// Transition must happen after...
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;										// Subpass index (VK_SUBPASS_EXTERNAL = special value meaning outside of renderpass)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;						// Pipeline Stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;								// Stage access mask (memory access)

	// But must happen before...
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after...
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// But must happen before...
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;


	// Create info for renderpass
	VkRenderPassCreateInfo renderPassCreateInfo = { };
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Render Pass!");
	}
}

void VulkanRenderer::createGraphicsPipeline() {
	// Read in SPIR-V code for shaders
	auto vertexShaderCode = readFile("Shaders/vert.spv");
	auto fragmentShaderCode = readFile("Shaders/frag.spv");

	//Build Shader Modules to link to Graphics Pipeline
	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	// Shader Stage Creation Information

	// Vertex Stage Creation Information
	VkPipelineShaderStageCreateInfo vertexShaderCreateinfo = { };
	vertexShaderCreateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateinfo.stage = VK_SHADER_STAGE_VERTEX_BIT;								// Shader Stage name
	vertexShaderCreateinfo.module = vertexShaderModule;										// Shader Module to be used by stage
	vertexShaderCreateinfo.pName = "main";													// Entry point into shader

	// Fragment Stage Creation Information
	VkPipelineShaderStageCreateInfo fragmentShaderCreateinfo = { };
	fragmentShaderCreateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateinfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;							// Shader Stage name
	fragmentShaderCreateinfo.module = fragmentShaderModule;									// Shader Module to be used by stage
	fragmentShaderCreateinfo.pName = "main";												// Entry point into shader

	// Put shader stage creation info into array
	// Graphics pipeline creation info requires array of shader stage creates
	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertexShaderCreateinfo, fragmentShaderCreateinfo
	};

	// Create Pipeline

	// Vertex Input (TODO: Put in vertex descriptions when resources created)
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = { };
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;				// List of Vertex Binding Descriptions (data spacing / stride info)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;			// List of Vertex Attribut Descriptions (data format and where to bind to/from)

	// Input Assembler
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { };
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;			// Primitive type to assemble vertices as
	inputAssembly.primitiveRestartEnable = VK_FALSE;						// Allow overriding of "strip" topology to start new primitives

	// Viewport and Scissor
	VkViewport viewport = { };
	viewport.x = 0.0f;														// x start coordinate
	viewport.y = 0.0f;														// y start coordinate
	viewport.width = (float)swapChainExtent.width;							// width of viewport
	viewport.height = (float)swapChainExtent.height;						// height of viewport
	viewport.minDepth = 0.0f;												// min framebuffer depth
	viewport.maxDepth = 1.0f;												// max framebuffer depth

	VkRect2D scissor = { };
	scissor.offset = { 0, 0 };												// offset to use region from
	scissor.extent = swapChainExtent;										// Extent to describe region to use, starting at offset

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = { };
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	/*
	
	// Dynamic State
	std::vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);				// Dynamic Viewport : Can resize in command buffer with vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);				// Dyanmic Scissor  : Can resize in command buffer with vkCmdSetScissor(commandbuffer, 0, 1, &scissor);

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { };
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

	// Can't forget to remake swapchain and any depth / image buffers you are using
	
	*/

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = { };
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;						// Change if fragments beyond near / far planes are clipped (default) or clamped to plane
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;				// Whether to discard data and skip rassterizer. Used if you don't want to output to a framebuffer but want to use the other parts of the pipeline.
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;				// How to handle filling points between vertices. Can just do lines or points
	rasterizerCreateInfo.lineWidth = 1.0f;									// How thick lines should be when drawn
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;					// Which face of a triangle to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;				// Winding to determine which side is front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;						// Whether to add depth bias to fragments (good for stopping "shadow acne" in shadow mapping)

	// Multisampling
	VkPipelineMultisampleStateCreateInfo multiSamplingCreateInfo = { };
	multiSamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multiSamplingCreateInfo.sampleShadingEnable = VK_FALSE;					// Enable multisample shading or not
	multiSamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// Number of samples to use per fragment

	// Blending
	// Blend Attachment State (how blending is handled)
	VkPipelineColorBlendAttachmentState colorState = { };
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;		// Colors to apply blending to
	colorState.blendEnable = VK_TRUE;										// Enable Blending
	// Blending uses equation: (srcColorBlendFactor * new Color) colorBlendOp (dstColorBlendFactor * old color)
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;
	// Summarized:  (VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * old color)
	//				( new color alpha * new color) + ( (1 - new color alpha) * old color)
	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;
	// Summarized: (1 * new alpha) + (0 * old alpha) = new alpha

	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = { };
	colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE;						// Alternative to calculations is to use logical operations
	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorState;

	// Pipeline Layout (TODO: Apply Future Desciptor Set Layouts)
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { };
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	// Create Pipeline Layout
	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create Pipeline Layout!");
	}

	// Depth Stencil Testing
	// TODO: Set up depth stencil testing

	// Create Graphics Pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { };
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multiSamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = nullptr;
	pipelineCreateInfo.layout = pipelineLayout;							// Pipeline layout pipeline should use
	pipelineCreateInfo.renderPass = renderPass;							// renderpass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;										// subpass of render pass to use with pipeline

	// Pipeline Derivatives : Can create multiple pipelines that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;				// Exisiting pipeline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;							// Or index of pipeline being created to derive from (in case creating multiple at once)

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create Graphics Pipeline!");
	}

	// Destroy Shader Modules, no longer needed after Pipeline created
	// destroyed in reverse order
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
}

void VulkanRenderer::createFramebuffers() {
	// Resize framebuffer count to equal swapchain image count
	swapChainFramebuffers.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
		std::array<VkImageView, 1> attachments = {
			swapChainImages[i].imageView
		};

		VkFramebufferCreateInfo framebufferCreateInfo = { };
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;											// render pass layout the Framebuffer will be used with
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());		//
		framebufferCreateInfo.pAttachments = attachments.data();								// list of attachments (1:1 with render pass)
		framebufferCreateInfo.width = swapChainExtent.width;									// framebuffer width
		framebufferCreateInfo.height = swapChainExtent.height;									// framebuffer height
		framebufferCreateInfo.layers = 1;														// framebuffer layers

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFramebuffers.at(i));

		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Framebuffer!");
		}
	}
}

void VulkanRenderer::createCommandPool() {
	VkCommandPoolCreateInfo poolInfo = { };
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = getQueueFamilies(mainDevice.physicalDevice).graphicsFamily;		// Queue Family type that buffers from this command pool will use

	// Create a graphics queue family command pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Command Pool!");
	}
}

void VulkanRenderer::createCommandBuffers() {
	// Resize command buffer count to have one for each frame buffer
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = { };
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;					// VK_COMMAND_BUFFER_PRIMARY : Buffer you submit directly to queue. Can't be called by other buffers
																			// VK_COMMAND_BUFFER_SECONDARY : Buffer can't be called directly. Can be called from other buffers via "vkCmdExecuteCommands"
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate Command Buffers!");
	}
}

void VulkanRenderer::createSynchronization() {
	imageAvailable.resize(MAX_FRAMES_DRAWS);
	renderFinished.resize(MAX_FRAMES_DRAWS);
	drawFences.resize(MAX_FRAMES_DRAWS);

	VkSemaphoreCreateInfo semaphoreCreateInfo = { };
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = { };
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;


	for (size_t i = 0; i < MAX_FRAMES_DRAWS; i++) {
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS
			|| vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS
			|| vkCreateFence(mainDevice.logicalDevice, &fenceInfo, nullptr, &drawFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create a Semaphore and/or Fence!");
		}
	}
}

void VulkanRenderer::recordCommands() {
	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// Information about how to begin a render pass (only needed for a graphical application)
	VkRenderPassBeginInfo renderPassBeginInfo = { };
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;								// Render pass to begin
	renderPassBeginInfo.renderArea.offset = { 0, 0 };							// start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent;					// size of region to run render pass on (starting at offset)
	renderPassBeginInfo.clearValueCount = 1;
	VkClearValue clearValues[] = {												// TODO: Add depth attachment
		{0.6f, 0.65f, 0.4f, 1.0f }
	};
	renderPassBeginInfo.pClearValues = clearValues;

	for (size_t i = 0; i < commandBuffers.size(); i++) {
		renderPassBeginInfo.framebuffer = swapChainFramebuffers[i];

		// Start recording commands to command buffer!
		VkResult result;
		result = vkBeginCommandBuffer(commandBuffers.at(i), &beginInfo);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to start recording a command buffer!");
		}

			vkCmdBeginRenderPass(commandBuffers.at(i), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Bind Pipeline to be used in render pass
				vkCmdBindPipeline(commandBuffers.at(i), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
				// Can add mutliple bind pipeline cmd calls. Useful for doing deferred shading.

				vkCmdDraw(commandBuffers.at(i), 3, 1, 0, 0);

			vkCmdEndRenderPass(commandBuffers.at(i));

		// Stop recording to command buffer
		result = vkEndCommandBuffer(commandBuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to stop recording a command buffer!");
		}
	}
}

void VulkanRenderer::getPhysicalDevice() {
	// Enumerate Physical devices the vkInstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	// if no devices available, then none support Vulkan!
	if (deviceCount == 0) {
		throw std::runtime_error("Can't find a GPU that supports Vulkan Instance!");
	}

	// Get list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	// TEMP: Pick first device
	for (const auto& device : deviceList) {
		if (checkDeviceSuitable(device)) {
			mainDevice.physicalDevice = device;
			break;
		}
	}
}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions) {
	// Need to get number of extensions to create array of correct size to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	// Create a list of VkExtensionProperties using count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	// Check if given extensions are in list of available extensions
	for (const auto& checkExtension : *checkExtensions) {
		bool hasExtension = false;
		for (const auto& extension : extensions) {
			if (strcmp(checkExtension, extension.extensionName)) {
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension) {
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device) {
	/*
	// Information about the device itself (ID, name, type, vendor, etc)
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// Information about what the device can do (geo shader, tess shader, wide lines, etc)
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	*/

	QueueFamilyIndices indices = getQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainValid = false;

	if (extensionsSupported) {
		SwapChainDetails swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	return indices.isValid() && extensionsSupported && swapChainValid;
}

bool VulkanRenderer::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
	// Get device extension count
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	// if no extensions found return fail
	if (extensionCount == 0) {
		return false;
	}

	// populate list of extensions
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	// Check for extension
	for (const auto& deviceExtension : deviceExtensions) {
		bool hasExtension = false;
		for (const auto& extension : extensions) {
			if (strcmp(deviceExtension, extension.extensionName) == 0) {
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension) {
			return false;
		}
	}

	return true;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;

	// Get all Queue Family Property info for the given device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	// Go through each queue family and check if it has at least of the required types of queue
	int i = 0;
	for (const auto& queueFamily : queueFamilyList) {
		// First check if queue family has atleast 1 queue in that family (could have no queues)
		// Queue can be multiple types defined through bitfield. Need to bitwise AND with VK_QUEUE_*_BIT to check
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i; // if queue family is valid, then get index
		}

		// Check if queue family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
		// Check if queue is presentation type (can be both graphics and presentation)
		if (queueFamily.queueCount > 0 && presentationSupport) {
			indices.presentationFamily = i;
		}

		// Check if queue family indices are in a valid state, stop searching if so
		if (indices.isValid()) {
			break;
		}

		i++;
	}

	return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device) {
	SwapChainDetails swapChainDetails;

	// Capabilities
	// Get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

	// Formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	// if formats returned, get list of formats
	if (formatCount != 0) {
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}

	// Presentation Modes
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	// if presentation modes returned, get list
	if (presentationCount != 0) {
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}

VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
	// Best format is subjective, but ours will be:
	// Format		:	VK_FORMAT_R8G8B8A8_UNORM (VK_FORMAT_B8G8R8A8_UNORM as backup)
	// Color Space	:	VK_COLOR_SPACE_SRGB_NONLINEAR_KHR

	// if only 1 availabe and is undefined, then all formats are available
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	// if restricted, search for optimal format
	for (const auto& format : formats) {
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}

	// if can't find optimal format, then return first format
	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes) {
	// Look for mailbox presentation mode
	for (const auto& presentationMode : presentationModes) {
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return presentationMode;
		}
	}

	// This will always be available, good backup
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
	// if current extent is at numeric limits then extent can vary, otherwise it is the size of the window
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return surfaceCapabilities.currentExtent;
	} else {
		// if value can vary, need to set manually

		// get window size
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		// Create new extent using window size
		VkExtent2D newExtent = { };
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		// Surface also defines max and min, so make sure within boundaries by clamping value
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;
	}
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
	VkImageViewCreateInfo viewCreateInfo = { };
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;										// Image to create view for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;					// Type of image (1D, 2D, 3D, Cube, etc)
	viewCreateInfo.format = format;										// Format of image data
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	//viewCreateInfo.flags = aspectFlags;									

	// Subresources allow the view to view only a part of the image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;			// Which aspect of image to view (e.g. COLOR_BIT for viewing color)
	viewCreateInfo.subresourceRange.baseMipLevel = 0;					// Start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;						// Number of mipmap levels to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;					// Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;						// Number of array levels to view

	// Create image view and return it
	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create an ImageView!");
	}

	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code) {
	// Shader Module Creation Information
	VkShaderModuleCreateInfo shaderModuleCreateInfo = { };
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();										// size of code
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());		// pointer to code (of uint32_t pointer type)

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Shader Module!");
	}

	return shaderModule;
}