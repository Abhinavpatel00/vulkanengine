#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define _USE_MATH_DEFINE
#include <math.h>

#include "../external/stb/stb_ds.h"
#define VK_NO_PROTOTYPES

#define GLFW_INCLUDE_VULKAN

#include "../external/stb/stb_image.h"

#include <GLFW/glfw3.h>

#include "../external/cgltf/cgltf.h"

#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>

#include "../external/fast_obj/fast_obj.h"
#include "../external/volk/volk.h"

#include "../external/cglm/include/cglm/cglm.h"


// Nuklear GUI includes

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>

#include "tinytypes.h"
#define VK_CHECK(call) \
	do \
	{\
		VkResult result_ = call; \
		if (result_ != VK_SUCCESS) \
		{\
			fprintf(stderr, "Vulkan call failed in %s:%d:\n  → %s (%d)\n", \
			    __FILE__, __LINE__, vk_result_to_string(result_), result_); \
			assert(result_ == VK_SUCCESS); \
		} \
	} while (0)

#ifndef ARRAYSIZE
#define ARRAYSIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

// --- Core Structures ---
//
// static inline char* strdup(const char* s) {
//     size_t len = strlen(s) + 1;
//     char* copy = (char*)malloc(len);
//     if (copy) {
//         memcpy(copy, s, len);
//     }
//     return copy;
// }
typedef struct
{
	vec3 pos;
	vec3 normal;
	vec2 texcoord;
	vec4 color; // 👈 Per-vertex color from material
} Vertex;

typedef struct Buffer
{
	VkBuffer vkbuffer;
	VkDeviceMemory memory;
	void* data;
	size_t size;
} Buffer;

typedef struct Texture
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkSampler sampler;
} Texture;
typedef struct StorageImage
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFormat format; // VK_FORMAT_R32G32B32A32_SFLOAT
	VkExtent2D extent;
} StorageImage;

typedef struct Material
{
	vec4 base_color;    // Base color factor
	int has_texture;    // Whether this material has a texture
	int texture_index;  // Index into the textures array (-1 if no texture)
	char* texture_path; // Path to texture file
	float alpha_cutoff; // Alpha cutoff value for alpha testing
	int alpha_mode;     // 0 = opaque, 1 = mask, 2 = blend
} Material;

typedef struct Primitive
{
	u32 first_index;    // Starting index in the index buffer
	u32 index_count;    // Number of indices for this primitive
	int material_index; // Index into the materials array
} Primitive;

#define MAX_POINT_LIGHTS 8

typedef struct PointLight
{
	vec3 position;
	float _pad1; // Padding for alignment
	vec3 color;
	float intensity;
	float constant; // Attenuation factors
	float linear;
	float quadratic;
	float _pad2; // Padding for alignment
} PointLight;

typedef struct DirectionalLight
{
	vec3 direction;
	float _pad1; // Padding
	vec3 color;
	float intensity;
} DirectionalLight;

typedef struct UniformBufferObject
{
	mat4 proj;
	mat4 view;
	mat4 model;
	vec3 cameraPos;
	u32 numLights;
	PointLight lights[MAX_POINT_LIGHTS];
	DirectionalLight dirLight;
} UniformBufferObject;

typedef struct Mesh
{
	Vertex* vertices;
	u32* indices;
	u32 vertex_count;
	u32 index_count;

	// Per-material texture support
	Material* materials;
	u32 material_count;
	Primitive* primitives;
	u32 primitive_count;

	// Legacy fields (kept for compatibility)
	char* texture_path; // from glTF material texture
	vec4 base_color;    // from glTF material baseColorFactor
	int has_texture;    // 1 if texture used, else 0
} Mesh;

typedef struct ComputePipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkDescriptorSetLayout descLayout;
	VkShaderModule shaderModule;
	VkDescriptorPool descPool;
} ComputePipeline;

typedef struct ComputeUniforms
{
	vec3 mouse_position;
	int is_additive; // bool in GLSL is 4 bytes, we use int (4 bytes) to match
	vec2 path_mask_ws_dims;
} ComputeUniforms;

typedef struct Particle
{
	vec4 pos; // x, y, vx, vy
} Particle;

#define MAX_FRAMES_IN_FLIGHT 2

typedef struct Application
{
	GLFWwindow* window;
	int width, height;
	bool framebufferResized;

	// Camera
	vec3 cameraPos;
	vec3 cameraFront;
	vec3 cameraUp;
	float yaw;
	float pitch;
	float lastX;
	float lastY;
	bool firstMouse;
	float deltaTime;
	float lastFrame;

	// Vulkan core
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	VkCommandPool commandPool;
	VkCommandBuffer* commandBuffers;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkSurfaceKHR surface;

	// Swapchain
	VkSwapchainKHR swapchain;
	VkFormat swapchainFormat;
	VkColorSpaceKHR swapchainColorSpace;
	VkImage* swapchainImages;
	VkImageView* swapchainImageViews;
	u32 swapchainImageCount;
	// Track current layouts of swapchain images
	VkImageLayout* swapchainImageLayouts;

	// Depth buffer
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;
	VkFormat depthFormat;

	// Command pool and buffers
	Buffer baseColorBuffer;
	Buffer hasTextureBuffer;
	Buffer alphaCutoffBuffer;

	// Pipeline
	//	VkRenderPass renderPass;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	// Particle simulation
	ComputePipeline particleCompute;
	Buffer particleBuffer;
	VkDescriptorSet particleComputeDescSet;
	VkPipeline particlePipeline;
	VkPipelineLayout particlePipelineLayout;
	VkDescriptorSetLayout particleGraphicsDescriptorSetLayout;
	VkDescriptorSet particleGraphicsDescriptorSet;

	// Resources
	Mesh mesh;
	Buffer vertexBuffer;
	Buffer indexBuffer;

	// Multiple textures support
	Texture* textures;               // Array of textures
	u32 texture_count;               // Number of textures
	VkDescriptorSet* descriptorSets; // One descriptor set per texture

	// Legacy single texture support (kept for compatibility)
	Texture texture;
	u32 mipLevels;
	Buffer uniformBuffer;

	// Descriptors
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;
	VkPhysicalDeviceMemoryProperties memProperties;

	// Lighting
	PointLight lights[MAX_POINT_LIGHTS];
	u32 numActiveLights;
	DirectionalLight dirLight;

	// Sync objects
	VkSemaphore ImageAquireSemaphore[MAX_FRAMES_IN_FLIGHT]; // Per frame in flight
	VkSemaphore* imageReleaseSemaphore;                     // Per swapchain image (present wait)
	VkSemaphore* sceneDoneSemaphores;                       // Per swapchain image (signaled after main rendering)
	VkCommandBuffer* presentCmdBuffers;                     // Per swapchain image (transition to PRESENT)
	VkSemaphore* presentReadySemaphores;                    // Per swapchain image (signaled after post-NK present transition)
	VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];           // Per frame in flight
	u32 currentFrame;
	StorageImage computeImage;
	ComputePipeline compute;
	VkCommandBuffer computeCmdBuffer;
	VkFence computeFence;
	VkDescriptorSet computeDescSet;
	Buffer computeUniformBuffer;

	// Skybox
	Texture skyboxTexture;
	VkDescriptorSet skyboxDescriptorSet;
	VkDescriptorSetLayout skyboxDescriptorSetLayout;
	VkPipeline skyboxPipeline;
	VkPipelineLayout skyboxPipelineLayout;
	Buffer skyboxVertexBuffer;
	Buffer skyboxUniformBuffer;

	// FPS tracking
	double fpsLastTime;
	int fpsFrameCount;
	float fps;

	// Nuklear UI context
	struct nk_context* nkCtx;
	bool is_ui_mode;
} Application;

void createSkyboxTexture(Application* app);

// --- Compute ---

void createComputeSync(Application* app);
void updateStorageImage(Application* app, StorageImage* img, float* data);
void clearStorageImage(Application* app, StorageImage* img);
void createComputePipeline(Application* app, ComputePipeline* compute, const char* shaderPath, VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount);
void createComputeDescriptors(Application* app, ComputePipeline* compute, VkDescriptorSet* descriptorSet, VkDescriptorPoolSize* poolSizes, uint32_t poolSizeCount, VkWriteDescriptorSet* descriptorWrites, uint32_t descriptorWriteCount);
void createComputeDescriptorSetLayout(Application* app);
void recordComputeCommands(Application* app);
void updateComputeUniforms(Application* app, vec3 mousePos, int is_additive, vec2 path_mask_ws_dims);

// Vulkan Core Setup
VkInstance createVulkanInstance(void);
VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window);
VkPhysicalDevice selectPhysicalDevice(VkInstance instance);
u32 find_graphics_queue_family_index(VkPhysicalDevice pickedPhysicalDevice);
VkDevice create_logical_device(VkPhysicalDevice pickedPhysicalDevice, u32 queueFamilyIndex);

// Memory and Buffers
VkSemaphore createSemaphore(VkDevice device);
uint32_t findMemoryType(const VkPhysicalDeviceMemoryProperties* memProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties);
u32 selectmemorytype(VkPhysicalDeviceMemoryProperties* memprops, u32 memtypeBits, VkFlags requirements_mask);
void createBuffer(Application* app, Buffer* buffer, VkDeviceSize size, VkBufferUsageFlags usage);
void destroyBuffer(VkDevice device, Buffer* buffer);
Buffer createStagingBuffer(Application* app, const void* data, VkDeviceSize size);
void copyBufferToDeviceLocal(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkBuffer src, VkBuffer dst, VkDeviceSize size);
// Swapchain
VkSwapchainKHR createSwapchain(Application* app);
void createSwapchainViews(Application* app);
void createSwapchainRelatedResources(Application* app);
void cleanupSwapchain(Application* app);
void recreateSwapchain(Application* app);

// Textures and Samplers
void createTextureImage(Application* app, const char* path, Texture* outTexture, u32* outMipLevels);
void generateMipmaps(Application* app, VkCommandBuffer cmd, VkImage image, VkFormat format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
void createTextureSampler(Application* app, Texture* texture, u32 mipLevels);
void updateBaseColorAndHasTexture(Application* app);
void createTextureResources(Application* app);

// Rendering and Command Buffers
void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkImageAspectFlags aspectMask);

VkCommandBuffer beginSingleTimeCommands(Application* app);
void endSingleTimeCommands(Application* app, VkCommandBuffer commandBuffer);
void recordCommandBuffer(Application* app, VkCommandBuffer commandBuffer, u32 imageIndex);

void recordComputeCommands(Application* app);
void drawFrame(Application* app);
void createPipeline(Application* app);
VkPipeline createMeshPipeline(Application* app, VkShaderModule vertShader, VkShaderModule fragShader);
VkPipeline createParticlePipeline(Application* app, VkShaderModule vertShader, VkShaderModule fragShader);
VkPipeline createBrickPipeline(Application* app, VkShaderModule vertShader, VkShaderModule fragShader);
VkPipeline createTerrainPipeline(Application* app, VkShaderModule vertShader, VkShaderModule fragShader);

// Bloom system functions
void createBloomSystem(Application* app);
void createBloomRenderTargets(Application* app);
void createBloomPipelines(Application* app);
void createBloomDescriptors(Application* app);
void renderBloomPass(Application* app, VkCommandBuffer cmd);
// Descriptors and Uniforms
VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device);
VkDescriptorPool createDescriptorPool(VkDevice device);
VkDescriptorSet allocateDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);
void createDescriptors(Application* app);
void createUniformBuffers(Application* app);
// Models and GLTF
void ProcessGltfNode(cgltf_node* node, Mesh* outMesh, cgltf_data* data, mat4 parentTransform,
    Application* app, uint32_t* vertexOffset, uint32_t* indexOffset, uint32_t* primitiveIndex);
void checkMaterials(cgltf_data* data);
void loadGltfModel(const char* path, Mesh* outMesh);
void createModelAndBuffers(Application* app);
// Depth and Shaders
VkShaderModule LoadShaderModule(const char* filepath, VkDevice device);
VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
void createDepthResources(Application* app);
// Synchronization
void createSyncObjects(Application* app);
void cleanAcquiresemaphore_and_fences(Application* app);
// Command Pool
void createCommandPoolAndBuffer(Application* app, u32 queueFamilyIndex);

// Window and Events
void initWindow(Application* app);
void processInput(Application* app);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
// Application Lifecycle
void initVulkan(Application* app);
void createResources(Application* app);
void updateLights(Application* app);
void mainLoop(Application* app);
void cleanupResources(Application* app);
void cleanupPipeline(Application* app);
void cleanupComputePipeline(Application* app, ComputePipeline* compute);
void cleanup(Application* app);

void createStorageImage(Application* app, StorageImage* img, uint32_t width, uint32_t height);
void createComputePipeline(Application* app, ComputePipeline* compute, const char* shaderPath, VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount);
void createComputeDescriptors(Application* app, ComputePipeline* compute, VkDescriptorSet* descriptorSet, VkDescriptorPoolSize* poolSizes, uint32_t poolSizeCount, VkWriteDescriptorSet* descriptorWrites, uint32_t descriptorWriteCount);


void createSkyboxPipeline(Application* app);

int main(void);
