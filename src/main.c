
#define STB_DS_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#define FAST_OBJ_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_VULKAN_IMPLEMENTATION
#include "../external/nuklear/nuklear.h"
#include "main.h"
#include "nuklear_glfw_vulkan.h"
// Update image data (replaces update())
// // Update image data (replaces update())

// --- Command Buffer Helpers ---

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

// decriptors are defined to use by shader  , desciptorSet are used to group resources, layout is used  to define the resources
//  Push are limited to a certain number of bindings, but are synchronized with the frame. Set can be huge, but are not synchronized with the frame (command buffer).
// . Multiple sets can be
// bound to a pipeline at a tim
//
// set layout is passed to pipeline layout while inialising a pipeline
// --- Vulkan Helpers ---

void createDepthResources(Application* app)
{
	app->depthFormat = findDepthFormat(app->physicalDevice);

	VkImageCreateInfo imageInfo = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
	    .imageType = VK_IMAGE_TYPE_2D,
	    .format = app->depthFormat,
	    .extent = {app->width, app->height, 1},
	    .mipLevels = 1,
	    .arrayLayers = 1,
	    .samples = VK_SAMPLE_COUNT_1_BIT,
	    .tiling = VK_IMAGE_TILING_OPTIMAL,
	    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

	VK_CHECK(vkCreateImage(app->device, &imageInfo, NULL, &app->depthImage));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(app->device, app->depthImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {
	    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
	    .allocationSize = memRequirements.size,
	    .memoryTypeIndex = selectmemorytype(&app->memoryProperties, memRequirements.memoryTypeBits,
	        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

	VK_CHECK(vkAllocateMemory(app->device, &allocInfo, NULL, &app->depthImageMemory));
	VK_CHECK(vkBindImageMemory(app->device, app->depthImage, app->depthImageMemory, 0));

	VkImageViewCreateInfo viewInfo = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,

	    .image = app->depthImage,
	    .viewType = VK_IMAGE_VIEW_TYPE_2D,
	    .format = app->depthFormat,

	    .subresourceRange = {
	        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
	        .baseMipLevel = 0,
	        .levelCount = 1,
	        .baseArrayLayer = 0,
	        .layerCount = 1}};

	VK_CHECK(vkCreateImageView(app->device, &viewInfo, NULL, &app->depthImageView));
}
void createSwapchainRelatedResources(Application* app)
{
	// Create swapchain
	app->swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
	app->swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	app->swapchain = createSwapchain(app);

	// Get swapchain images
	VK_CHECK(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->swapchainImageCount, NULL));
	app->swapchainImages = malloc(app->swapchainImageCount * sizeof(VkImage));
	VK_CHECK(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->swapchainImageCount, app->swapchainImages));
	createSwapchainViews(app);

	// Track swapchain image layouts
	app->swapchainImageLayouts = malloc(app->swapchainImageCount * sizeof(VkImageLayout));
	for (u32 i = 0; i < app->swapchainImageCount; ++i) {
		app->swapchainImageLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	// Create depth resources
	createDepthResources(app);

	app->imageReleaseSemaphore = malloc(app->swapchainImageCount * sizeof(VkSemaphore));
	app->sceneDoneSemaphores   = malloc(app->swapchainImageCount * sizeof(VkSemaphore));
	app->presentReadySemaphores= malloc(app->swapchainImageCount * sizeof(VkSemaphore));
	// Allocate per-image present command buffers
	app->presentCmdBuffers = malloc(app->swapchainImageCount * sizeof(VkCommandBuffer));
	VkCommandBufferAllocateInfo presentAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = app->commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = app->swapchainImageCount,
	};
	VK_CHECK(vkAllocateCommandBuffers(app->device, &presentAllocInfo, app->presentCmdBuffers));
	for (u32 i = 0; i < app->swapchainImageCount; i++)
	{
		app->imageReleaseSemaphore[i] = createSemaphore(app->device);
		app->sceneDoneSemaphores[i]   = createSemaphore(app->device);
		app->presentReadySemaphores[i]= createSemaphore(app->device);
	}
}

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

void createModelAndBuffers(Application* app)
{
	// === Load model ===
	// loadGltfModel("/home/lka/myprojects/vulkantest3/sponza/Sponza.gltf", &app->mesh);
	//
	//
	loadGltfModel("data/scene.gltf", &app->mesh);

	// === Vertex buffer ===
	VkDeviceSize vertexSize = app->mesh.vertex_count * sizeof(Vertex);
	Buffer vertexStaging = createStagingBuffer(app, app->mesh.vertices, vertexSize);
	createBuffer(app, &app->vertexBuffer, vertexSize,
	    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	copyBufferToDeviceLocal(app->device, app->commandPool, app->graphicsQueue,
	    vertexStaging.vkbuffer, app->vertexBuffer.vkbuffer, vertexSize);
	destroyBuffer(app->device, &vertexStaging);

	// === Index buffer ===
	VkDeviceSize indexSize = app->mesh.index_count * sizeof(u32);
	Buffer indexStaging = createStagingBuffer(app, app->mesh.indices, indexSize);
	createBuffer(app, &app->indexBuffer, indexSize,
	    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	copyBufferToDeviceLocal(app->device, app->commandPool, app->graphicsQueue,
	    indexStaging.vkbuffer, app->indexBuffer.vkbuffer, indexSize);
	destroyBuffer(app->device, &indexStaging);

	// === Skybox vertex buffer ===
	float skyboxVertices[] = {
	    // positions
	    -1.0f, 1.0f, -1.0f,
	    -1.0f, -1.0f, -1.0f,
	    1.0f, -1.0f, -1.0f,
	    1.0f, -1.0f, -1.0f,
	    1.0f, 1.0f, -1.0f,
	    -1.0f, 1.0f, -1.0f,

	    -1.0f, -1.0f, 1.0f,
	    -1.0f, -1.0f, -1.0f,
	    -1.0f, 1.0f, -1.0f,
	    -1.0f, 1.0f, -1.0f,
	    -1.0f, 1.0f, 1.0f,
	    -1.0f, -1.0f, 1.0f,

	    1.0f, -1.0f, -1.0f,
	    1.0f, -1.0f, 1.0f,
	    1.0f, 1.0f, 1.0f,
	    1.0f, 1.0f, 1.0f,
	    1.0f, 1.0f, -1.0f,
	    1.0f, -1.0f, -1.0f,

	    -1.0f, -1.0f, 1.0f,
	    -1.0f, 1.0f, 1.0f,
	    1.0f, 1.0f, 1.0f,
	    1.0f, 1.0f, 1.0f,
	    1.0f, -1.0f, 1.0f,
	    -1.0f, -1.0f, 1.0f,

	    -1.0f, 1.0f, -1.0f,
	    1.0f, 1.0f, -1.0f,
	    1.0f, 1.0f, 1.0f,
	    1.0f, 1.0f, 1.0f,
	    -1.0f, 1.0f, 1.0f,
	    -1.0f, 1.0f, -1.0f,

	    -1.0f, -1.0f, -1.0f,
	    -1.0f, -1.0f, 1.0f,
	    1.0f, -1.0f, -1.0f,
	    1.0f, -1.0f, -1.0f,
	    -1.0f, -1.0f, 1.0f,
	    1.0f, -1.0f, 1.0f};

	VkDeviceSize skyboxVertexSize = sizeof(skyboxVertices);
	Buffer skyboxVertexStaging = createStagingBuffer(app, skyboxVertices, skyboxVertexSize);
	createBuffer(app, &app->skyboxVertexBuffer, skyboxVertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	copyBufferToDeviceLocal(app->device, app->commandPool, app->graphicsQueue, skyboxVertexStaging.vkbuffer, app->skyboxVertexBuffer.vkbuffer, skyboxVertexSize);
	destroyBuffer(app->device, &skyboxVertexStaging);
}

void createTextureResources(Application* app)
{
	// Count unique textures from materials
	app->texture_count = 0;
	for (u32 i = 0; i < app->mesh.material_count; i++)
	{
		if (app->mesh.materials[i].has_texture)
		{
			app->texture_count++;
		}
	}

	// Always have at least one texture (fallback)
	if (app->texture_count == 0)
	{
		app->texture_count = 1;
	}

	// Allocate texture array
	app->textures = calloc(app->texture_count, sizeof(Texture));

	// Load textures and assign indices to materials
	u32 textureIndex = 0;
	for (u32 i = 0; i < app->mesh.material_count; i++)
	{
		if (app->mesh.materials[i].has_texture && app->mesh.materials[i].texture_path)
		{
			u32 mipLevels;
			createTextureImage(app, app->mesh.materials[i].texture_path, &app->textures[textureIndex], &mipLevels);
			createTextureSampler(app, &app->textures[textureIndex], mipLevels);
			app->mesh.materials[i].texture_index = textureIndex;
			textureIndex++;
		}
	}

	// Fallback texture if no materials have textures
	if (textureIndex == 0)
	{
		u32 mipLevels;
		createTextureImage(app, app->mesh.texture_path ? app->mesh.texture_path : "Bark_DeadTree.png", &app->textures[0], &mipLevels);
		createTextureSampler(app, &app->textures[0], mipLevels);
	}

	// Legacy single texture support
	if (app->texture_count > 0)
	{
		app->texture = app->textures[0];
		app->mipLevels = 1; // Default mip levels for legacy support
	}
}

void createUniformBuffers(Application* app)
{
	createBuffer(app, &app->uniformBuffer, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	createBuffer(app, &app->baseColorBuffer, sizeof(vec4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	createBuffer(app, &app->hasTextureBuffer, sizeof(int), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	createBuffer(app, &app->alphaCutoffBuffer, sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	createBuffer(app, &app->skyboxUniformBuffer, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

// --- Vulkan Initialization Helpers ---

void createCommandPoolAndBuffer(Application* app, u32 queueFamilyIndex)
{
	// Create command pool
	VkCommandPoolCreateInfo commandPoolInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
	    .queueFamilyIndex = queueFamilyIndex,
	    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
	};
	VK_CHECK(vkCreateCommandPool(app->device, &commandPoolInfo, NULL, &app->commandPool));

	// Allocate command buffers (one per frame in flight)
	app->commandBuffers = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkCommandBuffer));
	VkCommandBufferAllocateInfo cmdAllocInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	    .commandPool = app->commandPool,
	    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	};
	VK_CHECK(vkAllocateCommandBuffers(app->device, &cmdAllocInfo, app->commandBuffers));

	// Allocate a dedicated compute command buffer (from the same pool)
	VkCommandBufferAllocateInfo computeAllocInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	    .commandPool = app->commandPool,
	    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	    .commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(app->device, &computeAllocInfo, &app->computeCmdBuffer));
}

void createSkyboxPipeline(Application* app)
{
	VkDescriptorSetLayoutBinding bindings[] = {
	    {
	        .binding = 0,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	    },
	    {
	        .binding = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	    },
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	    .bindingCount = ARRAYSIZE(bindings),
	    .pBindings = bindings,
	};
	VK_CHECK(vkCreateDescriptorSetLayout(app->device, &layoutInfo, NULL, &app->skyboxDescriptorSetLayout));

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	    .setLayoutCount = 1,
	    .pSetLayouts = &app->skyboxDescriptorSetLayout,
	};
	VK_CHECK(vkCreatePipelineLayout(app->device, &pipelineLayoutInfo, NULL, &app->skyboxPipelineLayout));

	VkShaderModule vertShader = LoadShaderModule("compiledshaders/skybox.vert.spv", app->device);
	VkShaderModule fragShader = LoadShaderModule("compiledshaders/skybox.frag.spv", app->device);

	VkPipelineShaderStageCreateInfo shaderStages[] = {
	    {
	        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	        .stage = VK_SHADER_STAGE_VERTEX_BIT,
	        .module = vertShader,
	        .pName = "main",
	    },
	    {
	        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	        .module = fragShader,
	        .pName = "main",
	    },
	};

	VkVertexInputBindingDescription bindingDescription = {
	    .binding = 0,
	    .stride = sizeof(float) * 3,
	    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription attributeDescription = {
	    .binding = 0,
	    .location = 0,
	    .format = VK_FORMAT_R32G32B32_SFLOAT,
	    .offset = 0,
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	    .vertexBindingDescriptionCount = 1,
	    .pVertexBindingDescriptions = &bindingDescription,
	    .vertexAttributeDescriptionCount = 1,
	    .pVertexAttributeDescriptions = &attributeDescription,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	    .primitiveRestartEnable = VK_FALSE,
	};

	VkPipelineViewportStateCreateInfo viewportState = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	    .viewportCount = 1,
	    .scissorCount = 1,
	};

	VkPipelineRasterizationStateCreateInfo rasterizer = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	    .depthClampEnable = VK_FALSE,
	    .rasterizerDiscardEnable = VK_FALSE,
	    .polygonMode = VK_POLYGON_MODE_FILL,
	    .lineWidth = 1.0f,
	    // Cull front faces to render the cube from the inside without overlap
	    .cullMode = VK_CULL_MODE_FRONT_BIT,
	    // Our projection flips Y, which flips winding; use CLOCKWISE to keep expected facing
	    .frontFace = VK_FRONT_FACE_CLOCKWISE,
	    .depthBiasEnable = VK_FALSE,
	};

	VkPipelineMultisampleStateCreateInfo multisampling = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	    .sampleShadingEnable = VK_FALSE,
	    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencil = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
	    .depthTestEnable = VK_TRUE,
	    .depthWriteEnable = VK_FALSE,
	    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
	    .depthBoundsTestEnable = VK_FALSE,
	    .stencilTestEnable = VK_FALSE,
	};

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {
	    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	    .blendEnable = VK_FALSE,
	};

	VkPipelineColorBlendStateCreateInfo colorBlending = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	    .logicOpEnable = VK_FALSE,
	    .attachmentCount = 1,
	    .pAttachments = &colorBlendAttachment,
	};

	VkDynamicState dynamicStates[] = {
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	    .dynamicStateCount = ARRAYSIZE(dynamicStates),
	    .pDynamicStates = dynamicStates,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo = {
	    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
	    .stageCount = 2,
	    .pStages = shaderStages,
	    .pVertexInputState = &vertexInputInfo,
	    .pInputAssemblyState = &inputAssembly,
	    .pViewportState = &viewportState,
	    .pRasterizationState = &rasterizer,
	    .pMultisampleState = &multisampling,
	    .pDepthStencilState = &depthStencil,
	    .pColorBlendState = &colorBlending,
	    .pDynamicState = &dynamicState,
	    .layout = app->skyboxPipelineLayout,
	    .renderPass = NULL,
	    .subpass = 0,
	};

	VkPipelineRenderingCreateInfo renderingCreateInfo = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	    .colorAttachmentCount = 1,
	    .pColorAttachmentFormats = &app->swapchainFormat,
	    .depthAttachmentFormat = app->depthFormat,
	};
	pipelineInfo.pNext = &renderingCreateInfo;

	VK_CHECK(vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &app->skyboxPipeline));

	vkDestroyShaderModule(app->device, vertShader, NULL);
	vkDestroyShaderModule(app->device, fragShader, NULL);
}
void createPipeline(Application* app)
{
	// Create descriptor set layout
	app->descriptorSetLayout = createDescriptorSetLayout(app->device);

	// Create pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	    .setLayoutCount = 1,
	    .pSetLayouts = &app->descriptorSetLayout,
	};
	VK_CHECK(vkCreatePipelineLayout(app->device, &pipelineLayoutInfo, NULL, &app->pipelineLayout));

	// Load shaders and create main pipeline
	app->vertShaderModule = LoadShaderModule("compiledshaders/tri.vert.spv", app->device);
	app->fragShaderModule = LoadShaderModule("compiledshaders/tri.frag.spv", app->device);
	app->pipeline = createMeshPipeline(app, app->vertShaderModule, app->fragShaderModule);
}

void createResources(Application* app)
{
	createModelAndBuffers(app);
	createTextureResources(app);

	createUniformBuffers(app);
	updateBaseColorAndHasTexture(app);

	createDescriptors(app);
}

// --- Vulkan Cleanup Helpers ---

// --- Main Application ---

void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	(void)width;
	(void)height;
	Application* app = glfwGetWindowUserPointer(window);
	app->framebufferResized = true;
}

void initWindow(Application* app)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	app->width = 1280;
	app->height = 720;
	app->window = glfwCreateWindow(app->width, app->height, "Vulkan Test", 0, 0);
	glfwSetWindowUserPointer(app->window, app);
	glfwSetFramebufferSizeCallback(app->window, framebufferResizeCallback);
	glfwSetCursorPosCallback(app->window, mouse_callback);
	glfwSetInputMode(app->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Initialize camera - positioned to see the ground plane
	glm_vec3_copy((vec3){0.0f, 111.0f, 10.0f}, app->cameraPos); // Higher up and back
	glm_vec3_copy((vec3){0.0f, 0.0f, -1.0f}, app->cameraFront);
	glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, app->cameraUp);
	app->yaw = -90.0f;
	app->pitch = -20.0f; // Look down slightly to see the ground
	app->lastX = app->width / 2.0f;
	app->lastY = app->height / 2.0f;
	app->firstMouse = true;
	app->deltaTime = 0.0f;
	app->lastFrame = 0.0f;

	// // Initialize point lights
	// app->numActiveLights = 4;
	//
	// // Light 1 - Red light
	// glm_vec3_copy((vec3){2.0f, 1.0f, 0.0f}, app->lights[0].position);
	// glm_vec3_copy((vec3){1.0f, 0.3f, 0.3f}, app->lights[0].color);
	// app->lights[0].intensity = 1.0f;
	// app->lights[0].constant = 1.0f;
	// app->lights[0].linear = 0.09f;
	// app->lights[0].quadratic = 0.032f;
	//
	// // Light 2 - Green light
	// glm_vec3_copy((vec3){-2.0f, 1.0f, 0.0f}, app->lights[1].position);
	// glm_vec3_copy((vec3){0.3f, 1.0f, 0.3f}, app->lights[1].color);
	// app->lights[1].intensity = 1.0f;
	// app->lights[1].constant = 1.0f;
	// app->lights[1].linear = 0.09f;
	// app->lights[1].quadratic = 0.032f;
	//
	// // Light 3 - Blue light
	// glm_vec3_copy((vec3){0.0f, 1.0f, 2.0f}, app->lights[2].position);
	// glm_vec3_copy((vec3){0.3f, 0.3f, 1.0f}, app->lights[2].color);
	// app->lights[2].intensity = 1.0f;
	// app->lights[2].constant = 1.0f;
	// app->lights[2].linear = 0.09f;
	// app->lights[2].quadratic = 0.032f;
	//
	// // Light 4 - White light
	// glm_vec3_copy((vec3){0.0f, 1.0f, -2.0f}, app->lights[3].position);
	// glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, app->lights[3].color);
	// app->lights[3].intensity = 1.0f;
	// app->lights[3].constant = 1.0f;
	// app->lights[3].linear = 0.09f;
	// app->lights[3].quadratic = 0.032f;
	//
	// Initialize directional light
	// glm_vec3_copy((vec3){-0.2f, -1.0f, -0.3f}, app->dirLight.direction);
	// glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, app->dirLight.color);
	// app->dirLight.intensity = 0.5f;
	//

	// Direction remains the same
	glm_vec3_copy((vec3){-0.2f, -1.0f, -0.3f}, app->dirLight.direction);
	glm_vec3_copy((vec3){1.0f, 1.0f, 0.0f}, app->dirLight.color);
	app->dirLight.intensity = 10.0f;
}

void initVulkan(Application* app)
{
	//
	// ctx = nk_glfw3_init(
	//         demo.win, demo.device, demo.physical_device, demo.indices.graphics,
	//         demo.overlay_image_views, demo.swap_chain_images_len,
	//         demo.swap_chain_image_format, NK_GLFW3_INSTALL_CALLBACKS,
	//         MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
	//
	//
	VK_CHECK(volkInitialize());

	// Initialize frame count first
	app->currentFrame = 0;

	// Create instance
	app->instance = createVulkanInstance();
	volkLoadInstance(app->instance);

	// Select physical device
	app->physicalDevice = selectPhysicalDevice(app->instance);
	vkGetPhysicalDeviceMemoryProperties(app->physicalDevice, &app->memoryProperties);

	// Create logical device and queue
	u32 graphicsqueueFamilyIndex = find_graphics_queue_family_index(app->physicalDevice);
	app->device = create_logical_device(app->physicalDevice, graphicsqueueFamilyIndex);
	volkLoadDevice(app->device);
	vkGetDeviceQueue(app->device, graphicsqueueFamilyIndex, 0, &app->graphicsQueue);

	createCommandPoolAndBuffer(app, graphicsqueueFamilyIndex);

	// Create surface
	app->surface = createSurface(app->instance, app->window);

	createSwapchainRelatedResources(app);

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024

	// Store the nuklear context in the app struct
	app->nkCtx = nk_glfw3_init(app->window, app->device, app->physicalDevice,
	    graphicsqueueFamilyIndex, app->swapchainImageViews, app->swapchainImageCount,
	    app->swapchainFormat, NK_GLFW3_INSTALL_CALLBACKS, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);

	{
		struct nk_font_atlas* atlas;
		nk_glfw3_font_stash_begin(&atlas);
		/*struct nk_font *droid = nk_font_atlas_add_from_file(atlas,
		 * "../../../extra_font/DroidSans.ttf", 14, 0);*/
		/*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas,
		 * "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
		/*struct nk_font *future = nk_font_atlas_add_from_file(atlas,
		 * "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
		/*struct nk_font *clean = nk_font_atlas_add_from_file(atlas,
		 * "../../../extra_font/ProggyClean.ttf", 12, 0);*/
		/*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas,
		 * "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
		/*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas,
		 * "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
		nk_glfw3_font_stash_end(app->graphicsQueue);
		/*nk_style_load_all_cursors(ctx, atlas->cursors);*/
	/*nk_style_set_font(ctx, &droid->handle);*/}
	
	// Initialize FPS tracking
	app->fpsLastTime = glfwGetTime();
	app->fpsFrameCount = 0;
	app->fps = 0.0f;
	
	createPipeline(app);
	createSkyboxPipeline(app);
	createSkyboxTexture(app);
	createResources(app);
	createSyncObjects(app);

	// Create particle buffer
	const int PARTICLE_COUNT = 1024;
	createBuffer(app, &app->particleBuffer, sizeof(Particle) * PARTICLE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	// Initialize particle data
	Particle* particles = (Particle*)malloc(sizeof(Particle) * PARTICLE_COUNT);
	for (int i = 0; i < PARTICLE_COUNT; i++)
	{
		particles[i].pos[0] = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
		particles[i].pos[1] = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
		particles[i].pos[2] = 0.0f;
		particles[i].pos[3] = 1.0f;
	}
	memcpy(app->particleBuffer.data, particles, sizeof(Particle) * PARTICLE_COUNT);
	free(particles);

	// Create particle compute pipeline
	VkDescriptorSetLayoutBinding particleBindings[] = {
	    {
	        .binding = 0,
	        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	    },
	    {
	        .binding = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	    }};
	createComputePipeline(app, &app->particleCompute, "compiledshaders/particle.comp.spv", particleBindings, 2);

	// Create particle compute descriptors
	VkDescriptorPoolSize particlePoolSizes[] = {
	    {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1},
	    {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
	};

	VkDescriptorBufferInfo particleBufferInfo = {
	    .buffer = app->particleBuffer.vkbuffer,
	    .offset = 0,
	    .range = sizeof(Particle) * PARTICLE_COUNT,
	};

	VkDescriptorBufferInfo uniformBufferInfo = {
	    .buffer = app->uniformBuffer.vkbuffer, // Note: using the main uniform buffer for now
	    .offset = 0,
	    .range = sizeof(UniformBufferObject),
	};

	VkWriteDescriptorSet particleDescriptorWrites[] = {
	    {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .dstBinding = 0,
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        .pBufferInfo = &particleBufferInfo,
	    },
	    {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .dstBinding = 1,
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .pBufferInfo = &uniformBufferInfo,
	    },
	};

	createComputeDescriptors(app, &app->particleCompute, &app->particleComputeDescSet, particlePoolSizes, 2, particleDescriptorWrites, 2);

	// Create particle graphics descriptor set layout
	VkDescriptorSetLayoutBinding particleGraphicsBindings[] = {
	    {
	        .binding = 0,
	        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	    },
	};
	VkDescriptorSetLayoutCreateInfo particleGraphicsLayoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	    .bindingCount = 1,
	    .pBindings = particleGraphicsBindings,
	};
	VK_CHECK(vkCreateDescriptorSetLayout(app->device, &particleGraphicsLayoutInfo, NULL, &app->particleGraphicsDescriptorSetLayout));

	// Create particle graphics pipeline layout
	VkPipelineLayoutCreateInfo particlePipelineLayoutInfo = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	    .setLayoutCount = 1,
	    .pSetLayouts = &app->particleGraphicsDescriptorSetLayout,
	};
	VK_CHECK(vkCreatePipelineLayout(app->device, &particlePipelineLayoutInfo, NULL, &app->particlePipelineLayout));

	// Create particle graphics descriptor set
	app->particleGraphicsDescriptorSet = allocateDescriptorSet(app->device, app->descriptorPool, app->particleGraphicsDescriptorSetLayout);

	VkDescriptorBufferInfo particleGraphicsBufferInfo = {
	    .buffer = app->particleBuffer.vkbuffer,
	    .offset = 0,
	    .range = sizeof(Particle) * 1024,
	};

	VkWriteDescriptorSet particleGraphicsDescriptorWrite = {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstSet = app->particleGraphicsDescriptorSet,
	    .dstBinding = 0,
	    .descriptorCount = 1,
	    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	    .pBufferInfo = &particleGraphicsBufferInfo,
	};
	vkUpdateDescriptorSets(app->device, 1, &particleGraphicsDescriptorWrite, 0, NULL);

	VkShaderModule particleVertShader = LoadShaderModule("compiledshaders/particle.vert.spv", app->device);
	VkShaderModule particleFragShader = LoadShaderModule("compiledshaders/particle.frag.spv", app->device);
	app->particlePipeline = createParticlePipeline(app, particleVertShader, particleFragShader);
	vkDestroyShaderModule(app->device, particleVertShader, NULL);
	vkDestroyShaderModule(app->device, particleFragShader, NULL);

	// Create the other compute pipeline
	createStorageImage(app, &app->computeImage, 512, 512);
	createBuffer(app, &app->computeUniformBuffer, sizeof(ComputeUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	VkDescriptorSetLayoutBinding bindings[] = {
	    {
	        .binding = 0,
	        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	    },
	    {
	        .binding = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .descriptorCount = 1,
	        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	    }};

	createComputePipeline(app, &app->compute, "compiledshaders/compute_path_mask.comp.spv", bindings, 2);

	VkDescriptorPoolSize poolSizes[] = {
	    {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1},
	    {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
	};

	VkDescriptorImageInfo imageInfo = {
	    .imageView = app->computeImage.view,
	    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	};

	VkDescriptorBufferInfo bufferInfo = {
	    .buffer = app->computeUniformBuffer.vkbuffer,
	    .offset = 0,
	    .range = sizeof(ComputeUniforms),
	};

	VkWriteDescriptorSet descriptorWrites[] = {
	    {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .dstBinding = 0,
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
	        .pImageInfo = &imageInfo,
	    },
	    {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .dstBinding = 1,
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .pBufferInfo = &bufferInfo,
	    },
	};

	createComputeDescriptors(app, &app->compute, &app->computeDescSet, poolSizes, 2, descriptorWrites, 2);

	createComputeSync(app);
}

void recordComputeCommands(Application* app)
{
	// Ensure the image is in GENERAL layout for compute writing
	VkImageMemoryBarrier barrier = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	    .srcAccessMask = 0,
	    .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
	    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, // We don't care about previous content
	    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
	    .image = app->computeImage.image,
	    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
	vkCmdPipelineBarrier(
	    app->computeCmdBuffer,
	    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	    0,
	    0, NULL,
	    0, NULL,
	    1, &barrier);
	// Bind the compute pipeline and descriptor set
	vkCmdBindPipeline(app->computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, app->compute.pipeline);
	vkCmdBindDescriptorSets(app->computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, app->compute.layout, 0, 1, &app->computeDescSet, 0, NULL);
	// Dispatch with one workgroup per pixel (since local size is 1x1)
	vkCmdDispatch(app->computeCmdBuffer, app->computeImage.extent.width, app->computeImage.extent.height, 1);
	// If we plan to use the image in the fragment shader, we transition it to SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier readBarrier = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
	    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
	    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
	    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	    .image = app->computeImage.image,
	    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
	vkCmdPipelineBarrier(
	    app->computeCmdBuffer,
	    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	    0,
	    0, NULL,
	    0, NULL,
	    1, &readBarrier);
}

void updateComputeUniforms(Application* app, vec3 mousePos, int is_additive, vec2 path_mask_ws_dims)
{
	ComputeUniforms uniforms;
	glm_vec3_copy(mousePos, uniforms.mouse_position);
	uniforms.is_additive = is_additive;
	glm_vec2_copy(path_mask_ws_dims, uniforms.path_mask_ws_dims);
	memcpy(app->computeUniformBuffer.data, &uniforms, sizeof(ComputeUniforms));
}

void recordParticleComputeCommands(Application* app)
{
	// Bind the particle compute pipeline and descriptor set
	vkCmdBindPipeline(app->computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, app->particleCompute.pipeline);
	vkCmdBindDescriptorSets(app->computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, app->particleCompute.layout, 0, 1, &app->particleComputeDescSet, 0, NULL);

	// Dispatch the compute shader
	vkCmdDispatch(app->computeCmdBuffer, 1024, 1, 1);

	VkMemoryBarrier memoryBarrier = {
	    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
	    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
	    .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
	};
	vkCmdPipelineBarrier(
	    app->computeCmdBuffer,
	    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
	    0,
	    1, &memoryBarrier,
	    0, NULL,
	    0, NULL);
}
void recordCommandBuffer(Application* app, VkCommandBuffer commandBuffer, u32 imageIndex)
{
	VkCommandBufferBeginInfo beginInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	// Transition image layout for color attachment from its current layout
	VkImageLayout currentLayout = app->swapchainImageLayouts ? app->swapchainImageLayouts[imageIndex] : VK_IMAGE_LAYOUT_UNDEFINED;
	transitionImageLayout(commandBuffer, app->swapchainImages[imageIndex],
						  currentLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						  0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						  VK_IMAGE_ASPECT_COLOR_BIT);
    
    // Transition depth image layout
    transitionImageLayout(commandBuffer, app->depthImage, 
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 
                         0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR, 
                         VK_IMAGE_ASPECT_DEPTH_BIT);


	VkRenderingAttachmentInfo colorAttachment = {
	    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	    .imageView = app->swapchainImageViews[imageIndex],
	    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
	    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	    .clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}},
	};

	VkRenderingAttachmentInfo depthAttachment = {
	    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	    .imageView = app->depthImageView,
	    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
	    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
	    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    .clearValue.depthStencil = {1.0f, 0},
	};

	VkRenderingInfo renderingInfo = {
	    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, {app->width, app->height}},
	    .layerCount = 1,
	    .colorAttachmentCount = 1,
	    .pColorAttachments = &colorAttachment,
	    .pDepthAttachment = &depthAttachment,
	};

	vkCmdBeginRendering(commandBuffer, &renderingInfo);

	// Update camera & lights (before any draw so skybox uses current frame matrices)
	UniformBufferObject ubo = {0};
	glm_perspective(glm_rad(45.0f), app->width / (float)app->height, 0.01f, 1000.0f, ubo.proj);
	ubo.proj[1][1] *= -1;
	vec3 center;
	glm_vec3_add(app->cameraPos, app->cameraFront, center);
	glm_lookat(app->cameraPos, center, app->cameraUp, ubo.view);
	glm_mat4_identity(ubo.model);
	glm_vec3_copy(app->cameraPos, ubo.cameraPos);
	ubo.numLights = app->numActiveLights;
	for (u32 i = 0; i < app->numActiveLights; i++)
		ubo.lights[i] = app->lights[i];
	ubo.dirLight = app->dirLight;
	memcpy(app->uniformBuffer.data, &ubo, sizeof(ubo));

	// Update skybox uniform buffer (vertex shader removes translation)
	// Update skybox uniform buffer with view matrix without translation
	UniformBufferObject skyboxUbo = {0};
	glm_mat4_copy(ubo.proj, skyboxUbo.proj); // Same projection matrix
	glm_mat4_copy(ubo.view, skyboxUbo.view); // Copy the view matrix
	glm_mat4_copy(ubo.model, skyboxUbo.model);
	// Remove translation from the view matrix (set translation column to 0)
	skyboxUbo.view[3][0] = 0.0f;
	skyboxUbo.view[3][1] = 0.0f;
	skyboxUbo.view[3][2] = 0.0f;
	glm_mat4_identity(skyboxUbo.model);
	memcpy(app->skyboxUniformBuffer.data, &skyboxUbo, sizeof(skyboxUbo));

	// Draw skybox
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->skyboxPipeline);

	VkViewport skyboxViewport = {.x = 0.0f, .y = 0.0f, .width = (float)app->width, .height = (float)app->height, .minDepth = 0.0f, .maxDepth = 1.0f};
	vkCmdSetViewport(commandBuffer, 0, 1, &skyboxViewport);

	VkRect2D skyboxScissor = {{0, 0}, {app->width, app->height}};
	vkCmdSetScissor(commandBuffer, 0, 1, &skyboxScissor);

	VkBuffer skyboxVertexBuffers[] = {app->skyboxVertexBuffer.vkbuffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, skyboxVertexBuffers, offsets);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->skyboxPipelineLayout, 0, 1, &app->skyboxDescriptorSet, 0, NULL);
	vkCmdDraw(commandBuffer, 36, 1, 0, 0);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline);

	VkViewport viewport = {.x = 0.0f, .y = 0.0f, .width = (float)app->width, .height = (float)app->height, .minDepth = 0.0f, .maxDepth = 1.0f};
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {{0, 0}, {app->width, app->height}};
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// Bind vertex/index buffers
	VkBuffer vertexBuffers[] = {app->vertexBuffer.vkbuffer};
	VkDeviceSize modelOffsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, modelOffsets);
	vkCmdBindIndexBuffer(commandBuffer, app->indexBuffer.vkbuffer, 0, VK_INDEX_TYPE_UINT32);
	// Draw all primitives
	for (u32 i = 0; i < app->mesh.primitive_count; i++)
	{
		Primitive* prim = &app->mesh.primitives[i];
		if (prim->material_index >= 0 && prim->material_index < (int)app->mesh.material_count)
		{
			Material* mat = &app->mesh.materials[prim->material_index];
			memcpy(app->baseColorBuffer.data, mat->base_color, sizeof(vec4));
			memcpy(app->hasTextureBuffer.data, &mat->has_texture, sizeof(int));
			memcpy(app->alphaCutoffBuffer.data, &mat->alpha_cutoff, sizeof(float));

			VkDescriptorSet descriptorSet = app->descriptorSet;
			if (mat->has_texture && mat->texture_index >= 0 && mat->texture_index < (int)app->texture_count)
				descriptorSet = app->descriptorSets[mat->texture_index];

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
		}
		else
		{
			vec4 defaultColor = {1.0f, 1.0f, 1.0f, 1.0f};
			int hasTexture = 0;
			float defaultAlphaCutoff = 0.0f;
			memcpy(app->baseColorBuffer.data, defaultColor, sizeof(vec4));
			memcpy(app->hasTextureBuffer.data, &hasTexture, sizeof(int));
			memcpy(app->alphaCutoffBuffer.data, &defaultAlphaCutoff, sizeof(float));
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipelineLayout, 0, 1, &app->descriptorSet, 0, NULL);
		}

		vkCmdDrawIndexed(commandBuffer, prim->index_count, 1, prim->first_index, 0, 0);
	}

	if (app->mesh.primitive_count == 0)
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipelineLayout, 0, 1, &app->descriptorSet, 0, NULL);
		vkCmdDrawIndexed(commandBuffer, app->mesh.index_count, 1, 0, 0, 0);
	}

	// Draw particles
	// vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app->particlePipeline);
	// VkBuffer particleVertexBuffers[] = {app->particleBuffer.vkbuffer};
	// VkDeviceSize particleOffsets[] = {0};
	// vkCmdBindVertexBuffers(commandBuffer, 0, 1, particleVertexBuffers, particleOffsets);
	// vkCmdDraw(commandBuffer, 1024, 1, 0, 0);
	//
	vkCmdEndRendering(commandBuffer);

	VkImageMemoryBarrier presentBarrier = {
	    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	    .dstAccessMask = 0,
	    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .image = app->swapchainImages[imageIndex],
	    .subresourceRange = {
	        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	        .baseMipLevel = 0,
	        .levelCount = 1,
	        .baseArrayLayer = 0,
	        .layerCount = 1,
	    },
	};

	vkCmdPipelineBarrier(
	    commandBuffer,
	    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
	    0,
	    0, NULL,
	    0, NULL,
	    1, &presentBarrier);

	// Track new layout as PRESENT for this image
	if (app->swapchainImageLayouts) {
		app->swapchainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}

	VK_CHECK(vkEndCommandBuffer(commandBuffer));
}
void drawFrame(Application* app)
{
	VK_CHECK(vkWaitForFences(app->device, 1, &app->inFlightFences[app->currentFrame], VK_TRUE, UINT64_MAX));

	u32 imageIndex;
	VkResult result = vkAcquireNextImageKHR(app->device, app->swapchain, UINT64_MAX, app->ImageAquireSemaphore[app->currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapchain(app);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		assert(0 && "failed to acquire swap chain image!");
	}

	VK_CHECK(vkResetFences(app->device, 1, &app->inFlightFences[app->currentFrame]));

	VkCommandBuffer commandBuffer = app->commandBuffers[app->currentFrame];
	VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));
	recordCommandBuffer(app, commandBuffer, imageIndex);


	// Build Nuklear UI
	nk_glfw3_new_frame();

	// FPS Widget
	if (nk_begin(app->nkCtx, "Performance", nk_rect(10, 10, 180, 60),
		NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE))
	{
		char fps_text[64];
		snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", app->fps);
		nk_layout_row_dynamic(app->nkCtx, 30, 1);
		nk_label(app->nkCtx, fps_text, NK_TEXT_LEFT);
	}
	nk_end(app->nkCtx);

	// Submit the main command buffer first; signal per-image sceneDone semaphore
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &app->ImageAquireSemaphore[app->currentFrame],
		.pWaitDstStageMask = (VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &app->sceneDoneSemaphores[imageIndex],
	};
	VK_CHECK(vkQueueSubmit(app->graphicsQueue, 1, &submitInfo, app->inFlightFences[app->currentFrame]));

	// Render Nuklear UI waiting on sceneDone; returns NK's own completion semaphore
	VkSemaphore nk_done_semaphore = nk_glfw3_render(
		app->graphicsQueue, imageIndex, app->sceneDoneSemaphores[imageIndex], NK_ANTI_ALIASING_ON);

	// Optional: record a tiny cmd buffer to ensure final layout is PRESENT (no-op if already tracked)
	VkCommandBuffer presentCmd = app->presentCmdBuffers[imageIndex];
	VK_CHECK(vkResetCommandBuffer(presentCmd, 0));
	VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VK_CHECK(vkBeginCommandBuffer(presentCmd, &beginInfo));
	if (!app->swapchainImageLayouts || app->swapchainImageLayouts[imageIndex] != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
		transitionImageLayout(presentCmd, app->swapchainImages[imageIndex],
							  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
							  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
							  VK_IMAGE_ASPECT_COLOR_BIT);
		if (app->swapchainImageLayouts) app->swapchainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	VK_CHECK(vkEndCommandBuffer(presentCmd));

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSubmitInfo presentSubmit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &nk_done_semaphore,
		.pWaitDstStageMask = &waitStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &presentCmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &app->presentReadySemaphores[imageIndex],
	};
	VK_CHECK(vkQueueSubmit(app->graphicsQueue, 1, &presentSubmit, VK_NULL_HANDLE));

	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &app->presentReadySemaphores[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &app->swapchain,
		.pImageIndices = &imageIndex,
	};
	result = vkQueuePresentKHR(app->graphicsQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || app->framebufferResized)
	{
		app->framebufferResized = false;
		recreateSwapchain(app);
	}
	else if (result != VK_SUCCESS)
	{
		assert(0 && "failed to present swap chain image!");
	}

	app->currentFrame = (app->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
void mainLoop(Application* app)
{
	while (!glfwWindowShouldClose(app->window))
	{
		float currentFrame = glfwGetTime();
		app->deltaTime = currentFrame - app->lastFrame;
		app->lastFrame = currentFrame;
		
		// Calculate FPS
		app->fpsFrameCount++;
		if (currentFrame - app->fpsLastTime >= 1.0) {
			app->fps = app->fpsFrameCount / (currentFrame - app->fpsLastTime);
			app->fpsFrameCount = 0;
			app->fpsLastTime = currentFrame;
		}

		glfwPollEvents();
		processInput(app);
		updateLights(app);

		// Execute compute shaders
		VkCommandBufferBeginInfo beginInfo = {
		    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		VK_CHECK(vkBeginCommandBuffer(app->computeCmdBuffer, &beginInfo));

		recordComputeCommands(app);
		recordParticleComputeCommands(app);

		VK_CHECK(vkEndCommandBuffer(app->computeCmdBuffer));

		VkSubmitInfo computeSubmit = {
		    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		    .commandBufferCount = 1,
		    .pCommandBuffers = &app->computeCmdBuffer,
		};
		vkQueueSubmit(app->graphicsQueue, 1, &computeSubmit, app->computeFence);

		// Wait for compute to finish before rendering
		vkWaitForFences(app->device, 1, &app->computeFence, VK_TRUE, UINT64_MAX);
		vkResetFences(app->device, 1, &app->computeFence);

		drawFrame(app);
	}

	vkDeviceWaitIdle(app->device);
}

void cleanupSwapchain(Application* app)
{
	vkDestroyImageView(app->device, app->depthImageView, NULL);
	vkDestroyImage(app->device, app->depthImage, NULL);
	vkFreeMemory(app->device, app->depthImageMemory, NULL);

	for (u32 i = 0; i < app->swapchainImageCount; i++)
	{
		vkDestroyImageView(app->device, app->swapchainImageViews[i], NULL);
	}
	free(app->swapchainImageViews);
	free(app->swapchainImages);

	vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
	// Clean up per-image semaphores (they are tied to swapchain images)
	if (app->imageReleaseSemaphore)
	{
		for (u32 i = 0; i < app->swapchainImageCount; i++)
		{
			vkDestroySemaphore(app->device, app->imageReleaseSemaphore[i], NULL);
		}
		free(app->imageReleaseSemaphore);
		app->imageReleaseSemaphore = NULL;
	}
	if (app->sceneDoneSemaphores)
	{
		for (u32 i = 0; i < app->swapchainImageCount; i++)
		{
			vkDestroySemaphore(app->device, app->sceneDoneSemaphores[i], NULL);
		}
		free(app->sceneDoneSemaphores);
		app->sceneDoneSemaphores = NULL;
	}
	if (app->presentReadySemaphores)
	{
		for (u32 i = 0; i < app->swapchainImageCount; i++)
		{
			vkDestroySemaphore(app->device, app->presentReadySemaphores[i], NULL);
		}
		free(app->presentReadySemaphores);
		app->presentReadySemaphores = NULL;
	}
	if (app->swapchainImageLayouts)
	{
		free(app->swapchainImageLayouts);
		app->swapchainImageLayouts = NULL;
	}
}

void recreateSwapchain(Application* app)
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(app->window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(app->window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(app->device);

	// Destroy pipeline before swapchain resources (like render pass)
	vkDestroyPipeline(app->device, app->pipeline, NULL);
	cleanupSwapchain(app);
	// Destroy Nuklear device before swapchain recreation (keeps nk_context alive)
	nk_glfw3_device_destroy();
	app->width = width;
	app->height = height;

	createSwapchainRelatedResources(app);
	app->pipeline = createMeshPipeline(app, app->vertShaderModule, app->fragShaderModule);

	// Recreate Nuklear device with new swapchain image views and framebuffer size
	{
		u32 graphicsqueueFamilyIndex = find_graphics_queue_family_index(app->physicalDevice);
		nk_glfw3_device_create(
			app->device, app->physicalDevice,
			graphicsqueueFamilyIndex,
			app->swapchainImageViews, app->swapchainImageCount,
			app->swapchainFormat,
			512 * 1024, 128 * 1024,
			(uint32_t)app->width, (uint32_t)app->height);

		// Re-upload font atlas for Nuklear
		struct nk_font_atlas* atlas = NULL;
		nk_glfw3_font_stash_begin(&atlas);
		nk_glfw3_font_stash_end(app->graphicsQueue);
	}

	// Make sure Nuklear knows the new framebuffer size
	nk_glfw3_resize((uint32_t)app->width, (uint32_t)app->height);
}

void cleanup(Application* app)
{
	vkDeviceWaitIdle(app->device);

	// Clean up nuklear
	nk_glfw3_shutdown();

	cleanAcquiresemaphore_and_fences(app);
	cleanupResources(app);
	cleanupPipeline(app);
	cleanupComputePipeline(app, &app->compute);
	cleanupComputePipeline(app, &app->particleCompute);
	vkDestroyPipeline(app->device, app->skyboxPipeline, NULL);
	vkDestroyPipelineLayout(app->device, app->skyboxPipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(app->device, app->skyboxDescriptorSetLayout, NULL);
	vkDestroySampler(app->device, app->skyboxTexture.sampler, NULL);
	vkDestroyImageView(app->device, app->skyboxTexture.view, NULL);
	vkDestroyImage(app->device, app->skyboxTexture.image, NULL);
	vkFreeMemory(app->device, app->skyboxTexture.memory, NULL);
	destroyBuffer(app->device, &app->skyboxVertexBuffer);
	vkDestroyPipeline(app->device, app->particlePipeline, NULL);
	vkDestroyPipelineLayout(app->device, app->particlePipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(app->device, app->particleGraphicsDescriptorSetLayout, NULL);
	vkDestroyImageView(app->device, app->computeImage.view, NULL);
	vkDestroyImage(app->device, app->computeImage.image, NULL);
	vkFreeMemory(app->device, app->computeImage.memory, NULL);
	cleanupSwapchain(app);

	vkDestroySurfaceKHR(app->instance, app->surface, NULL);
	free(app->commandBuffers);
	vkDestroyCommandPool(app->device, app->commandPool, NULL);
	vkDestroyDevice(app->device, NULL);
	vkDestroyInstance(app->instance, NULL);

	glfwDestroyWindow(app->window);
	glfwTerminate();
}

int main(void)
{
	Application app = {0};
	initWindow(&app);
	initVulkan(&app);
	mainLoop(&app);
	cleanup(&app);
	return 0;
}

// --- Input Handling ---
void processInput(Application* app)
{
	float cameraSpeed = 20.5f * app->deltaTime;
	if (glfwGetKey(app->window, GLFW_KEY_W) == GLFW_PRESS)
	{
		vec3 front;
		glm_vec3_scale(app->cameraFront, cameraSpeed, front);
		glm_vec3_add(app->cameraPos, front, app->cameraPos);
	}
	if (glfwGetKey(app->window, GLFW_KEY_S) == GLFW_PRESS)
	{
		vec3 front;
		glm_vec3_scale(app->cameraFront, cameraSpeed, front);
		glm_vec3_sub(app->cameraPos, front, app->cameraPos);
	}
	if (glfwGetKey(app->window, GLFW_KEY_A) == GLFW_PRESS)
	{
		vec3 right;
		glm_cross(app->cameraFront, app->cameraUp, right);
		glm_normalize(right);
		glm_vec3_scale(right, cameraSpeed, right);
		glm_vec3_sub(app->cameraPos, right, app->cameraPos);
	}
	if (glfwGetKey(app->window, GLFW_KEY_D) == GLFW_PRESS)
	{
		vec3 right;
		glm_cross(app->cameraFront, app->cameraUp, right);
		glm_normalize(right);
		glm_vec3_scale(right, cameraSpeed, right);
		glm_vec3_add(app->cameraPos, right, app->cameraPos);
	}
	if (glfwGetKey(app->window, GLFW_KEY_Q) == GLFW_PRESS)
	{
		vec3 up;
		glm_vec3_scale(app->cameraUp, cameraSpeed, up);
		glm_vec3_add(app->cameraPos, up, app->cameraPos);
	}
	if (glfwGetKey(app->window, GLFW_KEY_E) == GLFW_PRESS)
	{
		vec3 up;
		glm_vec3_scale(app->cameraUp, cameraSpeed, up);
		glm_vec3_sub(app->cameraPos, up, app->cameraPos);
	}
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	Application* app = glfwGetWindowUserPointer(window);

	if (app->firstMouse)
	{
		app->lastX = xpos;
		app->lastY = ypos;
		app->firstMouse = false;
	}

	float xoffset = xpos - app->lastX;
	float yoffset = app->lastY - ypos; // reversed since y-coordinates go from bottom to top
	app->lastX = xpos;
	app->lastY = ypos;

	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	app->yaw += xoffset;
	app->pitch += yoffset;

	if (app->pitch > 89.0f)
		app->pitch = 89.0f;
	if (app->pitch < -89.0f)
		app->pitch = -89.0f;

	vec3 front;
	front[0] = cos(glm_rad(app->yaw)) * cos(glm_rad(app->pitch));
	front[1] = sin(glm_rad(app->pitch));
	front[2] = sin(glm_rad(app->yaw)) * cos(glm_rad(app->pitch));
	glm_normalize(front);
	glm_vec3_copy(front, app->cameraFront);
}

void updateLights(Application* app)
{
	float time = glfwGetTime();

	// Animate light positions in a circular pattern
	float radius = 3.0f;

	// Light 1 - Red, circular motion in XZ plane
	app->lights[0].position[0] = cosf(time) * radius;
	app->lights[0].position[2] = sinf(time) * radius;

	// Light 2 - Green, circular motion in XZ plane (offset)
	app->lights[1].position[0] = cosf(time + 3.14159265f) * radius;
	app->lights[1].position[2] = sinf(time + 3.14159265f) * radius;

	// Light 3 - Blue, up and down motion
	app->lights[2].position[1] = 1.0f + sinf(time * 2.0f) * 1.5f;

	// Light 4 - White, diagonal circular motion
	app->lights[3].position[0] = cosf(time * 0.5f) * radius * 0.7f;
	app->lights[3].position[1] = 1.0f + sinf(time * 0.7f) * 1.0f;
	app->lights[3].position[2] = sinf(time * 0.5f) * radius * 0.7f;

	// Animate directional light
	app->dirLight.direction[0] = sinf(time * 2.0f);
	app->dirLight.direction[2] = cosf(time * 2.0f);
}
