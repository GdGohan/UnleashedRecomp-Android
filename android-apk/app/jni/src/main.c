#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define TAG "smoketest"

static void test_mmap_4gb(void)
{
    void *addr = (void *)0x100000000ull;
    void *result = mmap(addr, 0x100000000ull, PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

    if (result == MAP_FAILED) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                      "[%s] mmap(0x100000000, 4GB) FAILED: %s (errno=%d)",
                      TAG, strerror(errno), errno);
        return;
    }

    if (result != addr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                     "[%s] mmap succeeded but returned a DIFFERENT address: got %p, wanted %p",
                     TAG, result, addr);
    } else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                     "[%s] mmap(0x100000000, 4GB) OK, got exact address %p", TAG, result);
    }

    munmap(result, 0x100000000ull);
}

/* Extension requirements mirrored from plume_vulkan.cpp (RequiredDeviceExtensions /
 * OptionalDeviceExtensions), so this smoke test reports plume-relevant support directly. */
static const char *kRequiredDeviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
    VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
};

static const char *kOptionalDeviceExtensions[] = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME,
    VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME,
    VK_KHR_PRESENT_ID_EXTENSION_NAME,
    VK_KHR_PRESENT_WAIT_EXTENSION_NAME,
    VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
};

static int has_extension(VkExtensionProperties *props, uint32_t count, const char *name)
{
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(props[i].extensionName, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_vulkan(SDL_Window *window)
{
    if (SDL_Vulkan_LoadLibrary(NULL) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_Vulkan_LoadLibrary failed: %s", TAG, SDL_GetError());
        return;
    }

    PFN_vkGetInstanceProcAddr getInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!getInstanceProcAddr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_Vulkan_GetVkGetInstanceProcAddr failed", TAG);
        return;
    }

    unsigned int extCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extCount, NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_Vulkan_GetInstanceExtensions (count) failed: %s", TAG, SDL_GetError());
        return;
    }
    const char **extNames = (const char **)malloc(sizeof(const char *) * extCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extCount, extNames);

    for (unsigned int i = 0; i < extCount; i++) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] required instance extension: %s", TAG, extNames[i]);
    }

    VkApplicationInfo appInfo = { 0 };
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "smoketest";
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instInfo = { 0 };
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = extCount;
    instInfo.ppEnabledExtensionNames = extNames;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&instInfo, NULL, &instance);
    free(extNames);

    if (res != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[%s] vkCreateInstance FAILED: VkResult=%d", TAG, res);
        return;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] vkCreateInstance OK", TAG);

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
    if (deviceCount == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[%s] vkEnumeratePhysicalDevices found 0 devices", TAG);
        vkDestroyInstance(instance, NULL);
        return;
    }

    VkPhysicalDevice *devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] found %u physical device(s)", TAG, deviceCount);

    for (uint32_t d = 0; d < deviceCount; d++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[d], &props);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                     "[%s] device[%u]: %s (driverVersion=%u.%u.%u, apiVersion=%u.%u.%u, type=%d)",
                     TAG, d, props.deviceName,
                     VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion),
                     VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion),
                     props.deviceType);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                     "[%s] device[%u] limits: maxImageDimension2D=%u maxBoundDescriptorSets=%u maxPushConstantsSize=%u maxSamplerAnisotropy=%.1f",
                     TAG, d, props.limits.maxImageDimension2D, props.limits.maxBoundDescriptorSets,
                     props.limits.maxPushConstantsSize, props.limits.maxSamplerAnisotropy);

        uint32_t devExtCount = 0;
        vkEnumerateDeviceExtensionProperties(devices[d], NULL, &devExtCount, NULL);
        VkExtensionProperties *devExts = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * devExtCount);
        vkEnumerateDeviceExtensionProperties(devices[d], NULL, &devExtCount, devExts);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] device[%u] exposes %u device extensions total", TAG, d, devExtCount);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] --- plume RequiredDeviceExtensions check ---", TAG);
        size_t reqN = sizeof(kRequiredDeviceExtensions) / sizeof(kRequiredDeviceExtensions[0]);
        int allRequiredOk = 1;
        for (size_t i = 0; i < reqN; i++) {
            int ok = has_extension(devExts, devExtCount, kRequiredDeviceExtensions[i]);
            if (!ok) allRequiredOk = 0;
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s]   %-52s %s", TAG, kRequiredDeviceExtensions[i], ok ? "SUPPORTED" : "MISSING");
        }
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] all REQUIRED extensions supported: %s", TAG, allRequiredOk ? "YES" : "NO");

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] --- plume OptionalDeviceExtensions check ---", TAG);
        size_t optN = sizeof(kOptionalDeviceExtensions) / sizeof(kOptionalDeviceExtensions[0]);
        for (size_t i = 0; i < optN; i++) {
            int ok = has_extension(devExts, devExtCount, kOptionalDeviceExtensions[i]);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s]   %-52s %s", TAG, kOptionalDeviceExtensions[i], ok ? "supported" : "not supported");
        }

        free(devExts);
    }

    free(devices);
    vkDestroyInstance(instance, NULL);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] vulkan test complete", TAG);
}

int main(int argc, char *argv[])
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] starting up", TAG);

    test_mmap_4gb();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_Init failed: %s", TAG, SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("smoketest", SDL_WINDOWPOS_UNDEFINED,
                                           SDL_WINDOWPOS_UNDEFINED, 800, 600,
                                           SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_CreateWindow failed: %s", TAG, SDL_GetError());
        return 1;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] window created OK, entering loop", TAG);

    test_vulkan(window);

    int running = 1;
    Uint32 frame = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        if (frame % 60 == 0) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] alive, frame=%u", TAG, frame);
        }
        frame++;

        SDL_Delay(16);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
