# ImGui Vulkan Hpp Backend

A library for writing ImGui simply using Vulkan Hpp (unofficial)

## Usage

Simply include `imgui_impl_vulkan_hpp.h` and `imgui_impl_vulkan_hpp.cpp` in your ImGui project.

## Feature

- Support for `vk::` objects
- Support `std::vector` instead of pointer and count

## Comparison

### CreateDevice

without `Vulkan Hpp`

```cpp
int device_extension_count = 1;
const char* device_extensions[] = { "VK_KHR_swapchain" };
const float queue_priority[] = { 1.0f };

VkDeviceQueueCreateInfo queue_info[1] = {};
queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
queue_info[0].queueFamilyIndex = g_QueueFamily;
queue_info[0].queueCount = 1;
queue_info[0].pQueuePriorities = queue_priority;

VkDeviceCreateInfo create_info = {};
create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
create_info.pQueueCreateInfos = queue_info;
create_info.enabledExtensionCount = device_extension_count;
create_info.ppEnabledExtensionNames = device_extensions;
err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
check_vk_result(err);

// ...

vkDestroyDevice(g_Device, g_Allocator);
```

with `Vulkan HPP`

```cpp
const std::vector device_extensions = { "VK_KHR_swapchain" };
const float queuePriority = 1.0f;

vk::DeviceQueueCreateInfo queue_info = {}; // No need to fill in sType!
queue_info.setQueueFamilyIndex(g_QueueFamily);
queue_info.setQueueCount(1);
queue_info.setPQueuePriorities(&queuePriority);

vk::DeviceCreateInfo create_info = {};
create_info.setQueueCreateInfos(queue_info); // No need to pass the count!
create_info.setPEnabledExtensionNames(device_extensions); // No need to pass the count!
g_Device = g_PhysicalDevice.createDeviceUnique(create_info); // No need to call destroy()!
```

### SelectPresentMode

without `Vulkan Hpp`

```cpp
VkPresentModeKHR present_modes[] = {
    VK_PRESENT_MODE_MAILBOX_KHR,
    VK_PRESENT_MODE_IMMEDIATE_KHR,
    VK_PRESENT_MODE_FIFO_KHR };

wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
    g_PhysicalDevice,
    wd->Surface,
    &present_modes[0],
    IM_ARRAYSIZE(present_modes));
```

with `Vulkan HPP`

```cpp
std::vector present_modes = {
    vk::PresentModeKHR::eMailbox,
    vk::PresentModeKHR::eImmediate,
    vk::PresentModeKHR::eFifo }; // scoped enums!

wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
    g_PhysicalDevice,
    wd->Surface,
    present_modes); // No need to pass the array size!
```
