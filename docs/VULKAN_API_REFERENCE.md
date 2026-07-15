# Vulkan API 字段级参考

> 按初始化顺序，逐个解释项目中用到的每个 VkXxxCreateInfo 结构体的**每一个字段**。

---

## 目录

1. [初始化阶段](#一初始化阶段)
   - VkApplicationInfo
   - VkInstanceCreateInfo
   - VkDebugUtilsMessengerCreateInfoEXT
   - VkWin32SurfaceCreateInfoKHR
   - VkDeviceQueueCreateInfo
   - VkDeviceCreateInfo
   - VkCommandPoolCreateInfo
   - VkSwapchainCreateInfoKHR
   - VkImageViewCreateInfo
2. [RenderPass 阶段](#二renderpass-阶段)
   - VkAttachmentDescription
   - VkAttachmentReference
   - VkSubpassDescription
   - VkSubpassDependency
   - VkRenderPassCreateInfo
3. [Descriptor 阶段](#三descriptor-阶段)
   - VkDescriptorSetLayoutBinding
   - VkDescriptorSetLayoutCreateInfo
   - VkDescriptorPoolSize
   - VkDescriptorPoolCreateInfo
   - VkDescriptorSetAllocateInfo
   - VkWriteDescriptorSet
   - VkDescriptorBufferInfo
   - VkDescriptorImageInfo
4. [Pipeline 阶段](#四pipeline-阶段)
   - VkShaderModuleCreateInfo
   - VkPipelineShaderStageCreateInfo
   - VkVertexInputBindingDescription
   - VkVertexInputAttributeDescription
   - VkPipelineVertexInputStateCreateInfo
   - VkPipelineInputAssemblyStateCreateInfo
   - VkPipelineViewportStateCreateInfo
   - VkPipelineRasterizationStateCreateInfo
   - VkPipelineMultisampleStateCreateInfo
   - VkPipelineDepthStencilStateCreateInfo
   - VkPipelineColorBlendAttachmentState
   - VkPipelineColorBlendStateCreateInfo
   - VkPipelineDynamicStateCreateInfo
   - VkPipelineLayoutCreateInfo
   - VkGraphicsPipelineCreateInfo
5. [Image / Buffer / Memory 阶段](#五image--buffer--memory-阶段)
   - VkImageCreateInfo
   - VkBufferCreateInfo
   - VkMemoryAllocateInfo
   - VkSamplerCreateInfo
   - VkFramebufferCreateInfo
6. [Command / Sync 阶段](#六command--sync-阶段)
   - VkCommandBufferAllocateInfo
   - VkSemaphoreCreateInfo
   - VkFenceCreateInfo
   - VkCommandBufferBeginInfo
   - VkRenderPassBeginInfo
   - VkImageMemoryBarrier
   - VkBufferImageCopy
   - VkSubmitInfo
   - VkPresentInfoKHR
7. [Copy 操作](#七copy-操作)
   - VkBufferCopy
8. [查询函数](#八查询函数)

---

## 一、初始化阶段

### VkApplicationInfo

**文件：** `VulkanContext.cpp:502`

```cpp
VkApplicationInfo appInfo{};
appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;  // (1)
appInfo.pApplicationName   = "Vulkan Renderer";                  // (2)
appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);            // (3)
appInfo.pEngineName        = "VR";                                // (4)
appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);            // (5)
appInfo.apiVersion         = VK_API_VERSION_1_2;                  // (6)
```

| 字段 | 类型 | 含义 |
|------|------|------|
| `sType` (1) | `VkStructureType` | Vulkan 要求所有 CreateInfo 的第一字段必须是 sType。驱动用这个判断你传的是哪种结构体 |
| `pApplicationName` (2) | `const char*` | 纯诊断用途。驱动不会根据这个名字改变行为。在 RenderDoc/Nsight 等工具中可以看到 |
| `applicationVersion` (3) | `uint32_t` | MAJOR.MINOR.PATCH 编码。你程序的版本号。驱动可能用它来绕过已知的应用 bug（和 GPU 厂商合作时有用） |
| `pEngineName` (4) | `const char*` | 引擎名称。同样纯诊断。如果你用 Unity/Unreal 这里填的是引擎名而非应用名 |
| `engineVersion` (5) | `uint32_t` | 引擎版本 |
| `apiVersion` (6) | `uint32_t` | 请求的 Vulkan API 版本。**这个值会影响可用功能**。`VK_API_VERSION_1_2`（1.2.0）与 `VK_API_VERSION_1_0` 的区别：1.2 可以直接用 `VK_KHR_*` 变成核心特性的一部分（如 descriptor indexing）而不需要显式启用扩展。驱动必须 >= 这个版本才可能成功创建 Instance |

---

### VkInstanceCreateInfo

**文件：** `VulkanContext.cpp:526`

```cpp
VkInstanceCreateInfo info{};
info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; // (1)
info.pApplicationInfo        = &appInfo;                               // (2)
info.enabledLayerCount       = 1;                                      // (3)
info.ppEnabledLayerNames     = &layerName;                             // (4)
info.enabledExtensionCount   = extCount;                               // (5)
info.ppEnabledExtensionNames = extNames.data();                        // (6)
info.pNext                   = &debugCreateInfo;                       // (7)
```

| 字段 | 含义 |
|------|------|
| `sType` (1) | 同上，类型标记 |
| `pApplicationInfo` (2) | 指向上面的 VkApplicationInfo。可以为 `nullptr`（驱动会用默认值），但建议填写以便调试工具识别你的应用 |
| `enabledLayerCount` (3) | 要启用的 Validation Layer 数量。**生产环境应该为 0**——Layer 有性能开销 |
| `ppEnabledLayerNames` (4) | Layer 名称数组。当前：`"VK_LAYER_KHRONOS_validation"`。如果 `areValidationLayersSupported()` 返回 false，这套数组为空 |
| `enabledExtensionCount` (5) | Instance 级扩展数量。当前：2 个核心扩展（`VK_KHR_surface` + `VK_KHR_win32_surface`）+ 可能的第 3 个（`VK_EXT_debug_utils`） |
| `ppEnabledExtensionNames` (6) | 扩展名称数组。**Instance 级和 Device 级扩展是分开的**——这里只填 Instance 级的。Device 级扩展在 VkDeviceCreateInfo 中填 |
| `pNext` (7) | **pNext 链**。填 `&debugCreateInfo` 表示在 Instance 创建时同时注册 Debug Messenger 回调。这是 Vulkan 的扩展机制——CreateInfo 本身没有 debug 回调字段，通过 pNext 链追加 `VkDebugUtilsMessengerCreateInfoEXT` |

**关键设计：pNext 链**

Vulkan 的核心设计模式之一是 pNext 链。几乎所有 CreateInfo 都有一个 `void* pNext` 字段。你可以在 pNext 上挂一个或多个扩展结构体，驱动解析时遍历链表读取额外配置。这就是 Vulkan 在不改 API 的情况下支持扩展的方式。

例如这里 pNext 指向 `VkDebugUtilsMessengerCreateInfoEXT`，驱动在创建 Instance 时会读取它，立刻注册调试回调。这样在 Instance 创建失败时也能看到诊断信息。

---

### VkDebugUtilsMessengerCreateInfoEXT

**文件：** `VulkanContext.cpp:51`

```cpp
VkDebugUtilsMessengerCreateInfoEXT info{};
info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; // (1)
info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT           // (2)
                     | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT               // (3)
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
info.pfnUserCallback = myCallback;                                                // (4)
```

| 字段 | 含义 |
|------|------|
| `sType` (1) | `VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT` |
| `messageSeverity` (2) | **过滤哪些严重级别**的通知。`WARNING \| ERROR` 表示忽略 `INFO` 和 `VERBOSE`。这是位掩码——可以组合多个值 |
| `messageType` (3) | **过滤哪些类型的消息**。三个标志：`GENERAL`（API 使用不当）、`VALIDATION`（规范违反）、`PERFORMANCE`（潜在性能问题）。全开以获得最详细的信息 |
| `pfnUserCallback` (4) | 回调函数指针。签名：`VkBool32 (*)(severity, type, pCallbackData, pUserData)`。**必须返回 `VK_FALSE`**（应用程序应始终返回 false——Vulkan 规范明确禁止应用在 validation 回调中返回 true） |

**回调函数内收到的 VkDebugUtilsMessengerCallbackDataEXT：**
- `pMessage`：人类可读的错误描述
- `pMessageIdName`：消息 ID 字符串（如 `"VUID-vkDestroyDevice-device-05137"`）
- `objectCount + pObjects`：涉及的具体 VkObject 句柄
- `queueLabelCount + pQueueLabels`：提交时设置的标签（用于定位问题来源）

---

### VkWin32SurfaceCreateInfoKHR

**文件：** `VulkanContext.cpp:593`

```cpp
VkWin32SurfaceCreateInfoKHR info{};
info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR; // (1)
info.hinstance = GetModuleHandle(nullptr);                         // (2)
info.hwnd      = windowHandle_;                                     // (3)
```

| 字段 | 含义 |
|------|------|
| `sType` (1) | 类型标记 |
| `hinstance` (2) | Win32 `HINSTANCE`。`GetModuleHandle(nullptr)` 返回当前 exe 的实例句柄。操作系统用这个来关联窗口和进程 |
| `hwnd` (3) | Win32 `HWND`。由 `CreateWindowEx` 创建的窗口句柄。Vulkan 通过这个句柄找到窗口的像素缓冲区来呈现 |

**注意：** 这是平台特定扩展（`VK_KHR_win32_surface`）。其他平台有对应的变体：
- Windows: `VkWin32SurfaceCreateInfoKHR`
- Linux X11: `VkXlibSurfaceCreateInfoKHR`
- Linux Wayland: `VkWaylandSurfaceCreateInfoKHR`
- macOS: `VkMacOSSurfaceCreateInfoMVK`（MoltenVK）

---

### VkDeviceQueueCreateInfo

**文件：** `VulkanContext.cpp:632`

```cpp
VkDeviceQueueCreateInfo info{};
info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; // (1)
info.queueFamilyIndex = queueFamily;                                 // (2)
info.queueCount       = 1;                                           // (3)
info.pQueuePriorities = &priority;                                    // (4)
```

| 字段 | 含义 |
|------|------|
| `sType` (1) | 类型标记 |
| `queueFamilyIndex` (2) | 从哪个 Queue Family 分配队列。每个 Family 代表一组能力（如 GRAPHICS、COMPUTE、TRANSFER）。我们创建两个 `VkDeviceQueueCreateInfo`：一个用 graphicsFamily，一个用 presentFamily。如果它们在同一个 Family（大多数 GPU 是这样），用 `std::set` 去重 |
| `queueCount` (3) | 从该 Family 分配几个 Queue。通常 1 个就够了（多线程提交才需要多个）。**注意：这不是"有几个队列"而是"每次创建几个"** |
| `pQueuePriorities` (4) | `float` 数组，长度为 queueCount。范围 [0.0, 1.0]。`vkGetDeviceQueue` 时得到的是对应优先级的 Queue。多个 Queue 之间由驱动调度——高优先级的可能被优先处理 |

---

### VkDeviceCreateInfo

**文件：** `VulkanContext.cpp:643`

```cpp
VkDeviceCreateInfo info{};
info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;  // (1)
info.queueCreateInfoCount    = count;                                  // (2)
info.pQueueCreateInfos       = queueInfos.data();                      // (3)
info.enabledExtensionCount   = 1;                                      // (4)
info.ppEnabledExtensionNames = &kDeviceExtensions[0];                  // (5)
info.pEnabledFeatures        = &deviceFeatures;                        // (6)
info.enabledLayerCount       = 1;                                      // (7)
info.ppEnabledLayerNames     = &layerName;                             // (8)
```

| 字段 | 含义 |
|------|------|
| `sType` (1) | 类型标记 |
| `queueCreateInfoCount` (2) | 上面的 VkDeviceQueueCreateInfo 数组长度 |
| `pQueueCreateInfos` (3) | 数组指针。驱动根据这些创建 Queue |
| `enabledExtensionCount` (4) | **Device 级扩展**数量。当前只有 1 个：`VK_KHR_swapchain`。Device 级和 Instance 级扩展是分开的——这个很容易搞混 |
| `ppEnabledExtensionNames` (5) | Device 级扩展名称列表 |
| `pEnabledFeatures` (6) | **可选 GPU 特性**。当前全部为 `VK_FALSE`（默认值）。`VkPhysicalDeviceFeatures` 是一个很大的结构体，包含 `tessellationShader`、`geometryShader`、`samplerAnisotropy` 等几十个布尔值。只有你显式启用的特性才能使用。**每个特性都可能在 GPU 上有性能代价** |
| `enabledLayerCount` (7) | Device 级 Validation Layer 数量。`VK_LAYER_KHRONOS_validation` 既是 Instance 层也是 Device 层 |
| `ppEnabledLayerNames` (8) | Device 级 Layer 名称 |

**创建完 Device 后获取 Queue：**
```cpp
vkGetDeviceQueue(device_, graphicsFamilyIndex, /*queueIndex=*/0, &graphicsQueue_);
vkGetDeviceQueue(device_, presentFamilyIndex,  /*queueIndex=*/0, &presentQueue_);
```
`queueIndex` 是 VkDeviceQueueCreateInfo 中 `queueCount` 的索引。我们创建了 1 个 Queue，所以索引固定为 0。

---

### VkCommandPoolCreateInfo

**文件：** `VulkanContext.cpp:727`

```cpp
VkCommandPoolCreateInfo info{};
info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; // (1)
info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // (2)
info.queueFamilyIndex = graphicsFamily;                              // (3)
```

| 字段 | 含义 |
|------|------|
| `sType` (1) | 类型标记 |
| `flags` (2) | `RESET_COMMAND_BUFFER_BIT` 允许单独 reset 一个已分配的 CommandBuffer。不带这个标志的话你只能 reset 整个 Pool（会同时影响所有 CommandBuffer）。另外还有 `TRANSIENT_BIT`（表示 CommandBuffer 会被频繁重新录制——驱动可以做优化）和 `PROTECTED_BIT`（受保护内容，极少用） |
| `queueFamilyIndex` (3) | 从该 Pool 分配的 CommandBuffer 只能提交到同族的 Queue。填 `graphicsFamily` 因为我们的 CommandBuffer 要提交到 Graphics Queue |

---

### VkSwapchainCreateInfoKHR

**文件：** `VulkanContext.cpp:678`

```cpp
VkSwapchainCreateInfoKHR info{};
info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; // (1)
info.surface          = surface_;                                      // (2)
info.minImageCount    = imageCount;                                    // (3)
info.imageFormat      = surfaceFormat.format;                          // (4)
info.imageColorSpace  = surfaceFormat.colorSpace;                      // (5)
info.imageExtent      = extent;                                        // (6)
info.imageArrayLayers = 1;                                             // (7)
info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;          // (8)
info.imageSharingMode = sharingMode;                                   // (9)
info.queueFamilyIndexCount = ...;                                      // (10)
info.pQueueFamilyIndices   = ...;                                      // (11)
info.preTransform     = capabilities.currentTransform;                 // (12)
info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;           // (13)
info.presentMode      = presentMode;                                   // (14)
info.clipped          = VK_TRUE;                                       // (15)
```

| 字段 | 含义 |
|------|------|
| `sType` (1) | 类型标记 |
| `surface` (2) | 呈现目标 Surface |
| `minImageCount` (3) | Swapchain 中 Image 的最小数量。设为 `capabilities.minImageCount + 1`（通常 3，即三缓冲）。多一张可以减少 `vkAcquireNextImageKHR` 阻塞等待的概率（详见"双缓冲 vs 三缓冲"） |
| `imageFormat` (4) | Image 像素格式。优先 `VK_FORMAT_B8G8R8A8_SRGB`（8 位 BGRA、sRGB 色彩空间）。sRGB 后缀表示写入时自动做 gamma 校正（线性→sRGB），采样时自动做逆校正 |
| `imageColorSpace` (5) | 色彩空间。与 format 配对：sRGB 格式必须配 `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`（非线性 = gamma 编码）。HDR 场景会用 `EXTENDED_SRGB_LINEAR` 或 HDR10 色彩空间 |
| `imageExtent` (6) | 图像分辨率。通常等于窗口的客户区大小（`capabilities.currentExtent`）。如果窗口被最小化，用 `framebufferWidth_/Height_` 回退 |
| `imageArrayLayers` (7) | 数组层数。普通 2D 渲染用 1。VR（立体渲染）会用 2，cubemap 用 6 |
| `imageUsage` (8) | **用途标志**。`COLOR_ATTACHMENT_BIT` 表示这个 Image 可以作为 RenderPass 的颜色附件。如果要做后处理，还需要加 `SAMPLED_BIT`（作为纹理采样）或 `TRANSFER_DST_BIT`（作为复制目标） |
| `imageSharingMode` (9) | 多队列族共享模式。如果 `graphicsFamily != presentFamily`：`CONCURRENT`（多个队列族同时访问，需要指定哪些族）。否则：`EXCLUSIVE`（单队列族独占，性能更好） |
| `queueFamilyIndexCount` (10) | 只有 CONCURRENT 模式下需要。指定哪些队列族共享这些 Image |
| `pQueueFamilyIndices` (11) | 队列族索引数组 |
| `preTransform` (12) | Surface 的预变换。设为 `currentTransform`（不额外旋转）。移动设备可能需要处理旋转变换（竖屏→横屏） |
| `compositeAlpha` (13) | Alpha 合成模式。`OPAQUE` 表示窗口不透明（忽略 alpha 通道）。如果需要窗口透明，用 `PRE_MULTIPLIED` 或 `POST_MULTIPLIED` |
| `presentMode` (14) | 呈现模式。优先 `MAILBOX`：驱动维护一个单元素队列，新帧直接替换旧帧（最低延迟，不等待 VBlank，不撕裂）。回退 `FIFO`：传统 V-Sync（等 VBlank，有延迟但保证不撕裂） |
| `clipped` (15) | `VK_TRUE`：窗口被遮挡时不渲染被遮挡的像素。可以提升性能。只在需要读回窗口像素时才设为 false |

**双缓冲 vs 三缓冲（`minImageCount`）：**
- 双缓冲（2）：每次 vkAcquireNextImageKHR 需要等前帧渲染完成。渲染慢时帧率直接减半
- 三缓冲（3）：可以在一帧 rendering 时开始下一帧的录制。降低阻塞，但增加内存

**Present Mode 对比：**
- `FIFO`：队列 ≥1，等 VBlank，不撕裂。最安全。类似 glfw 的 `glfwSwapInterval(1)`
- `MAILBOX`：队列 = 1，不等 VBlank，不撕裂。新帧直接替换队列中的旧帧。延迟最低
- `IMMEDIATE`：不等，不排队，可能撕裂。最高帧率但画面断裂

---

### VkImageViewCreateInfo

**文件：** `VulkanContext.cpp:355`

```cpp
VkImageViewCreateInfo info{};
info.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; // (1)
info.image        = image;                                      // (2)
info.viewType     = VK_IMAGE_VIEW_TYPE_2D;                      // (3)
info.format       = format;                                     // (4)
info.subresourceRange.aspectMask     = aspectFlags;              // (5)
info.subresourceRange.baseMipLevel   = 0;                       // (6)
info.subresourceRange.levelCount     = 1;                       // (7)
info.subresourceRange.baseArrayLayer = 0;                       // (8)
info.subresourceRange.layerCount     = 1;                       // (9)
```

| 字段 | 含义 |
|------|------|
| `sType` (1) | 类型标记 |
| `image` (2) | 被解释的 VkImage |
| `viewType` (3) | 视图类型。`2D` 表示普通 2D 纹理。其他值：`3D`、`CUBE`、`2D_ARRAY`。决定在 shader 中用什么 sampler 类型访问 |
| `format` (4) | 格式。必须与 Image 的格式**兼容**（可以不同但必须在同一兼容类别）。例如 Image 可以是 `D32_SFLOAT`，ImageView 用同样的格式 |
| `aspectMask` (5) | 访问哪些方面。`COLOR_BIT` 用于颜色、`DEPTH_BIT` 用于深度、`STENCIL_BIT` 用于模板。不能混用 COLOR 和 DEPTH |
| `baseMipLevel` (6) | 起始 mip 级别。0 表示最精细的 mip（原始分辨率）。当前项目不使用 mipmap，始终为 0 |
| `levelCount` (7) | mip 级别数量。1 表示只有一级（无 mipmap） |
| `baseArrayLayer` (8) | 起始数组层。2D 纹理始终为 0。cubemap 或纹理数组会用到不同层 |
| `layerCount` (9) | 层数。2D 纹理始终为 1 |

---

## 二、RenderPass 阶段

### VkAttachmentDescription

**文件：** `ForwardPass.cpp:186, 195`（Color 和 Depth 各一个）

```cpp
// Color Attachment
VkAttachmentDescription color{};
color.format         = swapchainFormat;                    // (1)
color.samples        = VK_SAMPLE_COUNT_1_BIT;             // (2)
color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;       // (3)
color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;      // (4)
color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   // (5)
color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // (6)
color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;          // (7)
color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;    // (8)
```

| 字段 | 含义 |
|------|------|
| `format` (1) | 附件格式。必须与写入它的 ImageView 格式完全一致 |
| `samples` (2) | MSAA 采样数。`1` = 无抗锯齿。`4` = 4x MSAA。注意：所有 Subpass 中同一位置的 Attachment 的 samples 必须一致 |
| `loadOp` (3) | **进入 Subpass 时做什么**。`CLEAR`：用 `VkClearValue` 清空附件。`LOAD`：保留之前的内容（用于多 Pass 累积）。`DONT_CARE`：内容未定义（性能最好，但之前的内容丢失） |
| `storeOp` (4) | **离开 Subpass 时做什么**。`STORE`：把结果写入内存（后续需要读取时必须用这个）。`DONT_CARE`：丢弃（如果后续不再需要，移动 GPU 可以省去写回，省带宽） |
| `stencilLoadOp` (5) | 模板缓存区的 loadOp。不需要模板时用 `DONT_CARE` |
| `stencilStoreOp` (6) | 模板缓存区的 storeOp |
| `initialLayout` (7) | RenderPass 开始前 Image 应该处于哪种布局。`UNDEFINED` 表示不关心（性能最优，GPU 不需要做任何布局转换）。**只有第一帧或 swapchain 重建后可以用 UNDEFINED**——后续帧应为 `PRESENT_SRC_KHR`，否则每一帧都会丢弃上一帧的内容 |
| `finalLayout` (8) | RenderPass 结束后 Image 应转换到的布局。Color Attachment 最终是 `PRESENT_SRC_KHR`（可以呈现）；Depth Attachment 是 `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` |

**Depth Attachment 的差异：**
- `format`：`findDepthFormat()` 返回的最优深度格式（`D32_SFLOAT` 优先）
- `storeOp`：`DONT_CARE`（渲染完不需要保留深度缓冲）

---

### VkAttachmentReference

```cpp
VkAttachmentReference colorRef{};
colorRef.attachment = 0;                                           // (1)
colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;   // (2)
```

| 字段 | 含义 |
|------|------|
| `attachment` (1) | 指向 VkAttachmentDescription 数组中的索引。0 = 第一个 Attachment（Color），1 = 第二个（Depth） |
| `layout` (2) | Subpass 期间 Image 应该处于的布局。`COLOR_ATTACHMENT_OPTIMAL` 是 GPU 写入颜色附件的最优布局。`DEPTH_STENCIL_ATTACHMENT_OPTIMAL` 是读取/写入深度的最优布局 |

---

### VkSubpassDescription

```cpp
VkSubpassDescription subpass{};
subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS; // (1)
subpass.colorAttachmentCount    = 1;                                 // (2)
subpass.pColorAttachments       = &colorRef;                         // (3)
subpass.pDepthStencilAttachment = &depthRef;                         // (4)
```

| 字段 | 含义 |
|------|------|
| `pipelineBindPoint` (1) | 绑定的管线类型。`GRAPHICS`（图形管线）或 `COMPUTE`（计算管线）或 `RAY_TRACING` |
| `colorAttachmentCount` (2) | Color Attachment 数量。当前 = 1（单 Render Target）。Deferred rendering 时会 = 3+（MRT） |
| `pColorAttachments` (3) | AttachmentReference 数组。长度等于 colorAttachmentCount |
| `pDepthStencilAttachment` (4) | Depth 附件。可以为 `nullptr`（不需要深度测试的 Pass） |

**注意：** `pColorAttachments[i]` 的索引对应 Shader 中 `layout(location = i) out vec4 color;`。

---

### VkSubpassDependency

```cpp
VkSubpassDependency dep{};
dep.srcSubpass    = VK_SUBPASS_EXTERNAL;                          // (1)
dep.dstSubpass    = 0;                                             // (2)
dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT  // (3)
                  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT  // (4)
                  | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
dep.srcAccessMask = 0;                                             // (5)
dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT           // (6)
                  | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
```

| 字段 | 含义 |
|------|------|
| `srcSubpass` (1) | 依赖的**源** Subpass。`VK_SUBPASS_EXTERNAL` 表示 RenderPass 之外的命令（如前帧的绘制、Swapchain 图像获取） |
| `dstSubpass` (2) | 依赖的**目标** Subpass。`0` = 我们的唯一 Subpass |
| `srcStageMask` (3) | 源阶段：在哪些 Pipeline Stage 完成之后。`COLOR_ATTACHMENT_OUTPUT`（颜色写入）+ `EARLY_FRAGMENT_TESTS`（深度测试）= 等前帧完全写完 |
| `dstStageMask` (4) | 目标阶段：在哪些 Pipeline Stage 可以开始。同上 |
| `srcAccessMask` (5) | 源访问类型。`0` 表示不等待任何特定内存访问（适用于 `VK_SUBPASS_EXTERNAL`→第一个 Subpass 的依赖） |
| `dstAccessMask` (6) | 目标访问类型：`COLOR_ATTACHMENT_WRITE`（允许写颜色附件）+ `DEPTH_STENCIL_ATTACHMENT_WRITE`（允许写深度附件） |

**Dependency 的作用：** 告诉 GPU 什么时候可以安全地开始执行目标 Subpass。不正确的 Dependency 会导致**数据竞争**——某帧的渲染还没完成，下一帧就开始写入同一个 Attachment。

---

### VkRenderPassCreateInfo

```cpp
VkRenderPassCreateInfo info{};
info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO; // (1)
info.attachmentCount = 2;                                          // (2)
info.pAttachments    = attachments;                                 // (3)
info.subpassCount    = 1;                                          // (4)
info.pSubpasses      = &subpass;                                   // (5)
info.dependencyCount = 1;                                          // (6)
info.pDependencies   = &dep;                                       // (7)
```

| 字段 | 含义 |
|------|------|
| `attachmentCount` (2) | 附件总数。2 = Color + Depth |
| `pAttachments` (3) | VkAttachmentDescription 数组。这里的顺序是 `attachment` 索引的来源 |
| `subpassCount` (4) | Subpass 数量。1 = 单 Pass 前向渲染 |
| `pSubpasses` (5) | VkSubpassDescription 数组 |
| `dependencyCount` (6) | Subpass 依赖关系数量 |
| `pDependencies` (7) | VkSubpassDependency 数组 |

---

## 三、Descriptor 阶段

### VkDescriptorSetLayoutBinding

```cpp
VkDescriptorSetLayoutBinding bindings[2]{};

bindings[0].binding         = 0;                                       // (1)
bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;      // (2)
bindings[0].descriptorCount = 1;                                       // (3)
bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;             // (4)

bindings[1].binding         = 1;
bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // (5)
bindings[1].descriptorCount = 1;
bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;            // (6)
```

| 字段 | 含义 |
|------|------|
| `binding` (1) | 绑定点索引。对应 Shader 中的 `layout(binding = N)`。必须是唯一的 |
| `descriptorType` (2) | 描述符类型。`UNIFORM_BUFFER`：只读 Uniform Buffer（对应 `layout(binding=0) uniform`）。`COMBINED_IMAGE_SAMPLER`：纹理 + 采样器的组合（对应 `uniform sampler2D`） |
| `descriptorCount` (3) | 数组大小。1 表示单一 Buffer/Image。`>1` 时 Shader 中对应 `layout(binding=0) uniform MyUBO { ... } ubos[N];` |
| `stageFlags` (4) | 哪些 Shader Stage 可见。`VERTEX_BIT` 表示只对 Vertex Shader 可见。Fragment Shader 访问不到——如果你在 frag 中访问这个 UBO 会报错 |
| `descriptorType` (5) | `COMBINED_IMAGE_SAMPLER` 是 Sampled Image + Sampler 的组合。分开的版本：`SAMPLED_IMAGE` + `SAMPLER`（可以独立绑定纹理和采样器，更灵活但更复杂） |
| `stageFlags` (6) | `FRAGMENT_BIT`：纹理只在 Fragment Shader 中使用 |

---

### VkDescriptorSetLayoutCreateInfo

```cpp
VkDescriptorSetLayoutCreateInfo info{};
info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
info.bindingCount = 2;
info.pBindings    = bindings;
```

| 字段 | 含义 |
|------|------|
| `bindingCount` | 上面的 binding 数组长度 |
| `pBindings` | 数组指针 |

---

### VkDescriptorPoolSize

```cpp
VkDescriptorPoolSize sizes[] = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         2 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
};
```

| 字段 | 含义 |
|------|------|
| `type` | 描述符类型 |
| `descriptorCount` | 该类型的描述符总数。2 表示 Pool 最多分配 2 个 UBO 描述符和 2 个纹理描述符 |

**注意：** 这是**总数**，不是 maxSets。Pool 的总容量 = 每种类型各 `descriptorCount` 个描述符。分配时如果超出容量，`vkAllocateDescriptorSets` 会失败。

### VkDescriptorPoolCreateInfo

```cpp
VkDescriptorPoolCreateInfo info{};
info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
info.poolSizeCount = 2;
info.pPoolSizes    = sizes;
info.maxSets       = 2;              // 最多分配 2 个 DescriptorSet
```

| 字段 | 含义 |
|------|------|
| `flags` | 未设置。`FREE_DESCRIPTOR_SET_BIT` 允许单独释放 DescriptorSet（不常用）。默认行为：只能整个 Pool 一起重置 |
| `poolSizeCount` | poolSizes 数组长度 |
| `pPoolSizes` | 容量配置 |
| `maxSets` | 最多从该 Pool 分配几个 Set。2 = 双缓冲，每帧一个独立的 Set |

---

### VkDescriptorSetAllocateInfo

```cpp
VkDescriptorSetAllocateInfo info{};
info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
info.descriptorPool     = descriptorPool_;
info.descriptorSetCount = 2;
info.pSetLayouts        = layouts;        // { layout, layout } — 两个 Set 同一个 Layout
```

| 字段 | 含义 |
|------|------|
| `descriptorPool` | 从哪个 Pool 分配 |
| `descriptorSetCount` | 分配几个 Set |
| `pSetLayouts` | 每个 Set 的 Layout。数组长度 = descriptorSetCount。可以每个 Set 用不同 Layout（不常用）。我们全部用同一个 |

---

### VkWriteDescriptorSet + VkDescriptorBufferInfo + VkDescriptorImageInfo

**Write UBO（Binding 0）：**
```cpp
VkDescriptorBufferInfo bufInfo{};
bufInfo.buffer = uniformBuffers_[i];         // (1)
bufInfo.offset = 0;                          // (2)
bufInfo.range  = sizeof(UniformBufferObject); // (3)

VkWriteDescriptorSet write{};
write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
write.dstSet          = descriptorSets_[i];                // (4) 目标 Set
write.dstBinding      = 0;                                  // (5) 对应 Shader 的 binding=0
write.dstArrayElement = 0;                                  // (6) 数组起始索引
write.descriptorCount = 1;                                  // (7) 描述符数量
write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // (8)
write.pBufferInfo     = &bufInfo;                           // (9)
```

**Write Sampler（Binding 1）：**
```cpp
VkDescriptorImageInfo imgInfo{};
imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
imgInfo.imageView   = textureImageView_;
imgInfo.sampler     = textureSampler_;

VkWriteDescriptorSet write{};
write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
write.dstSet          = descriptorSets_[i];                      // (4)
write.dstBinding      = 1;                                        // (5) 对应 Shader 的 binding=1
write.descriptorCount = 1;
write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
write.pImageInfo      = &imgInfo;                                 // (9) 纹理用 pImageInfo
```

| 字段 | 含义 |
|------|------|
| `dstSet` (4) | 要更新的 DescriptorSet 句柄 |
| `dstBinding` (5) | 更新哪个 Binding 点。必须与 Shader 中的 `layout(binding=N)` 一致 |
| `dstArrayElement` (6) | 如果 descriptorCount > 1（描述符数组），从哪个元素开始。单个描述符始终为 0 |
| `descriptorCount` (7) | 更新的描述符数量 |
| `descriptorType` (8) | 必须与 DescriptorSetLayout 中定义的类型一致 |
| `pBufferInfo` / `pImageInfo` (9) | 根据类型选择：`UNIFORM_BUFFER` / `STORAGE_BUFFER` 用 pBufferInfo；`COMBINED_IMAGE_SAMPLER` / `SAMPLED_IMAGE` 用 pImageInfo；`SAMPLER` 也用 pImageInfo（但只填 sampler） |

**VkDescriptorBufferInfo 字段：**
| 字段 | 含义 |
|------|------|
| `buffer` (1) | VkBuffer 句柄 |
| `offset` (2) | Buffer 内的偏移（字节）。可以多个 UBO 放在同一大 Buffer 的不同偏移位置 |
| `range` (3) | 可访问的范围（字节）。`VK_WHOLE_SIZE` 表示从 offset 到 buffer 末尾 |

**VkDescriptorImageInfo 字段：**
| 字段 | 含义 |
|------|------|
| `imageLayout` | Image 在 shader 访问时应处于的布局。`SHADER_READ_ONLY_OPTIMAL`（纹理采样）。如果 Image 被 shader 写入，用 `GENERAL` |
| `imageView` | VkImageView 句柄 |
| `sampler` | VkSampler 句柄。如果是 `COMBINED_IMAGE_SAMPLER` 必须填。如果是纯 `SAMPLED_IMAGE` 则为 `VK_NULL_HANDLE` |

---

## 四、Pipeline 阶段

### VkShaderModuleCreateInfo

```cpp
VkShaderModuleCreateInfo info{};
info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
info.codeSize = code.size();                                     // (1)
info.pCode    = reinterpret_cast<const uint32_t*>(code.data());  // (2)
```

| 字段 | 含义 |
|------|------|
| `codeSize` (1) | SPIR-V 二进制的大小（字节数）。不是 uint32_t 的数量——是总字节 |
| `pCode` (2) | SPIR-V 二进制的指针。**SPIR-V 指令是 32 位对齐的**，所以转换为 `uint32_t*` |

---

### VkPipelineShaderStageCreateInfo

```cpp
VkPipelineShaderStageCreateInfo stages[2]{};

stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;    // (1)
stages[0].module = vertModule;                      // (2)
stages[0].pName  = "main";                          // (3)

stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
stages[1].module = fragModule;
stages[1].pName  = "main";
```

| 字段 | 含义 |
|------|------|
| `stage` (1) | 哪个 Shader Stage。`VERTEX_BIT` / `FRAGMENT_BIT` / `GEOMETRY_BIT` / `COMPUTE_BIT` 等 |
| `module` (2) | 上面创建的 VkShaderModule |
| `pName` (3) | **入口函数名称**。GLSL 的 `void main()` 对应 `"main"`。可以用不同的入口函数名来实现多着色器变体（Specialization） |
| `pSpecializationInfo` | 未设置。Specialization Constant 允许在 Pipeline 创建时替换 SPIR-V 中的常量值（类似 C++ 模板），比运行时 uniform 更高效 |

---

### VkVertexInputBindingDescription

```cpp
VkVertexInputBindingDescription binding{};
binding.binding   = 0;                            // (1)
binding.stride    = sizeof(Vertex);               // (2) 32 bytes
binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // (3)
```

| 字段 | 含义 |
|------|------|
| `binding` (1) | Vertex Buffer 的 Binding 编号。对应 `vkCmdBindVertexBuffers(binding=0)`。可以绑定多个 VB（如 position 在 binding=0，normal 在 binding=1），但我们把所有顶点数据交织在同一个 buffer 中 |
| `stride` (2) | 相邻两个顶点的起始位置之间的字节距离。`sizeof(Vertex)` = `vec3 + vec3 + vec2` = `3*4 + 3*4 + 2*4` = **32 bytes**（无 padding） |
| `inputRate` (3) | `VERTEX`（逐顶点数据，每个顶点推进 stride 字节）。`INSTANCE` 是逐实例数据（每个 DrawCall 的每个 Instance 推进一次） |

---

### VkVertexInputAttributeDescription

```cpp
VkVertexInputAttributeDescription attrs[3]{};

attrs[0].binding  = 0;                                          // (1)
attrs[0].location = 0;                                          // (2)
attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;                // (3)
attrs[0].offset   = offsetof(Vertex, position);                 // (4) = 0

attrs[1].location = 1;
attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
attrs[1].offset   = offsetof(Vertex, normal);                   // = 12

attrs[2].location = 2;
attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;
attrs[2].offset   = offsetof(Vertex, uv);                       // = 24
```

| 字段 | 含义 |
|------|------|
| `binding` (1) | 数据来自哪个 VB Binding |
| `location` (2) | 对应 Shader 中的 `layout(location = N) in vec3 xxx;` |
| `format` (3) | 数据格式。`R32G32B32_SFLOAT` = 3 个 32 位有符号浮点数 = `vec3`。`R32G32_SFLOAT` = 2 个 = `vec2` |
| `offset` (4) | 该属性在单个顶点内的字节偏移。`offsetof` 宏（C++ 标准）计算成员在结构体中的偏移 |

**布局验证：**
```
Vertex 结构体 (32 bytes):
  offset 0:  position.x (4 bytes)
  offset 4:  position.y (4 bytes)
  offset 8:  position.z (4 bytes)
  offset 12: normal.x   (4 bytes)
  offset 16: normal.y   (4 bytes)
  offset 20: normal.z   (4 bytes)
  offset 24: uv.x       (4 bytes)
  offset 28: uv.y       (4 bytes)
```

---

### VkPipelineVertexInputStateCreateInfo

```cpp
VkPipelineVertexInputStateCreateInfo vi{};
vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
vi.vertexBindingDescriptionCount   = 1;               // (1)
vi.pVertexBindingDescriptions      = &binding;        // (2)
vi.vertexAttributeDescriptionCount = 3;               // (3)
vi.pVertexAttributeDescriptions    = attrs;           // (4)
```

| 字段 | 含义 |
|------|------|
| (1)(2) | 多少个 VB Binding、每个的描述 |
| (3)(4) | 多少个 Vertex Attribute、每个的描述 |

---

### VkPipelineInputAssemblyStateCreateInfo

```cpp
VkPipelineInputAssemblyStateCreateInfo ia{};
ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
ia.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // (1)
ia.primitiveRestartEnable = VK_FALSE;                              // (2)
```

| 字段 | 含义 |
|------|------|
| `topology` (1) | 图元拓扑。`TRIANGLE_LIST`（每 3 个顶点 = 1 个三角形）。其他：`LINE_LIST`（线）、`POINT_LIST`（点）、`TRIANGLE_STRIP`（连续三角形，节省索引） |
| `primitiveRestartEnable` (2) | 是否启用图元重启。配合 `TRIANGLE_STRIP` 使用——遇到特殊索引值时结束当前 Strip 开始新 Strip |

---

### VkPipelineViewportStateCreateInfo

```cpp
VkPipelineViewportStateCreateInfo vs{};
vs.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
vs.viewportCount = 1;    // (1)
vs.scissorCount  = 1;    // (2)
```

| 字段 | 含义 |
|------|------|
| `viewportCount` (1) | Viewport 数量。可以是多个（多视口渲染，如分屏）。**设置为 DYNAMIC 状态后这些是上限**——实际使用时可以更少但绝不能超过 |
| `scissorCount` (2) | Scissor 数量。同上。必须与 viewportCount 一致或为 1 |

---

### VkPipelineRasterizationStateCreateInfo

```cpp
VkPipelineRasterizationStateCreateInfo rs{};
rs.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rs.polygonMode             = VK_POLYGON_MODE_FILL;                 // (1)
rs.cullMode                = VK_CULL_MODE_BACK_BIT;               // (2)
rs.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;     // (3)
rs.lineWidth               = 1.0f;                                 // (4)
rs.depthClampEnable        = VK_FALSE;                             // (5)
rs.rasterizerDiscardEnable = VK_FALSE;                             // (6)
```

| 字段 | 含义 |
|------|------|
| `polygonMode` (1) | 填充模式。`FILL`（实心）、`LINE`（线框）、`POINT`（点云）。线框和点需要启用对应 GPU 特性 |
| `cullMode` (2) | 剔除模式。`BACK`（剔除背面）、`FRONT`（剔除正面）、`NONE`（不剔除）。背面 = 从摄像机看不到的面——节省约 50% 的片段着色计算 |
| `frontFace` (3) | 如何判断"正面"。`COUNTER_CLOCKWISE` 表示顶点按逆时针排列时是正面。这取决于你用的数学库——GLM 默认是逆时针正面的右手坐标系。**OpenGL 默认是 `COUNTER_CLOCKWISE`，Vulkan 也是，但 DirectX 是 `CLOCKWISE`** |
| `lineWidth` (4) | 线宽。默认 1.0。>1 需要 `wideLines` 特性 |
| `depthClampEnable` (5) | 是否把近/远裁剪面外的片段 clamp 到裁剪面而非丢弃。`FALSE` = 丢弃（默认）。`TRUE` = 保留但不写入深度缓冲 |
| `rasterizerDiscardEnable` (6) | 是否跳过光栅化。`TRUE` 时只在 Vertex Shader 阶段执行（用于 transform feedback） |

**剔除的视觉效果：**
- 剔除背面：节省性能，是标准做法。观察封闭网格的内部时看不到任何东西（因为所有面都是"背面"）
- 不剔除：两面都能看到（做树叶、纸张等薄物体时需要）

---

### VkPipelineMultisampleStateCreateInfo

```cpp
VkPipelineMultisampleStateCreateInfo ms{};
ms.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
ms.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;  // (1)
ms.sampleShadingEnable   = VK_FALSE;                // (2)
```

| 字段 | 含义 |
|------|------|
| `rasterizationSamples` (1) | MSAA 采样数。`1` = 无 MSAA。`4` = 4x MSAA（每个像素 4 个采样点，三角形边缘平滑） |
| `sampleShadingEnable` (2) | 是否启用 Sample Shading。`TRUE` 时片段着色器对每个 Sample 都执行一遍（而非每个 Pixel 执行一次然后复制到所有 Sample）。超高质量但极贵 |

---

### VkPipelineDepthStencilStateCreateInfo

```cpp
VkPipelineDepthStencilStateCreateInfo ds{};
ds.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
ds.depthTestEnable       = VK_TRUE;                // (1)
ds.depthWriteEnable      = VK_TRUE;                // (2)
ds.depthCompareOp        = VK_COMPARE_OP_LESS;     // (3)
ds.depthBoundsTestEnable = VK_FALSE;               // (4)
ds.stencilTestEnable     = VK_FALSE;               // (5)
```

| 字段 | 含义 |
|------|------|
| `depthTestEnable` (1) | 是否启用深度测试。`TRUE`：片段的深度值与深度缓冲比较，不通过的丢弃 |
| `depthWriteEnable` (2) | 是否写入深度缓冲。`TRUE`：通过的片段更新深度缓冲。**可以只测试不写入**（如半透明物体：需要读深度但不写） |
| `depthCompareOp` (3) | 比较操作。`LESS`：只保留比已有深度更近的片段（标准深度测试）。`LESS_OR_EQUAL`、`ALWAYS`（始终通过）、`NEVER` 等 |
| `depthBoundsTestEnable` (4) | 可选的额外深度范围测试（只在特定 min/max 深度范围通过）。关闭 |
| `stencilTestEnable` (5) | 模板测试。关闭（不需要模板） |

---

### VkPipelineColorBlendAttachmentState

```cpp
VkPipelineColorBlendAttachmentState cb{};
cb.blendEnable         = VK_FALSE;                                      // (1)
cb.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT                       // (2)
                       | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT
                       | VK_COLOR_COMPONENT_A_BIT;
```

| 字段 | 含义 |
|------|------|
| `blendEnable` (1) | 是否启用混合。`FALSE` 时片段着色器的输出直接覆盖 Framebuffer 中的值。`TRUE` 时需要配置 src/dst 混合因子和混合操作（`srcColorBlendFactor`、`dstColorBlendFactor`、`colorBlendOp` 等字段） |
| `colorWriteMask` (2) | 哪些通道可以写入。`R|G|B|A` 表示全部可写。如果做后处理可能需要只写部分通道 |

---

### VkPipelineColorBlendStateCreateInfo

```cpp
VkPipelineColorBlendStateCreateInfo cbs{};
cbs.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
cbs.attachmentCount = 1;             // (1)
cbs.pAttachments    = &cb;           // (2)
```

| 字段 | 含义 |
|------|------|
| `attachmentCount` (1) | Attachment 数量。必须与 Subpass 中 colorAttachmentCount 一致 |
| `pAttachments` (2) | 每个 Attachment 的混合配置 |

---

### VkPipelineDynamicStateCreateInfo

```cpp
VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

VkPipelineDynamicStateCreateInfo dy{};
dy.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
dy.dynamicStateCount = 2;
dy.pDynamicStates    = dynStates;
```

| 字段 | 含义 |
|------|------|
| `pDynamicStates` | 哪些管线状态可以在录制时动态设置而不用重建管线。`VIEWPORT`：`vkCmdSetViewport`。`SCISSOR`：`vkCmdSetScissor` |

**动态 vs 静态状态的权衡：**
- 动态：在 CommandBuffer 中设置，灵活但不保证最优（驱动需要 JIT 编译变体）
- 静态：在 Pipeline 创建时固定，性能最优（驱动提前编译优化后的代码）
- 常见动态状态：viewport、scissor（窗口大小可变）、blend constants（透明度渐变）

---

### VkPipelineLayoutCreateInfo

```cpp
VkPipelineLayoutCreateInfo pl{};
pl.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
pl.setLayoutCount = 1;                        // (1)
pl.pSetLayouts    = &descriptorSetLayout_;    // (2)
pl.pushConstantRangeCount = 0;                // (3) 未使用 Push Constants
```

| 字段 | 含义 |
|------|------|
| `setLayoutCount` (1) | DescriptorSet Layout 数量。1 = 全部资源（UBO + Texture）在一个 Set 中 |
| `pSetLayouts` (2) | 数组指针 |
| `pushConstantRangeCount` (3) | Push Constant 范围数量。0 = 未使用。如果用 Push Constant 传 model 矩阵，这里填 1 |

**Push Constants（未使用但值得了解）：**
```cpp
VkPushConstantRange range{};
range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;  // 对 Vertex Shader 可见
range.offset     = 0;
range.size       = sizeof(glm::mat4);            // 64 bytes

pl.pushConstantRangeCount = 1;
pl.pPushConstantRanges    = &range;
```
然后在 CommandBuffer 中调用：
```cpp
vkCmdPushConstants(cmd, pipelineLayout, VERTEX_BIT, 0, 64, &modelMatrix);
```

Push Constants 比 UBO 快（驱动直接内联到 Shader 代码），但有大小限制（通常 128 bytes）。

---

### VkGraphicsPipelineCreateInfo

```cpp
VkGraphicsPipelineCreateInfo pi{};
pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
pi.stageCount          = 2;                           // (1)
pi.pStages             = stages;                      // (2)
pi.pVertexInputState   = &vertexInput;               // (3)
pi.pInputAssemblyState = &inputAssembly;             // (4)
pi.pViewportState      = &viewportState;             // (5)
pi.pRasterizationState = &rasterizer;                // (6)
pi.pMultisampleState   = &multisampling;             // (7)
pi.pDepthStencilState  = &depthStencil;              // (8)
pi.pColorBlendState    = &colorBlending;             // (9)
pi.pDynamicState       = &dynamicState;              // (10)
pi.layout              = pipelineLayout_;            // (11)
pi.renderPass          = renderPass_;                // (12)
pi.subpass             = 0;                          // (13)
pi.basePipelineHandle  = VK_NULL_HANDLE;             // (14) 未使用 pipeline derivatives
```

| 字段 | 含义 |
|------|------|
| `stageCount` (1) | 着色器阶段数量 |
| `pStages` (2) | VkPipelineShaderStageCreateInfo 数组 |
| `pVertexInputState` (3) | 顶点输入（Binding + Attribute） |
| `pInputAssemblyState` (4) | 图元装配（拓扑） |
| `pViewportState` (5) | 视口状态（视口 + Scissor 数量上限） |
| `pRasterizationState` (6) | 光栅化（填充、剔除、正面方向） |
| `pMultisampleState` (7) | 多重采样（MSAA） |
| `pDepthStencilState` (8) | 深度测试 |
| `pColorBlendState` (9) | 颜色混合 |
| `pDynamicState` (10) | 哪些状态是动态的 |
| `layout` (11) | PipelineLayout（DescriptorSetLayout） |
| `renderPass` (12) | 该管线兼容的 RenderPass。**Pipeline 只能在创建时指定的 RenderPass 中使用**（或兼容的 RenderPass） |
| `subpass` (13) | 该管线用于 RenderPass 中的第几个 Subpass（从 0 开始） |
| `basePipelineHandle` (14) | Pipeline Derivatives：从一个基准 Pipeline 继承编译后的代码来加速新 Pipeline 创建。`VK_NULL_HANDLE` 表示不继承 |

---

## 五、Image / Buffer / Memory 阶段

### VkImageCreateInfo

```cpp
VkImageCreateInfo ii{};
ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
ii.imageType     = VK_IMAGE_TYPE_2D;                          // (1)
ii.format        = VK_FORMAT_R8G8B8A8_SRGB;                   // (2)
ii.extent        = {width, height, 1};                        // (3)
ii.mipLevels     = 1;                                         // (4)
ii.arrayLayers   = 1;                                         // (5)
ii.samples       = VK_SAMPLE_COUNT_1_BIT;                     // (6)
ii.tiling        = VK_IMAGE_TILING_OPTIMAL;                   // (7)
ii.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT            // (8)
                 | VK_IMAGE_USAGE_SAMPLED_BIT;
ii.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;                 // (9)
ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                 // (10)
```

| 字段 | 含义 |
|------|------|
| `imageType` (1) | `2D`：普通纹理。`3D`：体纹理。`1D`：一维纹理 |
| `format` (2) | 像素格式。`R8G8B8A8_SRGB`：8位RGBA + sRGB色彩空间。深度纹理用 `D32_SFLOAT` |
| `extent` (3) | 分辨率。`{256, 256, 1}` = 256×256 的 2D 纹理 |
| `mipLevels` (4) | Mipmap 级别数。1 = 无 mipmap。每次 mip 尺寸减半：256→128→64→32→16→8→4→2→1 = 9 levels |
| `arrayLayers` (5) | 数组层数。1 = 单一纹理 |
| `samples` (6) | MSAA 采样数。必须与使用该 Image 的 RenderPass 中的 Attachments 的 samples 一致 |
| `tiling` (7) | GPU 内存布局。`OPTIMAL`：驱动自动选择最优布局（不可 CPU 直接访问，性能最好）。`LINEAR`：逐行存储（CPU 可 map，但性能差且需要 `LINEAR` 标志支持的格式才可用） |
| `usage` (8) | 用途标志组合。`TRANSFER_DST`（可以作为 `vkCmdCopyBufferToImage` 的目标）+ `SAMPLED`（可以作为 Shader 纹理采样）。深度纹理用 `DEPTH_STENCIL_ATTACHMENT` |
| `sharingMode` (9) | 队列族共享模式。`EXCLUSIVE` = 单族独占 |
| `initialLayout` (10) | 创建时的布局。`UNDEFINED` 表示不关心初始布局（创建后立即用 Pipeline Barrier 转换） |

### VkBufferCreateInfo

```cpp
VkBufferCreateInfo bi{};
bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bi.size        = bufferSize;                                     // (1)
bi.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;              // (2)
bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;                      // (3)
```

| 字段 | 含义 |
|------|------|
| `size` (1) | Buffer 大小（字节） |
| `usage` (2) | 用途。常见：`VERTEX_BUFFER`、`INDEX_BUFFER`、`UNIFORM_BUFFER`、`TRANSFER_SRC`（staging 源）、`TRANSFER_DST`（GPU-local 目标） |
| `sharingMode` (3) | 队列族共享模式 |

### VkMemoryAllocateInfo

```cpp
VkMemoryAllocateInfo ai{};
ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
ai.allocationSize  = memRequirements.size;       // (1) 所需字节数
ai.memoryTypeIndex = findMemoryType(...);         // (2) 满足需求的内存类型索引
```

| 字段 | 含义 |
|------|------|
| `allocationSize` (1) | 分配大小。必须 ≥ `vkGetBufferMemoryRequirements` / `vkGetImageMemoryRequirements` 返回的 size |
| `memoryTypeIndex` (2) | **关键字段**——GPU 内存被分为多个类型（Heap），每种类型有不同的属性。通过 `findMemoryType(memoryTypeBits, properties)` 找到合适的索引 |

**内存类型查找逻辑（`findMemoryType`）：**
```
VkPhysicalDeviceMemoryProperties.memoryTypes[] 包含：
  - propertyFlags: DEVICE_LOCAL / HOST_VISIBLE / HOST_COHERENT / HOST_CACHED 等
  - heapIndex: 属于哪个 Heap

遍历 memoryTypes[0..count]：
  if (typeFilter 的对应位 为 1) AND (properties 满足需求)
    return 索引
```

**两种常见组合：**
- `DEVICE_LOCAL`：GPU 独占内存（VRAM），性能最高。CPU 不能直接访问。Mesh VB/IB、纹理 Image 用这个
- `HOST_VISIBLE | HOST_COHERENT`：CPU 可访问的系统内存。`COHERENT` 表示不需要 `vkFlushMappedMemoryRanges`（自动同步）。UBO、Staging Buffer 用这个

---

### VkSamplerCreateInfo

```cpp
VkSamplerCreateInfo sc{};
sc.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
sc.magFilter    = VK_FILTER_LINEAR;                 // (1) 放大时的过滤
sc.minFilter    = VK_FILTER_LINEAR;                 // (2) 缩小时的过滤
sc.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;    // (3) mip 级别间的混合
sc.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;   // (4)
sc.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;   // (5)
sc.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;   // (6)
```

| 字段 | 含义 |
|------|------|
| `magFilter` (1) | 纹理被放大（近距离看）的过滤。`LINEAR`（双线性插值，平滑）、`NEAREST`（最近邻居，像素风） |
| `minFilter` (2) | 纹理被缩小（远距离看）的过滤。`LINEAR`（双线性，默认）。配合 mipmap 时用 `LINEAR_MIPMAP_LINEAR`（三线性） |
| `mipmapMode` (3) | mip 级别之间的混合。当前无 mipmap，不影响 |
| `addressModeU/V/W` (4-6) | UV 坐标超出 [0,1] 范围时怎么办。`REPEAT`（重复平铺）、`CLAMP_TO_EDGE`（边缘拉伸）、`CLAMP_TO_BORDER`（边界色）、`MIRRORED_REPEAT`（镜像平铺） |

---

### VkFramebufferCreateInfo

```cpp
VkFramebufferCreateInfo fi{};
fi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
fi.renderPass      = renderPass_;              // (1) 必须与使用时的 RenderPass 兼容
fi.attachmentCount = 2;                        // (2) 附件数量
fi.pAttachments    = attachments;              // (3) 顺序必须与 RenderPass 的 AttachmentDescription 一致
fi.width           = swapchainExtent.width;    // (4)
fi.height          = swapchainExtent.height;   // (5)
fi.layers          = 1;                        // (6)
```

| 字段 | 含义 |
|------|------|
| `renderPass` (1) | Framebuffer 兼容的 RenderPass。**RenderPass + Framebuffer 必须兼容**：Attachment 数量、格式、samples 必须匹配 |
| `pAttachments` (3) | VkImageView 数组。`[0]` 对应 RenderPass 的 `pAttachments[0]`（Color），`[1]` 对应 Depth |

---

## 六、Command / Sync 阶段

### VkCommandBufferAllocateInfo

```cpp
VkCommandBufferAllocateInfo ai{};
ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
ai.commandPool        = commandPool_;                              // (1)
ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;          // (2)
ai.commandBufferCount = 2;                                         // (3)
```

| 字段 | 含义 |
|------|------|
| `commandPool` (1) | 从哪个 Pool 分配 |
| `level` (2) | `PRIMARY`：可以直接提交到 Queue。`SECONDARY`：只能被 Primary 调用（用于多线程录制） |
| `commandBufferCount` (3) | 分配几个 CommandBuffer |

---

### VkSemaphoreCreateInfo

```cpp
VkSemaphoreCreateInfo si{};
si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
// 没有其他字段 — Semaphore 是无类型的 GPU-GPU 信号
```

---

### VkFenceCreateInfo

```cpp
VkFenceCreateInfo fi{};
fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;     // (1)
```

| 字段 | 含义 |
|------|------|
| `flags` (1) | `SIGNALED_BIT`：初始状态为"已触发"。这样第一帧不会因为等 Fence 而卡住。不带此标志时首次 `vkWaitForFences` 会立即返回超时（0 ns） |

**Semaphore vs Fence：**
- Semaphore：GPU-GPU 同步。在 QueueSubmit 中 signal，在下一个 QueueSubmit 中 wait。CPU 不能等待 Semaphore
- Fence：GPU-CPU 同步。CPU 可以 `vkWaitForFences`。GPU 在 QueueSubmit 中 signal

---

### VkCommandBufferBeginInfo

```cpp
VkCommandBufferBeginInfo bi{};
bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // (1) 一次性提交
```

| 字段 | 含义 |
|------|------|
| `flags` (1) | `ONE_TIME_SUBMIT`：告诉驱动这个 CommandBuffer 只提交一次就废弃。驱动可以利用这个做优化（减少内部状态保存）。`SIMULTANEOUS_USE`：可以同时被多个 Queue 执行。`RENDER_PASS_CONTINUE`：用于 Secondary CommandBuffer |

---

### VkRenderPassBeginInfo

```cpp
VkRenderPassBeginInfo rp{};
rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
rp.renderPass  = renderPass_;
rp.framebuffer = swapchainFramebuffers_[imageIndex];  // (1)
rp.renderArea  = { {0,0}, swapchainExtent };           // (2)
rp.clearValueCount = 2;                                // (3)
rp.pClearValues    = clears;                           // (4)
```

| 字段 | 含义 |
|------|------|
| `framebuffer` (1) | 本次使用的 Framebuffer。每帧轮换到不同的 swapchainFramebuffer |
| `renderArea` (2) | 渲染区域。`offset={0,0}, extent=窗口大小`（全屏渲染） |
| `clearValueCount` (3) | 清除值数量。必须等于 RenderPass 中所有 `loadOp=CLEAR` 的 Attachment 数量 |
| `pClearValues` (4) | 清除值数组。顺序与 Attachment 一致：`[0]` = Color clear value，`[1]` = Depth clear value。**Color 用 Union 格式**：`clearValues[0].color = {{r,g,b,a}}`。Depth 用：`clearValues[1].depthStencil = {depth, stencil}` |

---

### VkImageMemoryBarrier

**文件：** `ForwardPass.cpp:440` — 纹理上传时的 Layout 转换

```cpp
VkImageMemoryBarrier bar{};
bar.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;   // (1)
bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;   // (2)
bar.image               = textureImage_;              // (3)
bar.subresourceRange    = { COLOR_BIT, 0, 1, 0, 1 }; // (4)
bar.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;  // (5)
bar.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // (6)
bar.srcAccessMask       = 0;                          // (7)
bar.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT; // (8)
```

| 字段 | 含义 |
|------|------|
| `srcQueueFamilyIndex` (1) | 如果要在不同 Queue Family 之间转移所有权，填源族索引。`IGNORED` 表示不转移所有权 |
| `dstQueueFamilyIndex` (2) | 目标族索引 |
| `image` (3) | 要转换布局的 Image |
| `subresourceRange` (4) | 影响哪些 mip levels 和 array layers。`{aspect, baseMip, levelCount, baseLayer, layerCount}` = 整个 Image |
| `oldLayout` (5) | 当前布局。`UNDEFINED` = 不在乎之前的内容（GPU 可以不做任何转换） |
| `newLayout` (6) | 目标布局。`TRANSFER_DST_OPTIMAL` = 准备接受 Image Copy。`SHADER_READ_ONLY_OPTIMAL` = 准备被 Shader 采样 |
| `srcAccessMask` (7) | 哪些内存访问必须在 Barrier 之前完成。`0` = 无需等待 |
| `dstAccessMask` (8) | 哪些内存访问必须等 Barrier 之后才能开始。`TRANSFER_WRITE` = 在 `vkCmdCopyBufferToImage` 写入前必须完成布局转换 |

**Layout 转换链（纹理上传）：**
```
UNDEFINED  → [Barrier 1] → TRANSFER_DST_OPTIMAL  → [vkCmdCopyBufferToImage]
→ [Barrier 2] → SHADER_READ_ONLY_OPTIMAL  → [Shader 采样]
```

每个 `vkCmdPipelineBarrier` 都是一个 GPU 执行屏障——它之前的所有命令必须完成，之后的所有命令才能开始。这保证了：
1. Copy 命令之前 Image 在正确布局
2. Shader 采样之前 Copy 已完成且 Image 在正确布局

---

### VkBufferImageCopy

```cpp
VkBufferImageCopy region{};
region.bufferOffset      = 0;
region.bufferRowLength   = 0;                          // (1) 0 = 紧密打包
region.bufferImageHeight = 0;                          // (2) 0 = 紧密打包
region.imageSubresource  = { COLOR_BIT, 0, 0, 1 };    // (3)
region.imageExtent       = { width, height, 1 };        // (4)
```

| 字段 | 含义 |
|------|------|
| `bufferRowLength` (1) | Buffer 中每一行像素的字节长度。0 = 等于 imageExtent.width（紧密打包，无 padding） |
| `bufferImageHeight` (2) | Buffer 中每一张图像的高度。0 = 等于 imageExtent.height |
| `imageSubresource` (3) | Image 的哪个 subresource（aspect + mip + layer） |
| `imageExtent` (4) | 复制的区域大小 |

---

### VkSubmitInfo

```cpp
VkSubmitInfo si{};
si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
si.waitSemaphoreCount   = 1;
si.pWaitSemaphores      = &imageAvailableSemaphore;           // (1)
si.pWaitDstStageMask    = &waitStage;                         // (2)
si.commandBufferCount   = 1;
si.pCommandBuffers      = &cmd;                               // (3)
si.signalSemaphoreCount = 1;
si.pSignalSemaphores    = &renderFinishedSemaphore;           // (4)
```

| 字段 | 含义 |
|------|------|
| `pWaitSemaphores` (1) | GPU 等待这些 Semaphore 被 signal 后才开始执行 CommandBuffer |
| `pWaitDstStageMask` (2) | 在哪个 Pipeline Stage 等。`COLOR_ATTACHMENT_OUTPUT`：等到可以写入颜色附件时。**这是关键**——你可以让某些阶段不等（如 Vertex Shader 可以提前开始），但通常全等 |
| `pCommandBuffers` (3) | 提交的 CommandBuffer 列表 |
| `pSignalSemaphores` (4) | CommandBuffer 执行完毕后 signal 这些 Semaphore |

---

### VkPresentInfoKHR

```cpp
VkPresentInfoKHR pi{};
pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
pi.waitSemaphoreCount = 1;
pi.pWaitSemaphores    = &renderFinishedSemaphore;             // (1)
pi.swapchainCount     = 1;
pi.pSwapchains        = &swapchain_;                          // (2)
pi.pImageIndices      = &imageIndex;                          // (3)
```

| 字段 | 含义 |
|------|------|
| `pWaitSemaphores` (1) | 等渲染完成的 Semaphore |
| `pSwapchains` (2) | 呈现目标 |
| `pImageIndices` (3) | 呈现哪个 Image |

**Present 的返回值处理：**
- `VK_SUCCESS`：正常
- `VK_SUBOPTIMAL_KHR`：窗口大小变了但仍可呈现（应立即重建 swapchain 来避免画面拉伸）
- `VK_ERROR_OUT_OF_DATE_KHR`：swapchain 无效（窗口 resize 或最小化后恢复），必须重建

---

## 七、Copy 操作

### VkBufferCopy

```cpp
VkBufferCopy copy{};
copy.srcOffset = 0;
copy.dstOffset = 0;
copy.size      = bufferSize;
```

用于 `vkCmdCopyBuffer(stagingBuf, gpuBuf, 1, &copy)` —— 将 Staging Buffer 的内容复制到 GPU-local Buffer。

---

## 八、查询函数

这些函数不创建对象，只查询 GPU 信息：

| 函数 | 查询什么 |
|------|---------|
| `vkEnumerateInstanceExtensionProperties` | 系统支持的 Instance 扩展 |
| `vkEnumerateInstanceLayerProperties` | 系统支持的 Layer |
| `vkEnumeratePhysicalDevices` | 所有物理 GPU |
| `vkGetPhysicalDeviceProperties` | GPU 名称、驱动版本、API 版本、各种 limit |
| `vkGetPhysicalDeviceFeatures` | 可选特性支持情况 |
| `vkGetPhysicalDeviceQueueFamilyProperties` | 每个 Queue Family 的能力 |
| `vkGetPhysicalDeviceSurfaceSupportKHR` | 指定物理设备 + Queue Family 是否支持 Present |
| `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` | Swapchain 能力（min/max image count, extent, transform） |
| `vkGetPhysicalDeviceSurfaceFormatsKHR` | Surface 支持的格式+色彩空间组合 |
| `vkGetPhysicalDeviceSurfacePresentModesKHR` | Surface 支持的呈现模式 |
| `vkGetPhysicalDeviceFormatProperties` | 指定格式的特性（是否支持采样、混合、深度等） |
| `vkEnumerateDeviceExtensionProperties` | 指定设备支持的 Device 扩展 |
| `vkGetPhysicalDeviceMemoryProperties` | 内存类型和 Heap 信息 |
| `vkGetBufferMemoryRequirements` | Buffer 的内存需求 |
| `vkGetImageMemoryRequirements` | Image 的内存需求 |
| `vkGetSwapchainImagesKHR` | Swapchain 中的所有 Image 句柄 |
