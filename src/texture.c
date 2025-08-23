#include "main.h"

void generateMipmaps(Application* app, VkCommandBuffer cmd, VkImage image, VkFormat format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++)
	{
		VkImageMemoryBarrier barrierBeforeBlit = {
		    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		    .image = image,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		    .subresourceRange.baseArrayLayer = 0,
		    .subresourceRange.layerCount = 1,
		    .subresourceRange.levelCount = 1,
		    .subresourceRange.baseMipLevel = i - 1,
		    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
		};

		vkCmdPipelineBarrier(
		    cmd,
		    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		    0,
		    0, NULL,
		    0, NULL,
		    1, &barrierBeforeBlit);

		VkImageBlit blit = {
		    .srcOffsets[1] = {mipWidth, mipHeight, 1},
		    .srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		    .srcSubresource.mipLevel = i - 1,
		    .srcSubresource.baseArrayLayer = 0,
		    .srcSubresource.layerCount = 1,

		    .dstOffsets[1] = {
		        mipWidth > 1 ? mipWidth / 2 : 1,
		        mipHeight > 1 ? mipHeight / 2 : 1,
		        1},
		    .dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		    .dstSubresource.mipLevel = i,
		    .dstSubresource.baseArrayLayer = 0,
		    .dstSubresource.layerCount = 1,
		};

		vkCmdBlitImage(
		    cmd,
		    image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		    image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		    1, &blit,
		    VK_FILTER_LINEAR);

		// Transition previous mip level to SHADER_READ_ONLY_OPTIMAL
		VkImageMemoryBarrier barrierAfterBlit = barrierBeforeBlit;
		barrierAfterBlit.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrierAfterBlit.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrierAfterBlit.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrierAfterBlit.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
		    cmd,
		    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		    0,
		    0, NULL,
		    0, NULL,
		    1, &barrierAfterBlit);

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	// Transition last mip level to SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier lastBarrier = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	    .image = image,
	    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .subresourceRange.baseMipLevel = mipLevels - 1,
	    .subresourceRange.levelCount = 1,
	    .subresourceRange.baseArrayLayer = 0,
	    .subresourceRange.layerCount = 1,
	    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
	    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
	};

	vkCmdPipelineBarrier(
	    cmd,
	    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	    0,
	    0, NULL,
	    0, NULL,
	    1, &lastBarrier);
}

void createTextureImage(Application* app, const char* path, Texture* outTexture, u32* outMipLevels)
{

	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	*outMipLevels = (u32)(floor(log2(texWidth > texHeight ? texWidth : texHeight))) + 1;

	if (!pixels)
	{
		fprintf(stderr, "Failed to load texture image: %s\n", path);
		fprintf(stderr, "STB Error: %s\n", stbi_failure_reason());
		exit(1);
	}

	printf("Loaded texture: %s (%dx%d, %d channels, %u mip levels)\n",
	    path, texWidth, texHeight, texChannels, *outMipLevels);

	Buffer stagingBuffer;
	createBuffer(app, &stagingBuffer, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	memcpy(stagingBuffer.data, pixels, (size_t)imageSize);
	stbi_image_free(pixels);

	VkImageCreateInfo imageInfo = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
	    .imageType = VK_IMAGE_TYPE_2D,
	    .extent.width = texWidth,
	    .extent.height = texHeight,
	    .extent.depth = 1,
	    .mipLevels = *outMipLevels,
	    .arrayLayers = 1,
	    .format = VK_FORMAT_R8G8B8A8_SRGB,
	    .tiling = VK_IMAGE_TILING_OPTIMAL,
	    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	    .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	    .samples = VK_SAMPLE_COUNT_1_BIT,
	};

	VK_CHECK(vkCreateImage(app->device, &imageInfo, NULL, &outTexture->image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(app->device, outTexture->image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
	    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
	    .allocationSize = memRequirements.size,
	    .memoryTypeIndex = selectmemorytype(&app->memoryProperties, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};

	VK_CHECK(vkAllocateMemory(app->device, &allocInfo, NULL, &outTexture->memory));
	VK_CHECK(vkBindImageMemory(app->device, outTexture->image, outTexture->memory, 0));

	VkCommandBuffer commandBuffer = beginSingleTimeCommands(app);

	// Transition layout to TRANSFER_DST_OPTIMAL
	VkImageMemoryBarrier barrier = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .image = outTexture->image,
	    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .subresourceRange.baseMipLevel = 0,
	    .subresourceRange.levelCount = *outMipLevels,
	    .subresourceRange.baseArrayLayer = 0,
	    .subresourceRange.layerCount = 1,
	    .srcAccessMask = 0,
	    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
	};

	vkCmdPipelineBarrier(
	    commandBuffer,
	    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	    0,
	    0, NULL,
	    0, NULL,
	    1, &barrier);

	// Copy buffer to image
	VkBufferImageCopy region = {
	    .bufferOffset = 0,
	    .bufferRowLength = 0,
	    .bufferImageHeight = 0,
	    .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .imageSubresource.mipLevel = 0,
	    .imageSubresource.baseArrayLayer = 0,
	    .imageSubresource.layerCount = 1,
	    .imageOffset = {0, 0, 0},
	    .imageExtent = {(u32)texWidth, (u32)texHeight, 1},
	};
	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.vkbuffer, outTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	// Generate mipmaps and transition layout to shader read
	generateMipmaps(app, commandBuffer, outTexture->image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, *outMipLevels);

	endSingleTimeCommands(app, commandBuffer);

	destroyBuffer(app->device, &stagingBuffer);

	// Create image view
	VkImageViewCreateInfo viewInfo = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	    .image = outTexture->image,
	    .viewType = VK_IMAGE_VIEW_TYPE_2D,
	    .format = VK_FORMAT_R8G8B8A8_SRGB,
	    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .subresourceRange.baseMipLevel = 0,
	    .subresourceRange.levelCount = *outMipLevels,
	    .subresourceRange.baseArrayLayer = 0,
	    .subresourceRange.layerCount = 1,
	};
	VK_CHECK(vkCreateImageView(app->device, &viewInfo, NULL, &outTexture->view));
}
void updateBaseColorAndHasTexture(Application* app)
{
	// ✅ Use already mapped pointer for baseColor
	memcpy(app->baseColorBuffer.data, app->mesh.base_color, sizeof(vec4));

	// ✅ Use already mapped pointer for hasTexture
	int hasTexture = app->mesh.has_texture;
	memcpy(app->hasTextureBuffer.data, &hasTexture, sizeof(int));
}

void createTextureSampler(Application* app, Texture* texture, u32 mipLevels)
{
	VkPhysicalDeviceProperties properties = {0};
	vkGetPhysicalDeviceProperties(app->physicalDevice, &properties);

	VkSamplerCreateInfo samplerInfo = {
	    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
	    .magFilter = VK_FILTER_LINEAR,
	    .minFilter = VK_FILTER_LINEAR,
	    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	    .anisotropyEnable = VK_TRUE,
	    .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
	    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
	    .unnormalizedCoordinates = VK_FALSE,
	    .compareEnable = VK_FALSE,
	    .compareOp = VK_COMPARE_OP_ALWAYS,
	    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
	    .mipLodBias = 0.0f,
	    .minLod = 0.0f,
	    .maxLod = (float)mipLevels,
	};

	VK_CHECK(vkCreateSampler(app->device, &samplerInfo, NULL, &texture->sampler));
}

void createStorageImage(Application* app, StorageImage* img, uint32_t width, uint32_t height)
{
	img->extent.width = width;
	img->extent.height = height;
	img->format = VK_FORMAT_R32G32B32A32_SFLOAT; // Example format

	VkImageCreateInfo imageInfo = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
	    .imageType = VK_IMAGE_TYPE_2D,
	    .format = img->format,
	    .extent = {width, height, 1},
	    .mipLevels = 1,
	    .arrayLayers = 1,
	    .samples = VK_SAMPLE_COUNT_1_BIT,
	    .tiling = VK_IMAGE_TILING_OPTIMAL,
	    .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // For compute and sampling
	    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VK_CHECK(vkCreateImage(app->device, &imageInfo, NULL, &img->image));

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(app->device, img->image, &memReqs);

	VkMemoryAllocateInfo allocInfo = {
	    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
	    .allocationSize = memReqs.size,
	    .memoryTypeIndex = selectmemorytype(&app->memoryProperties, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};
	VK_CHECK(vkAllocateMemory(app->device, &allocInfo, NULL, &img->memory));
	VK_CHECK(vkBindImageMemory(app->device, img->image, img->memory, 0));

	VkImageViewCreateInfo viewInfo = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	    .image = img->image,
	    .viewType = VK_IMAGE_VIEW_TYPE_2D,
	    .format = img->format,
	    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
	};
	VK_CHECK(vkCreateImageView(app->device, &viewInfo, NULL, &img->view));
}
