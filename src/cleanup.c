#include "main.h"
void cleanAcquiresemaphore_and_fences(Application* app)
{

	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(app->device, app->ImageAquireSemaphore[i], NULL);
	}

	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroyFence(app->device, app->inFlightFences[i], NULL);
	}
}

void cleanupResources(Application* app)
{
	destroyBuffer(app->device, &app->uniformBuffer);
	destroyBuffer(app->device, &app->indexBuffer);
	destroyBuffer(app->device, &app->vertexBuffer);
	destroyBuffer(app->device, &app->baseColorBuffer);
	destroyBuffer(app->device, &app->hasTextureBuffer);
	destroyBuffer(app->device, &app->alphaCutoffBuffer);
	destroyBuffer(app->device, &app->skyboxUniformBuffer);
	destroyBuffer(app->device, &app->particleBuffer);
	destroyBuffer(app->device, &app->computeUniformBuffer);

	// Clean up multiple textures
	if (app->baseColorTextures)
	{
		for (u32 i = 0; i < app->texture_count; i++)
		{
			vkDestroySampler(app->device, app->baseColorTextures[i].sampler, NULL);
			vkDestroyImageView(app->device, app->baseColorTextures[i].view, NULL);
			vkDestroyImage(app->device, app->baseColorTextures[i].image, NULL);
			vkFreeMemory(app->device, app->baseColorTextures[i].memory, NULL);
		}
		free(app->baseColorTextures);
	}
	if (app->metallicRoughnessTextures)
	{
		for (u32 i = 0; i < app->texture_count; i++)
		{
			vkDestroySampler(app->device, app->metallicRoughnessTextures[i].sampler, NULL);
			vkDestroyImageView(app->device, app->metallicRoughnessTextures[i].view, NULL);
			vkDestroyImage(app->device, app->metallicRoughnessTextures[i].image, NULL);
			vkFreeMemory(app->device, app->metallicRoughnessTextures[i].memory, NULL);
		}
		free(app->metallicRoughnessTextures);
	}
	if (app->emissiveTextures)
	{
		for (u32 i = 0; i < app->texture_count; i++)
		{
			vkDestroySampler(app->device, app->emissiveTextures[i].sampler, NULL);
			vkDestroyImageView(app->device, app->emissiveTextures[i].view, NULL);
			vkDestroyImage(app->device, app->emissiveTextures[i].image, NULL);
			vkFreeMemory(app->device, app->emissiveTextures[i].memory, NULL);
		}
		free(app->emissiveTextures);
	}

	vkDestroyDescriptorPool(app->device, app->descriptorPool, NULL);

	// Clean up descriptor sets array
	if (app->descriptorSets)
	{
		free(app->descriptorSets);
	}

	// Clean up mesh data
	free(app->mesh.vertices);
	free(app->mesh.indices);
	

	// Clean up primitives
	free(app->mesh.primitives);
}

void cleanupPipeline(Application* app)
{
	for (u32 i = 0; i < app->mesh.material_count; ++i)
	{
		vkDestroyPipeline(app->device, app->pipelines[i], NULL);
	}
	free(app->pipelines);
	vkDestroyPipelineLayout(app->device, app->pipelineLayout, NULL);
	vkDestroyShaderModule(app->device, app->fragShaderModule, NULL);
	vkDestroyShaderModule(app->device, app->vertShaderModule, NULL);
	vkDestroyDescriptorSetLayout(app->device, app->descriptorSetLayout, NULL);
}

void destroyBuffer(VkDevice device, Buffer* buffer)
{
	if (buffer->data)
	{
		vkUnmapMemory(device, buffer->memory);
		buffer->data = NULL;
	}
	if (buffer->vkbuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(device, buffer->vkbuffer, NULL);
		buffer->vkbuffer = VK_NULL_HANDLE;
	}
	if (buffer->memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, buffer->memory, NULL);
		buffer->memory = VK_NULL_HANDLE;
	}
}

void cleanupComputePipeline(Application* app, ComputePipeline* compute)
{
    vkDestroyPipeline(app->device, compute->pipeline, NULL);
    vkDestroyPipelineLayout(app->device, compute->layout, NULL);
    vkDestroyDescriptorSetLayout(app->device, compute->descLayout, NULL);
    vkDestroyShaderModule(app->device, compute->shaderModule, NULL);
    vkDestroyDescriptorPool(app->device, compute->descPool, NULL);
}
