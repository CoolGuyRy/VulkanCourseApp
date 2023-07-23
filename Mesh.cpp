#include "Mesh.h"

Mesh::Mesh() : vertexCount(0) {
	device = nullptr;
	physicalDevice = nullptr;
	vertexBuffer = nullptr;
	vertexBufferMemory = nullptr;
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex>* vertices) {
	vertexCount = (int)vertices->size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	createVertexBuffer(vertices);
}

int Mesh::getVertexCount() {
	return vertexCount;
}

VkBuffer Mesh::getVertexBuffer() {
	return vertexBuffer;
}

void Mesh::destroyVertexBuffer() {
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

Mesh::~Mesh() {
}

void Mesh::createVertexBuffer(std::vector<Vertex>* vertices) {
	// INformation to create a buffer (doesnt include assigning memory)
	VkBufferCreateInfo bufferInfo = { };
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Vertex) * vertices->size();		// Size of whole buffer (size of a vertex * number of vertices)
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;		// Multipe types of buffer possible, we want vertex buffer
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// Similar to swapchain images, can share vertex buffers

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create a Vertex Buffer!");
	}

	// Get Buffer Memory Requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

	// Allocate Memory to buffer
	VkMemoryAllocateInfo memoryAllocInfo = { };
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits,	// index of memory type on Physical Device that has required bit flags
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);		// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT - CPU can interact with memory
																							// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT - Allows placement of data straight into buffer after mapping (otherwise would have to specify manually)
	// Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, &vertexBufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate Vertex Buffer Memory!");
	}

	// Allocate memory to given vertex buffer
	vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

	// Map memory to Vertex Buffer
	void* data;																// 1. Create pointer to a point in normal memory
	vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);	// 2. Map the vertex buffer memory to that point
	memcpy(data, vertices->data(), (size_t)bufferInfo.size);				// 3. Copy memory from vertices vector to the point
	vkUnmapMemory(device, vertexBufferMemory);								// 4. Unmap the vertex buffer memory
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties) {
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
