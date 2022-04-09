#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan_hpp.h"
#include <iostream>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

vk::AllocationCallbacks* g_Allocator;
vk::UniqueInstance g_Instance;
vk::PhysicalDevice g_PhysicalDevice;
vk::UniqueDevice g_Device;
uint32_t g_QueueFamily = (uint32_t)-1;
vk::Queue g_Queue;
vk::UniqueDebugUtilsMessengerEXT g_DebugMessenger;
vk::UniquePipelineCache g_PipelineCache;
vk::UniqueDescriptorPool g_DescriptorPool;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static int g_MinImageCount = 2;
static bool g_SwapChainRebuild = false;

VKAPI_ATTR VkBool32 VKAPI_CALL debug_message(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                             VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
                                             void* pUserData)
{
    std::cerr << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

static void SetupVulkan(std::vector<const char*>& extensions)
{
    VkResult err;

    // Create Vulkan Instance
    {
        // Create Vulkan Instance
        vk::DynamicLoader dl;
        auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        std::vector layers{ "VK_LAYER_KHRONOS_validation" };

        vk::InstanceCreateInfo create_info = {};
        create_info.setPEnabledExtensionNames(extensions);
        create_info.setPEnabledLayerNames(layers);
        create_info.setPEnabledExtensionNames(extensions);
        create_info.setPEnabledLayerNames(layers);
        g_Instance = vk::createInstanceUnique(create_info);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*g_Instance);

        // Create debug messenger
        vk::DebugUtilsMessengerCreateInfoEXT debug_messanger_ci = {};
        debug_messanger_ci.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning);
        debug_messanger_ci.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
        debug_messanger_ci.setPfnUserCallback(&debug_message);
        g_DebugMessenger = g_Instance->createDebugUtilsMessengerEXTUnique(debug_messanger_ci);
    }

    // Select GPU
    {
        std::vector gpus = g_Instance->enumeratePhysicalDevices();

        // If a number >1 of GPUs got reported, find discrete GPU if present, or use first one available. This covers
        // most common cases (multi-gpu/integrated+dedicated graphics). Handling more complicated setups (multiple
        // dedicated GPUs) is out of scope of this sample.
        int use_gpu = 0;
        for (int i = 0; i < gpus.size(); i++) {
            vk::PhysicalDeviceProperties properties = gpus[i].getProperties();
            if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                use_gpu = i;
                break;
            }
        }

        g_PhysicalDevice = gpus[use_gpu];
    }

    // Select graphics queue family
    {
        std::vector queues = g_PhysicalDevice.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queues.size(); i++)
            if (queues[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                g_QueueFamily = i;
                break;
            }
        IM_ASSERT(g_QueueFamily != (uint32_t)-1);
    }

    // Create Logical Device (with 1 queue)
    {
        const std::vector device_extensions = { "VK_KHR_swapchain" };
        const float queuePriority = 1.0f;

        vk::DeviceQueueCreateInfo queue_info = {};
        queue_info.setQueueFamilyIndex(g_QueueFamily);
        queue_info.setQueueCount(1);
        queue_info.setPQueuePriorities(&queuePriority);

        vk::DeviceCreateInfo create_info = {};
        create_info.setQueueCreateInfos(queue_info);
        create_info.setPEnabledExtensionNames(device_extensions);
        g_Device = g_PhysicalDevice.createDeviceUnique(create_info);

        g_Queue = g_Device->getQueue(g_QueueFamily, 0);
    }

    // Create Descriptor Pool
    {
        std::vector<vk::DescriptorPoolSize> poolSizes{
            { vk::DescriptorType::eSampler, 1000 },
            { vk::DescriptorType::eCombinedImageSampler, 1000 },
            { vk::DescriptorType::eSampledImage, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eUniformTexelBuffer, 1000 },
            { vk::DescriptorType::eStorageTexelBuffer, 1000 },
            { vk::DescriptorType::eUniformBuffer, 1000 },
            { vk::DescriptorType::eStorageBuffer, 1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment, 1000 }
        };

        vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo.setPoolSizes(poolSizes);
        descriptorPoolCreateInfo.setMaxSets(1000 * poolSizes.size());
        descriptorPoolCreateInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
        g_DescriptorPool = g_Device->createDescriptorPoolUnique(descriptorPoolCreateInfo);
    }
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
    wd->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
    if (res != VK_TRUE) {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const std::vector<vk::Format> requestSurfaceImageFormat = { vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm };
    const vk::ColorSpaceKHR requestSurfaceColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
    std::vector<vk::PresentModeKHR> present_modes = { vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifo };
#else
    std::vector<vk::PresentModeKHR> present_modes = { vk::PresentModeKHR::eFifo };
#endif
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, present_modes);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(*g_Instance, g_PhysicalDevice, *g_Device, wd, g_QueueFamily, nullptr, width, height, g_MinImageCount);
}

static void CleanupVulkanWindow()
{
    ImGui_ImplVulkanH_DestroyWindow(*g_Instance, *g_Device, &g_MainWindowData, nullptr);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    vk::Semaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    vk::Semaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    auto res = g_Device->acquireNextImageKHR(wd->Swapchain, UINT64_MAX, image_acquired_semaphore);
    wd->FrameIndex = res.value;
    if (res.result == vk::Result::eErrorOutOfDateKHR || res.result == vk::Result::eSuboptimalKHR) {
        g_SwapChainRebuild = true;
        return;
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        g_Device->waitForFences(fd->Fence, VK_TRUE, UINT64_MAX);
        g_Device->resetFences(fd->Fence);
    }
    {
        g_Device->resetCommandPool(fd->CommandPool);
        vk::CommandBufferBeginInfo info = {};
        info.flags |= vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        fd->CommandBuffer.begin(info);
    }
    {
        vk::RenderPassBeginInfo info = {};
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        fd->CommandBuffer.beginRenderPass(info, vk::SubpassContents::eInline);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    fd->CommandBuffer.endRenderPass();
    {
        vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo info = {};
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        fd->CommandBuffer.end();
        g_Queue.submit(info, fd->Fence);
    }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (g_SwapChainRebuild)
        return;
    vk::Semaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    vk::PresentInfoKHR info = {};
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    try {
        g_Queue.presentKHR(info);
    } catch (std::exception exception) {
        g_SwapChainRebuild = true;
        return;
    }
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->ImageCount; // Now we can use the next set of semaphores
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**)
{
    // Setup GLFW window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "GLFW + VulkanHpp", NULL, NULL);

    // Setup Vulkan
    if (!glfwVulkanSupported()) {
        printf("GLFW: Vulkan Not Supported\n");
        return 1;
    }
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(*g_Instance, window, nullptr, &surface);
    if (err != VK_SUCCESS) {
        std::cerr << "failed to create window surface." << std::endl;
    }

    // Create Framebuffers
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = *g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = *g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = *g_PipelineCache;
    init_info.DescriptorPool = *g_DescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = vk::SampleCountFlagBits::e1;
    init_info.Allocator = nullptr;
    ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Upload Fonts
    {
        // Use any command queue
        vk::CommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
        vk::CommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

        g_Device->resetCommandPool(command_pool);
        vk::CommandBufferBeginInfo begin_info = {};
        begin_info.flags |= vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        command_buffer.begin(begin_info);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        vk::SubmitInfo end_info = {};
        end_info.setCommandBuffers(command_buffer);
        command_buffer.end();
        g_Queue.submit(end_info);

        g_Device->waitIdle();
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Resize swap chain?
        if (g_SwapChainRebuild) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            if (width > 0 && height > 0) {
                ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(*g_Instance, g_PhysicalDevice, *g_Device, &g_MainWindowData, g_QueueFamily, nullptr, width, height, g_MinImageCount);
                g_MainWindowData.FrameIndex = 0;
                g_SwapChainRebuild = false;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window) {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            wd->ClearValue.color.float32[3] = clear_color.w;
            FrameRender(wd, draw_data);
            FramePresent(wd);
        }
    }

    // Cleanup
    g_Device->waitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
