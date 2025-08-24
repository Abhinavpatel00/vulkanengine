#include "main.h"
void createSkyboxTexture(Application* app) {
    // Vulkan cubemap face order: +X, -X, +Y, -Y, +Z, -Z
    const char* faces[6] = {
        "data/skybox/xpos.png",  // Right  (+X)
        "data/skybox/xneg.png",  // Left   (-X)  
        "data/skybox/ypos.png",  // Top    (+Y)
        "data/skybox/yneg.png",  // Bottom (-Y)
        "data/skybox/zpos.png",  // Front  (+Z)
        "data/skybox/zneg.png"   // Back   (-Z)
    };

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels[6];
    VkDeviceSize imageSize = 0;
    VkDeviceSize layerSize = 0;
stbi_set_flip_vertically_on_load(false);

    for (int i = 0; i < 6; i++) {
        pixels[i] = stbi_load(faces[i], &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels[i]) {
            fprintf(stderr, "Failed to load skybox texture: %s\n", faces[i]);
            exit(1);
        }
        if (i == 0) {
            layerSize = texWidth * texHeight * 4;
        }
        imageSize += layerSize;
    }

    Buffer stagingBuffer;
    createBuffer(app, &stagingBuffer, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    for (int i = 0; i < 6; i++) {
        memcpy((char*)stagingBuffer.data + (layerSize * i), pixels[i], layerSize);
        stbi_image_free(pixels[i]);
    }

    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = texWidth,
        .extent.height = texHeight,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 6,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
    };

    VK_CHECK(vkCreateImage(app->device, &imageInfo, NULL, &app->skyboxTexture.image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(app->device, app->skyboxTexture.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = selectmemorytype(&app->memoryProperties, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    VK_CHECK(vkAllocateMemory(app->device, &allocInfo, NULL, &app->skyboxTexture.memory));
    VK_CHECK(vkBindImageMemory(app->device, app->skyboxTexture.image, app->skyboxTexture.memory, 0));

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(app);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = app->skyboxTexture.image,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 6,
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

    VkBufferImageCopy regions[6];
    for (int i = 0; i < 6; i++) {
        regions[i] = (VkBufferImageCopy){
            .bufferOffset = layerSize * i,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .imageSubresource.mipLevel = 0,
            .imageSubresource.baseArrayLayer = i,
            .imageSubresource.layerCount = 1,
            .imageOffset = {0, 0, 0},
            .imageExtent = {(u32)texWidth, (u32)texHeight, 1},
        };
    }
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.vkbuffer, app->skyboxTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);

    VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = app->skyboxTexture.image,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 6,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier2);


    endSingleTimeCommands(app, commandBuffer);

    destroyBuffer(app->device, &stagingBuffer);

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = app->skyboxTexture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 6,
    };
    VK_CHECK(vkCreateImageView(app->device, &viewInfo, NULL, &app->skyboxTexture.view));

    VkSamplerCreateInfo samplerInfo = {
	    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
	    .magFilter = VK_FILTER_LINEAR,
	    .minFilter = VK_FILTER_LINEAR,
	    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	    .anisotropyEnable = VK_FALSE,
	    .maxAnisotropy = 1.0f,
	    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
	    .unnormalizedCoordinates = VK_FALSE,
	    .compareEnable = VK_FALSE,
	    .compareOp = VK_COMPARE_OP_ALWAYS,
	    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
	    .mipLodBias = 0.0f,
	    .minLod = 0.0f,
	    .maxLod = 1.0f,
	};
	VK_CHECK(vkCreateSampler(app->device, &samplerInfo, NULL, &app->skyboxTexture.sampler));
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
