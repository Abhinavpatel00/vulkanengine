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
	if (app->textures)
	{
		for (u32 i = 0; i < app->texture_count; i++)
		{
			vkDestroySampler(app->device, app->textures[i].sampler, NULL);
			vkDestroyImageView(app->device, app->textures[i].view, NULL);
			vkDestroyImage(app->device, app->textures[i].image, NULL);
			vkFreeMemory(app->device, app->textures[i].memory, NULL);
		}
		free(app->textures);
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
	free(app->mesh.texture_path);

	// Clean up materials
	if (app->mesh.materials)
	{
		for (u32 i = 0; i < app->mesh.material_count; i++)
		{
			free(app->mesh.materials[i].texture_path);
		}
		free(app->mesh.materials);
	}

	// Clean up primitives
	free(app->mesh.primitives);
}

void cleanupPipeline(Application* app)
{
	vkDestroyPipeline(app->device, app->pipeline, NULL);
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
