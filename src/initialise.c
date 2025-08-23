#include "main.h"
#include <vulkan/vulkan_core.h>

VkInstance createVulkanInstance(void)
{
	VkApplicationInfo appInfo = {
	    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
	    .pApplicationName = "Vulkan Test",
	    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
	    .pEngineName = "No Engine",
	    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
	    .apiVersion = VK_API_VERSION_1_3,
	};

	VkInstanceCreateInfo createInfo = {
	    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
	    .pApplicationInfo = &appInfo,
	};

#ifdef _DEBUG
	const char* debugLayers[] = {"VK_LAYER_KHRONOS_validation"};
	createInfo.ppEnabledLayerNames = debugLayers;
	createInfo.enabledLayerCount = ARRAYSIZE(debugLayers);
#endif

	u32 glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	const char* extensions[16];
	assert(glfwExtensionCount < 15);

	for (u32 i = 0; i < glfwExtensionCount; ++i)
	{
		extensions[i] = glfwExtensions[i];
	}

	u32 extensionCount = glfwExtensionCount;

#ifndef NDEBUG
	extensions[extensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
#endif

	createInfo.ppEnabledExtensionNames = extensions;
	createInfo.enabledExtensionCount = extensionCount;

	VkInstance instance;
	VK_CHECK(vkCreateInstance(&createInfo, NULL, &instance));
	return instance;
}
VkPhysicalDevice selectPhysicalDevice(VkInstance instance)
{
	VkPhysicalDevice physicalDevices[8];
	u32 count = ARRAYSIZE(physicalDevices);
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, physicalDevices));

	VkPhysicalDevice selected = VK_NULL_HANDLE;

	for (u32 i = 0; i < count; ++i)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
		    props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		{
			printf("GPU%d: %s (%s)\n", i, props.deviceName,
			    props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete GPU" : "Integrated GPU");
			printf("  Vulkan API: %d.%d.%d\n",
			    VK_VERSION_MAJOR(props.apiVersion),
			    VK_VERSION_MINOR(props.apiVersion),
			    VK_VERSION_PATCH(props.apiVersion));
			printf("  Driver: %d.%d.%d\n",
			    VK_VERSION_MAJOR(props.driverVersion),
			    VK_VERSION_MINOR(props.driverVersion),
			    VK_VERSION_PATCH(props.driverVersion));

			// Prefer discrete GPU
			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				selected = physicalDevices[i];
			else if (!selected)
				selected = physicalDevices[i];
		}
	}

	if (!selected)
	{
		fprintf(stderr, "No suitable GPU found (integrated or discrete only).\n");
		exit(1);
	}

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(selected, &props);

	printf("\n=== SELECTED GPU ===\n");
	printf("Name: %s\n", props.deviceName);
	printf("Type: %s\n", props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete GPU" : "Integrated GPU");
	printf("Vendor ID: 0x%X\n", props.vendorID);
	printf("Device ID: 0x%X\n", props.deviceID);
	printf("Vulkan API: %d.%d.%d\n",
	    VK_VERSION_MAJOR(props.apiVersion),
	    VK_VERSION_MINOR(props.apiVersion),
	    VK_VERSION_PATCH(props.apiVersion));
	printf("Driver: %d.%d.%d\n",
	    VK_VERSION_MAJOR(props.driverVersion),
	    VK_VERSION_MINOR(props.driverVersion),
	    VK_VERSION_PATCH(props.driverVersion));
	printf("Max Texture Size: %d x %d\n",
	    props.limits.maxImageDimension2D,
	    props.limits.maxImageDimension2D);
	printf("Max Uniform Buffer Size: %u MB\n",
	    props.limits.maxUniformBufferRange / (1024 * 1024));
	printf("====================\n\n");

	return selected;
}
u32 find_graphics_queue_family_index(VkPhysicalDevice pickedPhysicalDevice)
{
	u32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pickedPhysicalDevice,
	    &queueFamilyCount, NULL);
	VkQueueFamilyProperties* queueFamilies =
	    malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(pickedPhysicalDevice,
	    &queueFamilyCount, queueFamilies);
	u32 queuefamilyIndex = UINT32_MAX;
	for (u32 i = 0; i < queueFamilyCount; ++i)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			queuefamilyIndex = i;
			break;
		}
	}
	assert(queuefamilyIndex != UINT32_MAX && "No suitable queue family found");
	free(queueFamilies);
	return queuefamilyIndex;
}
VkDevice create_logical_device(VkPhysicalDevice pickedPhysicaldevice, u32 queueFamilyIndex)
{
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {
	    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
	    .queueFamilyIndex = queueFamilyIndex,
	    .queueCount = 1,
	    .pQueuePriorities = &queuePriority,
	};

	const char* deviceExtensions[] = {
	    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	    VK_KHR_MAINTENANCE2_EXTENSION_NAME,
	    VK_KHR_MULTIVIEW_EXTENSION_NAME,
	    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
	    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME};

	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
	    .dynamicRendering = VK_TRUE,
	};

	VkPhysicalDeviceFeatures2 features2 = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
	    .features = {
	        .samplerAnisotropy = VK_TRUE,
	    },
	    .pNext = &dynamicRenderingFeatures,
	};

	VkDeviceCreateInfo deviceCreateInfo = {
	    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
	    .pNext = &features2,
	    .queueCreateInfoCount = 1,
	    .pQueueCreateInfos = &queueCreateInfo,
	    .enabledExtensionCount = ARRAYSIZE(deviceExtensions),
	    .ppEnabledExtensionNames = deviceExtensions,
	};

	VkDevice device;
	VK_CHECK(vkCreateDevice(pickedPhysicaldevice, &deviceCreateInfo, 0, &device));
	return device;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window)
{
	VkSurfaceKHR surface;
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
	VkWaylandSurfaceCreateInfoKHR surfacecreateInfo = {
	    .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
	    .display = glfwGetWaylandDisplay(),
	    .surface = glfwGetWaylandWindow(window),
	};
	VK_CHECK(vkCreateWaylandSurfaceKHR(instance, &surfacecreateInfo, 0, &surface));
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	VkXlibSurfaceCreateInfoKHR surfacecreateInfo = {
	    .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
	    .dpy = glfwGetX11Display(),
	    .window = glfwGetX11Window(window),
	};
	VK_CHECK(vkCreateXlibSurfaceKHR(instance, &surfacecreateInfo, 0, &surface));
#else
	fprintf(stderr, "No supported platform defined for Vulkan surface creation\n");
	exit(1);
#endif
	return surface;
}

VkSwapchainKHR createSwapchain(Application* app)
{
	u32 queueFamilyIndex = find_graphics_queue_family_index(app->physicalDevice);
	VkBool32 presentSupported = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
	    app->physicalDevice, queueFamilyIndex, app->surface, &presentSupported));
	assert(presentSupported);
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
	    app->physicalDevice, app->surface, &surfaceCapabilities));
	VkSwapchainCreateInfoKHR swapchainInfo = {
	    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
	    .surface = app->surface,
	    .minImageCount = surfaceCapabilities.minImageCount,
	    .imageFormat = app->swapchainFormat,
	    .imageColorSpace = app->swapchainColorSpace,
	    .imageExtent = {.width = app->width, .height = app->height},
	    .imageArrayLayers = 1,
	    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
	    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
	    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
	    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
	    .presentMode = VK_PRESENT_MODE_MAILBOX_KHR,
	    .clipped = VK_TRUE,
	    .queueFamilyIndexCount = 1,
	    .pQueueFamilyIndices = &queueFamilyIndex,
	};
	VkSwapchainKHR swapchain;
	VK_CHECK(vkCreateSwapchainKHR(app->device, &swapchainInfo, 0, &swapchain));
	return swapchain;
}
void createSwapchainViews(Application* app)
{
	app->swapchainImageViews = malloc(app->swapchainImageCount * sizeof(VkImageView));
	for (u32 i = 0; i < app->swapchainImageCount; ++i)
	{
		VkImageViewCreateInfo viewInfo = {
		    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		    .image = app->swapchainImages[i],
		    .viewType = VK_IMAGE_VIEW_TYPE_2D,
		    .format = app->swapchainFormat,
		    .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
		        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
		    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
		};
		VK_CHECK(vkCreateImageView(app->device, &viewInfo, NULL, &app->swapchainImageViews[i]));
	}
}




void createSyncObjects(Application* app)
{
	VkFenceCreateInfo fenceInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

	// Create imageAvailable semaphores (per frame in flight)
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		app->ImageAquireSemaphore[i] = createSemaphore(app->device);
	}

	// Create fences for each frame in flight
	for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VK_CHECK(vkCreateFence(app->device, &fenceInfo, NULL, &app->inFlightFences[i]));
	}
}
