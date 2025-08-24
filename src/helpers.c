#include "main.h"


void copyBufferToDeviceLocal(VkDevice device, VkCommandPool commandPool, VkQueue queue,
    VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
	VkCommandBufferAllocateInfo allocInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	    .commandPool = commandPool,
	    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	    .commandBufferCount = 1,
	};

	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd));

	VkCommandBufferBeginInfo beginInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

	VkBufferCopy copyRegion = {.size = size};
	vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submitInfo = {
	    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
	    .commandBufferCount = 1,
	    .pCommandBuffers = &cmd,
	};

	VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(queue));

	vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

Buffer createStagingBuffer(Application* app, const void* data, VkDeviceSize size)
{
	Buffer staging;
	createBuffer(app, &staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	memcpy(staging.data, data, size);
	return staging;
}

VkCommandBuffer beginSingleTimeCommands(Application* app)
{
	VkCommandBufferAllocateInfo allocInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	    .commandPool = app->commandPool,
	    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	    .commandBufferCount = 1,
	};
	VkCommandBuffer commandBuffer;
	VK_CHECK(vkAllocateCommandBuffers(app->device, &allocInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
	return commandBuffer;
}

void endSingleTimeCommands(Application* app, VkCommandBuffer commandBuffer)
{
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {
	    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
	    .commandBufferCount = 1,
	    .pCommandBuffers = &commandBuffer,
	};
	VK_CHECK(vkQueueSubmit(app->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(app->graphicsQueue));

	vkFreeCommandBuffers(app->device, app->commandPool, 1, &commandBuffer);
}

void createBuffer(Application* app, Buffer* buffer, VkDeviceSize size, VkBufferUsageFlags usage)
{
	buffer->size = size;

	// 1. Create buffer
	VkBufferCreateInfo bufferInfo = {
	    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
	    .size = size,
	    .usage = usage,
	    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	VK_CHECK(vkCreateBuffer(app->device, &bufferInfo, NULL, &buffer->vkbuffer));

	// 2. Get memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(app->device, buffer->vkbuffer, &memRequirements);

	// 3. Find memory type and allocate
	VkMemoryPropertyFlags desiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	uint32_t memoryTypeIndex = findMemoryType(&app->memoryProperties, memRequirements.memoryTypeBits, desiredFlags);
	VkMemoryAllocateInfo allocInfo = {
	    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
	    .pNext = NULL,
	    .allocationSize = bufferInfo.size,
	};
	allocInfo.memoryTypeIndex = memoryTypeIndex;

	VK_CHECK(vkAllocateMemory(app->device, &allocInfo, NULL, &buffer->memory));
	VK_CHECK(vkBindBufferMemory(app->device, buffer->vkbuffer, buffer->memory, 0));

	// Now check property flags before mapping
	if (desiredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VK_CHECK(vkMapMemory(app->device, buffer->memory, 0, size, 0, &buffer->data));
	}
}
void transitionImageLayout(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage,
    VkImageAspectFlags aspectMask)
{
	VkImageMemoryBarrier barrier = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	    .oldLayout = oldLayout,
	    .newLayout = newLayout,
	    .srcAccessMask = srcAccessMask,
	    .dstAccessMask = dstAccessMask,
	    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .image = image,
	    .subresourceRange = {
	        .aspectMask = aspectMask,
	        .baseMipLevel = 0,
	        .levelCount = 1,
	        .baseArrayLayer = 0,
	        .layerCount = 1,
	    },
	};

	vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

VkSemaphore createSemaphore(VkDevice device)
{
	VkSemaphoreCreateInfo semaphoreInfo = {
	    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

	VkSemaphore semaphore;
	VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &semaphore));
	return semaphore;
}



uint32_t findMemoryType(const VkPhysicalDeviceMemoryProperties* memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	for (uint32_t i = 0; i < memProperties->memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) &&
		    (memProperties->memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	fprintf(stderr, "Failed to find suitable memory type!\n");
	exit(1);
}
// --- Memory/Buffer Helpers ---

u32 selectmemorytype(
    VkPhysicalDeviceMemoryProperties* memprops, u32 memtypeBits, VkFlags requirements_mask)
{
	for (u32 i = 0; i < memprops->memoryTypeCount; ++i)
	{
		if ((memtypeBits & 1) == 1)
		{
			if ((memprops->memoryTypes[i].propertyFlags & requirements_mask) ==
			    requirements_mask)
			{
				return i;
			}
		}
		memtypeBits >>= 1;
	}
	assert(0 && "No suitable memory type found");
	return 0;
}


VkShaderModule LoadShaderModule(const char* filepath, VkDevice device)
{
	FILE* file = fopen(filepath, "rb");
	assert(file);

	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	assert(length >= 0);
	fseek(file, 0, SEEK_SET);

	char* buffer = (char*)malloc(length);
	assert(buffer);

	size_t rc = fread(buffer, 1, length, file);
	assert(rc == (size_t)length);
	fclose(file);

	VkShaderModuleCreateInfo createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = length;
	createInfo.pCode = (const u32*)buffer;

	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(device, &createInfo, NULL, &shaderModule));

	free(buffer);
	return shaderModule;
}

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice)
{
	const VkFormat candidates[] = {
	    VK_FORMAT_D32_SFLOAT,
	    VK_FORMAT_D32_SFLOAT_S8_UINT,
	    VK_FORMAT_D24_UNORM_S8_UINT};

	for (size_t i = 0; i < ARRAYSIZE(candidates); i++)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, candidates[i], &props);

		if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			return candidates[i];
		}
	}

	assert(0 && "Failed to find supported depth format");
	return VK_FORMAT_UNDEFINED;
}
