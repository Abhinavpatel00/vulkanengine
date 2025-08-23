#include "main.h"


void createComputeSync(Application* app);
void updateStorageImage(Application* app, StorageImage* img, float* data);
void clearStorageImage(Application* app, StorageImage* img);
void createComputePipeline(Application* app, ComputePipeline* compute, const char* shaderPath, VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount);
void createComputeDescriptors(Application* app, ComputePipeline* compute, VkDescriptorSet* descriptorSet, VkDescriptorPoolSize* poolSizes, uint32_t poolSizeCount, VkWriteDescriptorSet* descriptorWrites, uint32_t descriptorWriteCount);
void createComputeDescriptorSetLayout(Application* app);
void recordComputeCommands(Application* app);
void updateComputeUniforms(Application* app, vec3 mousePos, int is_additive, vec2 path_mask_ws_dims);

void createComputeSync(Application* app)
{
	VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	vkCreateFence(app->device, &fenceInfo, NULL, &app->computeFence);
}
void updateStorageImage(Application* app, StorageImage* img, float* data)
{
	// Create staging buffer
	VkDeviceSize size = img->extent.width * img->extent.height * 4 * sizeof(float);
	Buffer staging = createStagingBuffer(app, data, size);

	// Copy buffer to image
	VkCommandBuffer cmd = beginSingleTimeCommands(app);

	// Transition image to TRANSFER_DST_OPTIMAL
	transitionImageLayout(
	    cmd,
	    img->image,
	    VK_IMAGE_LAYOUT_UNDEFINED,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    0,
	    VK_ACCESS_TRANSFER_WRITE_BIT,
	    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	    VK_PIPELINE_STAGE_TRANSFER_BIT,
	    VK_IMAGE_ASPECT_COLOR_BIT);

	VkBufferImageCopy region = {
	    .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
	    .imageExtent = {img->extent.width, img->extent.height, 1}};

	vkCmdCopyBufferToImage(
	    cmd,
	    staging.vkbuffer,
	    img->image,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    1,
	    &region);

	// Transition image to GENERAL for shader access
	transitionImageLayout(
	    cmd,
	    img->image,
	    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    VK_IMAGE_LAYOUT_GENERAL,
	    VK_ACCESS_TRANSFER_WRITE_BIT,
	    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
	    VK_PIPELINE_STAGE_TRANSFER_BIT,
	    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	    VK_IMAGE_ASPECT_COLOR_BIT);

	endSingleTimeCommands(app, cmd);
	destroyBuffer(app->device, &staging);
}

// Clear image (replaces clear())
void clearStorageImage(Application* app, StorageImage* img)
{
	VkClearColorValue clear = {{0.0f, 0.0f, 0.0f, 1.0f}};
	VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

	VkCommandBuffer cmd = beginSingleTimeCommands(app);
	vkCmdClearColorImage(cmd, img->image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
	endSingleTimeCommands(app, cmd);
}

void createComputePipeline(Application* app, ComputePipeline* compute, const char* shaderPath, VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount)
{
	// Load compute shader module
	VkShaderModule shader = LoadShaderModule(shaderPath, app->device);

	VkDescriptorSetLayoutCreateInfo layoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	    .pNext = NULL,
	    .flags = 0,
	    .bindingCount = bindingCount,
	    .pBindings = bindings,
	};

	VK_CHECK(vkCreateDescriptorSetLayout(app->device, &layoutInfo, NULL, &compute->descLayout));

	VkPipelineLayoutCreateInfo plLayoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	    .pNext = NULL,
	    .flags = 0,
	    .setLayoutCount = 1,
	    .pSetLayouts = &compute->descLayout,
	    .pushConstantRangeCount = 0,
	    .pPushConstantRanges = NULL,
	};

	VK_CHECK(vkCreatePipelineLayout(app->device, &plLayoutInfo, NULL, &compute->layout));
	VkPipelineShaderStageCreateInfo stageInfo = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	    .pNext = NULL,
	    .flags = 0,
	    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
	    .module = shader,
	    .pName = "main",
	    .pSpecializationInfo = NULL,
	};

	VkComputePipelineCreateInfo cpInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
	    .pNext = NULL,
	    .flags = 0,
	    .stage = stageInfo,
	    .layout = compute->layout,
	    .basePipelineHandle = VK_NULL_HANDLE,
	    .basePipelineIndex = -1,
	};

	VK_CHECK(vkCreateComputePipelines(app->device, VK_NULL_HANDLE, 1, &cpInfo, NULL, &compute->pipeline));

	vkDestroyShaderModule(app->device, shader, NULL);
}

void createComputeDescriptors(Application* app, ComputePipeline* compute, VkDescriptorSet* descriptorSet, VkDescriptorPoolSize* poolSizes, uint32_t poolSizeCount, VkWriteDescriptorSet* descriptorWrites, uint32_t descriptorWriteCount)
{
	// Create descriptor pool
	VkDescriptorPoolCreateInfo poolInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
	    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
	    .maxSets = 1,
	    .poolSizeCount = poolSizeCount,
	    .pPoolSizes = poolSizes,
	};

	VK_CHECK(vkCreateDescriptorPool(app->device, &poolInfo, NULL, &compute->descPool));

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo allocInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	    .descriptorPool = compute->descPool,
	    .descriptorSetCount = 1,
	    .pSetLayouts = &compute->descLayout};
	VK_CHECK(vkAllocateDescriptorSets(app->device, &allocInfo, descriptorSet));

	// Update descriptor set
	for (uint32_t i = 0; i < descriptorWriteCount; ++i)
	{
		descriptorWrites[i].dstSet = *descriptorSet;
	}

	vkUpdateDescriptorSets(app->device, descriptorWriteCount, descriptorWrites, 0, NULL);
}




