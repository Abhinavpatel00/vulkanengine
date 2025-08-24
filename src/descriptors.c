#include "main.h"

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device)
{
	VkDescriptorSetLayoutBinding bindings[] = {
	    {
	        .binding = 0,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	    },
	    {
	        .binding = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	    },
	    {
	        .binding = 2,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	    },
	    {
	        .binding = 3,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	    },
	    {
	        .binding = 4,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	    },
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	    .bindingCount = ARRAYSIZE(bindings),
	    .pBindings = bindings,
	};

	VkDescriptorSetLayout descriptorSetLayout;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, &descriptorSetLayout));
	printf("createDescriptorSetLayout returning: %p\n", (void*)descriptorSetLayout);
	return descriptorSetLayout;
}

VkDescriptorPool createDescriptorPool(VkDevice device)
{
	VkDescriptorPoolSize poolSizes[] = {
	    {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 300},         // Increased for alpha cutoff buffer (26 textures × 4 uniform buffers each + extra)
	    {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 500}, // Much more for many textures (26 textures × 2 samplers each + extra)
	    {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 100},
	};

	VkDescriptorPoolCreateInfo poolInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
	    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // Allow freeing individual sets
	    .maxSets = 100,                                             // Allow many descriptor sets
	    .poolSizeCount = 3,
	    .pPoolSizes = poolSizes,
	};

	VkDescriptorPool descriptorPool;
	VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool));
	return descriptorPool;
}

VkDescriptorSet allocateDescriptorSet(VkDevice device, VkDescriptorPool pool, const VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetAllocateInfo allocInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	    .descriptorPool = pool,
	    .descriptorSetCount = 1,
	    .pSetLayouts = pLayout};

	VkDescriptorSet descriptorSet;
	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
	return descriptorSet;
}

void createDescriptors(Application* app)
{
	// Create descriptor pool
	app->descriptorPool = createDescriptorPool(app->device);

	// Allocate descriptor sets for each material
	app->descriptorSets = calloc(app->mesh.material_count, sizeof(VkDescriptorSet));

	for (u32 i = 0; i < app->mesh.material_count; i++)
	{
		printf("app->descriptorSetLayout in createDescriptors loop: %p\n", (void*)app->descriptorSetLayout);
		app->descriptorSets[i] = allocateDescriptorSet(app->device, app->descriptorPool, &app->descriptorSetLayout);

		// Update descriptor set for this material
		VkDescriptorBufferInfo bufferInfo = {
		    .buffer = app->uniformBuffer.vkbuffer,
		    .offset = 0,
		    .range = sizeof(UniformBufferObject)};

		VkDescriptorImageInfo baseColorImageInfo = {
		    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		    .imageView = app->baseColorTextures[i].view,
		    .sampler = app->baseColorTextures[i].sampler};

		VkDescriptorImageInfo metallicRoughnessImageInfo = {
		    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		    .imageView = app->metallicRoughnessTextures[i].view,
		    .sampler = app->metallicRoughnessTextures[i].sampler};

		VkDescriptorImageInfo emissiveImageInfo = {
		    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		    .imageView = app->emissiveTextures[i].view,
		    .sampler = app->emissiveTextures[i].sampler};

		VkDescriptorBufferInfo materialBufferInfo = {
			.buffer = app->materialUniformBuffers[i].vkbuffer,
		    .offset = 0,
			.range = sizeof(MaterialGPU)};

		VkWriteDescriptorSet descriptorWrites[] = {
		    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = app->descriptorSets[i], .dstBinding = 0, .dstArrayElement = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .pBufferInfo = &bufferInfo},
		    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = app->descriptorSets[i], .dstBinding = 1, .dstArrayElement = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .pImageInfo = &baseColorImageInfo},
		    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = app->descriptorSets[i], .dstBinding = 2, .dstArrayElement = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .pImageInfo = &metallicRoughnessImageInfo},
		    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = app->descriptorSets[i], .dstBinding = 3, .dstArrayElement = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .pImageInfo = &emissiveImageInfo},
		    {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = app->descriptorSets[i], .dstBinding = 4, .dstArrayElement = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .pBufferInfo = &materialBufferInfo},
		};

		vkUpdateDescriptorSets(app->device, ARRAYSIZE(descriptorWrites), descriptorWrites, 0, NULL);
	}

	// Legacy single descriptor set support (use first texture)
	app->descriptorSet = app->descriptorSets[0];
}

void createComputeDescriptorSetLayout(Application* app)
{
	// Storage image binding (binding = 0)
	VkDescriptorSetLayoutBinding storageImageBinding = {
	    .binding = 0,
	    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
	    .descriptorCount = 1,
	    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
	// Uniform buffer binding (binding = 1)
	VkDescriptorSetLayoutBinding uniformBufferBinding = {
	    .binding = 1,
	    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	    .descriptorCount = 1,
	    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
	VkDescriptorSetLayoutBinding bindings[] = {storageImageBinding, uniformBufferBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	    .bindingCount = 2,
	    .pBindings = bindings};
	VK_CHECK(vkCreateDescriptorSetLayout(app->device, &layoutInfo, NULL, &app->compute.descLayout));
}

void createSkyboxDescriptors(Application* app)
{
	// Requires: app->descriptorPool, app->skyboxDescriptorSetLayout, app->skyboxUniformBuffer, app->skyboxTexture
	app->skyboxDescriptorSet = allocateDescriptorSet(app->device, app->descriptorPool, &app->skyboxDescriptorSetLayout);

	VkDescriptorBufferInfo skyboxBufferInfo = {
		.buffer = app->skyboxUniformBuffer.vkbuffer,
		.offset = 0,
		.range = sizeof(UniformBufferObject),
	};

	VkDescriptorImageInfo skyboxImageInfo = {
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.imageView = app->skyboxTexture.view,
		.sampler = app->skyboxTexture.sampler,
	};

	VkWriteDescriptorSet skyboxDescriptorWrites[] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = app->skyboxDescriptorSet,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.pBufferInfo = &skyboxBufferInfo,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = app->skyboxDescriptorSet,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.pImageInfo = &skyboxImageInfo,
		},
	};

	vkUpdateDescriptorSets(app->device, ARRAYSIZE(skyboxDescriptorWrites), skyboxDescriptorWrites, 0, NULL);
}
