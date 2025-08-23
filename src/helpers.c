#include "main.h"
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
