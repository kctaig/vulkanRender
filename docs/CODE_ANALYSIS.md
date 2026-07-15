# 项目代码详细分析

## 目录结构

```
├── include/                          # 公共头文件
│   ├── core/
│   │   ├── Application.h            # 应用入口类
│   │   └── VulkanContext.h          # Vulkan 核心封装（设备/交换链/内存）
│   ├── renderer/
│   │   ├── RenderPass.h             # Pass 抽象接口
│   │   ├── Renderer.h               # 帧循环编排器
│   │   └── ForwardPass.h            # 前向渲染 Pass
│   └── scene/
│       ├── Camera.h                 # 轨道摄像机
│       ├── Scene.h                  # 场景数据容器
│       └── MeshIO.h                 # OBJ 模型加载
├── src/                              # 实现文件（与 include/ 一一对应）
│   ├── main.cpp
│   ├── core/
│   │   ├── Application.cpp
│   │   └── VulkanContext.cpp
│   ├── renderer/
│   │   ├── ForwardPass.cpp
│   │   └── Renderer.cpp
│   └── scene/
│       └── MeshIO.cpp
├── shaders/
│   ├── triangle.vert                 # 顶点着色器：MVP 变换
│   └── triangle.frag                 # 片段着色器：法线颜色
├── assets/models/                    # OBJ 模型文件
├── docs/
│   ├── ARCHITECTURE_ROADMAP.md       # 架构演进路线
│   └── CODE_ANALYSIS.md              # 本文档
└── CMakeLists.txt
```

---

## 一、整体架构

项目遵循分层 Pass 架构，核心原则是 **依赖单向**：上层依赖下层，下层不感知上层。

```
Application              ← 入口：组装顶层组件，解析命令行参数
    │
Renderer                 ← 编排层：窗口 + 帧循环 + Pass 列表 + 同步原语
    │
    ├── VulkanContext    ← 基础层：Vulkan 对象生命周期 + 资源工厂方法
    ├── Scene            ← 数据层：摄像机 + 光源 + 材质
    └── Pass[]           ← 渲染层：每个 Pass 独立实现一种渲染功能
        └── ForwardPass
```

### 各层职责

| 层 | 文件 | 职责 |
|----|------|------|
| 入口 | `Application.h/.cpp` | 命令行解析、组装 Renderer、调用 mainLoop |
| 编排 | `Renderer.h/.cpp` | Win32 窗口、消息循环、帧级同步、Pass 生命周期、鼠标→Camera 转发 |
| 基础 | `VulkanContext.h/.cpp` | Instance/Device/Swapchain/CommandPool 创建销毁、Buffer/Image/Shader 工厂方法、物理设备查询 |
| 数据 | `Scene.h` `Camera.h` | 场景数据结构（光源/材质）、轨道摄像机数学 |
| 渲染 | `ForwardPass.h/.cpp` | RenderPass、GraphicsPipeline、Descriptor、顶点/索引/UBO 缓冲、网格加载 |
| 接口 | `RenderPass.h` | 纯虚基类：`initialize()` `record()` `onSwapchainResize()` `shutdown()` |

---

## 二、类详解

### 2.1 VulkanContext — Vulkan 核心封装

**路径**: `include/core/VulkanContext.h` / `src/core/VulkanContext.cpp`

**职责**: 封装所有 Vulkan 对象创建、销毁、查询。不包含任何渲染逻辑。

**内部结构体**:

- `CreateInfo` — 初始化参数（窗口句柄、初始分辨率、是否启用 Validation Layer）
- `QueueFamilyIndices` — 队列族索引（graphics + present），含 `isComplete()` 判断
- `SwapchainSupportDetails` — 交换链支持信息（surface 能力、格式、呈现模式）

**公开接口分组**:

| 分组 | 方法 | 说明 |
|------|------|------|
| 初始化/销毁 | `initialize(CreateInfo)` `shutdown()` | 按顺序创建所有 Vulkan 对象，shutdown 逆序销毁 |
| 核心访问器 | `instance()` `physicalDevice()` `device()` `graphicsQueue()` `presentQueue()` `surface()` `commandPool()` `swapchain()` | 对外暴露底层 Vulkan 句柄 |
| 交换链访问器 | `swapchainFormat()` `swapchainExtent()` `swapchainImageCount()` `swapchainImages()` `swapchainImageViews()` `swapchainMinImageCount()` | 交换链状态查询 |
| 交换链操作 | `acquireNextImage()` `recreateSwapchain()` `cleanupSwapchain()` | 图像获取 + 重建（仅清理 swapchain 自身） |
| 帧缓冲尺寸 | `setFramebufferSize(w, h)` | 窗口 resize 时更新，供 `chooseSwapExtent` 回退使用 |
| 资源工厂 | `createBuffer()` `createImage()` `createImageView()` `createShaderModule()` | 创建 Vulkan 资源并分配/绑定内存 |
| 查询工具 | `findMemoryType()` `findSupportedFormat()` `findDepthFormat()` `findQueueFamilies()` `querySwapchainSupport()` | 物理设备能力查询 |
| CommandBuffer | `allocateCommandBuffers(count)` | 从 CommandPool 分配 |
| 静态工具 | `readBinaryFile(path)` | 读取文件到 `vector<char>`（SPIR-V 加载用） |

**初始化流程**（`initialize()` 内部调用链）:

```
createInstance()           → VkInstance + 可选 ValidationLayer + DebugMessenger
createSurface()            → VkSurfaceKHR (Win32)
pickPhysicalDevice()       → 遍历设备，选第一个满足条件的
createLogicalDevice()      → VkDevice + 获取 Graphics/Present Queue
createCommandPool()        → VkCommandPool (GRAPHICS 队列族, RESET 标志)
createSwapchain()          → VkSwapchainKHR (Mailbox/FIFO, BGRA8_sRGB)
createImageViews()         → 每个 Swapchain Image 创建 VkImageView
```

**设备选择条件**（`isDeviceSuitable`）:
- 队列族同时支持 Graphics 和 Present
- 支持 `VK_KHR_SWAPCHAIN_EXTENSION_NAME`
- 至少一个 Surface Format 和一个 Present Mode

**交换链偏好**:
- Format: `VK_FORMAT_B8G8R8A8_SRGB` + `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`
- Present Mode: `VK_PRESENT_MODE_MAILBOX_KHR`（不支持则回退 FIFO）
- Image Count: `minImageCount + 1`（不超过 maxImageCount）

**Validation Layer**: 由 CMake 宏 `VR_ENABLE_VALIDATION` 控制，Debug 构建时启用。回调输出到 stderr，过滤 WARNING 和 ERROR。

**文件依赖**:
- 使用匿名命名空间存放内部常量（`kDeviceExtensions`、`kInstanceExtensions`、`kValidationLayers`）和辅助函数（`isInstanceExtensionSupported`、`populateDebugMessengerCreateInfo`、`areValidationLayersSupported`、`hasStencilComponent`）
- 所有 Vulkan 对象初始化为 `VK_NULL_HANDLE`，销毁前检查非空

---

### 2.2 RenderPass — 抽象接口

**路径**: `include/renderer/RenderPass.h`

**职责**: 定义所有渲染 Pass 的统一接口。约 20 行代码。

```cpp
class RenderPass {
  public:
    static constexpr std::uint32_t kMaxFramesInFlight = 2;

    virtual bool initialize(VulkanContext& ctx) = 0;
    virtual void record(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t imageIndex) = 0;
    virtual void onSwapchainResize(VulkanContext& ctx) = 0;
    virtual void shutdown() = 0;
};
```

**四个抽象方法的语义**:

| 方法 | 调用时机 | 职责 |
|------|----------|------|
| `initialize(ctx)` | 程序启动时，只调用一次 | 创建 RenderPass、Pipeline、Descriptor、Framebuffer 等不变资源 |
| `record(cmd, frame, image)` | 每帧调用 | 更新 UBO、录制 vkCmd* 到 CommandBuffer |
| `onSwapchainResize(ctx)` | 窗口 resize 后 | 销毁依赖 Swapchain Extent 的资源，然后重建 |
| `shutdown()` | 程序退出时 | 销毁所有 Vulkan 资源（在 `vkDeviceWaitIdle` 之后调用） |

**`kMaxFramesInFlight`**: 放在基类中作为唯一权威定义。ForwardPass 通过继承直接使用，Renderer 通过 `RenderPass::kMaxFramesInFlight` 引用。

**设计要点**:
- `initialize` 接收 `VulkanContext&` 而非指针 —— 调用者保证 ctx 生命周期长于 Pass
- `record` 不接收 `VulkanContext` —— Pass 应在 `initialize` 时保存需要的指针（`ctx_`）
- `onSwapchainResize` 接收 `VulkanContext&` —— 重建时需要访问更新后的 swapchain 状态

---

### 2.3 Renderer — 帧循环编排器

**路径**: `include/renderer/Renderer.h` / `src/renderer/Renderer.cpp`

**职责**: 窗口管理 + 帧循环 + Pass 生命周期的纯编排层。**不包含任何渲染逻辑**。

#### 成员变量

| 分组 | 成员 | 说明 |
|------|------|------|
| 核心 | `ctx_` (VulkanContext) | Vulkan 设备/交换链 |
| 场景 | `scene_` (Scene) | 摄像机 + 光源数据 |
| Pass | `passes_` (vector<unique_ptr<RenderPass>>) | 多态 Pass 列表 |
| 窗口 | `windowHandle_` `instanceHandle_` | Win32 窗口句柄 |
| 输入 | `rightDragActive_` `leftDragActive_` `lastMousePosition_` | 鼠标拖拽状态 |
| 同步 | `imageAvailableSemaphores_[]` `renderFinishedSemaphores_[]` `inFlightFences_[]` `imagesInFlight_[]` | 帧级 GPU 同步 |
| 命令 | `commandBuffers_[]` | 主 CommandBuffer（每帧一个） |

#### 公开方法

| 方法 | 说明 |
|------|------|
| `initialize(w, h)` | 创建窗口 → 初始化 ctx_ → 创建 ForwardPass → 分配 CommandBuffer → 创建同步对象 |
| `setMeshInputPath(path)` | 命令行传入模型路径，通过 `dynamic_cast<ForwardPass*>` 转发 |
| `mainLoop()` | `while(running) { processMessages(); drawFrame(); }` |
| `shutdown()` | 逆序销毁：Pass→同步对象→ctx_→窗口 |

#### drawFrame() 帧循环流程

```
vkWaitForFences(inFlightFences_[currentFrame_])     ← 等上一帧 GPU 完成
ctx_.acquireNextImage(semaphore, imageIndex)        ← 获取可用的 Swapchain Image
vkWaitForFences(imagesInFlight_[imageIndex])        ← 等该 image 的旧帧完成
vkResetFences(inFlightFences_[currentFrame_])
vkResetCommandBuffer(cmd)
vkBeginCommandBuffer(cmd)

for each pass:
    pass->record(cmd, currentFrame_, imageIndex)    ← Pass 录制渲染命令

vkEndCommandBuffer(cmd)
vkQueueSubmit(... inFlightFences_[currentFrame_])   ← 提交 GPU 执行
vkQueuePresentKHR(... imageIndex)                   ← 呈现
currentFrame_ = (currentFrame_ + 1) % 2
```

**同步模型**: 双缓冲（kMaxFramesInFlight = 2）
- `inFlightFences_[frame]` — 保护 CommandBuffer 不被覆盖（CPU→GPU）
- `imageAvailableSemaphores_[frame]` — 等待 Swapchain 释放 Image（GPU→GPU）
- `renderFinishedSemaphores_[image]` — 等待渲染完成再 Present（GPU→GPU）
- `imagesInFlight_[image]` — 跟踪每个 Swapchain Image 正在执行的帧

#### 鼠标输入 → Camera

```
WM_RBUTTONDOWN → rightDragActive_ = true
WM_RBUTTONUP   → rightDragActive_ = false
WM_MOUSEMOVE + rightDragActive_  → scene_.camera.rotate(dx, dy)
WM_MOUSEMOVE + leftDragActive_   → scene_.camera.pan(dx, dy)
WM_MOUSEWHEEL                    → scene_.camera.zoom(delta)
```

Renderer **不感知 ForwardPass**（仅通过 `RenderPass*` 接口调用 `record`/`onSwapchainResize`/`shutdown`），`dynamic_cast` 仅用于 `setMeshInputPath` 的命令行参数转发。

---

### 2.4 ForwardPass — 前向渲染 Pass

**路径**: `include/renderer/ForwardPass.h` / `src/renderer/ForwardPass.cpp`

**职责**: 实现经典的 Forward Rendering（前向渲染）—— 一个 RenderPass、一个 Pipeline，每个物体一次 DrawCall。所有几何渲染逻辑集中在此。

#### 内部类型

- `Vertex` — 顶点格式：`position(vec3) + normal(vec3) + uv(vec2)`，含静态方法生成 `VkVertexInputBindingDescription` 和 `VkVertexInputAttributeDescription`
- `UniformBufferObject` — UBO 布局：`model(mat4) + view(mat4) + projection(mat4)`

#### 公开方法（RenderPass 接口 + 扩展）

| 方法 | 说明 |
|------|------|
| `initialize(ctx)` | 创建所有渲染资源，顺序见下方 |
| `record(cmd, frame, image)` | 更新 UBO → BeginRenderPass → BindPipeline → BindVB/IB → BindDescriptor → DrawIndexed → EndRenderPass |
| `onSwapchainResize(ctx)` | 销毁 Framebuffer/Depth/Pipeline/RenderPass 然后重建 |
| `shutdown()` | 销毁所有 Vulkan 资源 |
| `setScene(scene)` | 存储 Scene 指针，用于读取 Camera 矩阵 |
| `setMeshInputPath(path)` | 设置模型路径（下次 initialize 时加载） |
| `addModelYaw/Pitch/Translation` | 模型变换控制（供 UI 层使用，当前未绑定鼠标） |

#### 资源创建顺序（`initialize` 内部）

```
createRenderPass(ctx)           # 2 个 Attachment: Color(Swapchain格式) + Depth
createDepthResources(ctx)       # Depth Image + ImageView
createFramebuffers(ctx)         # 每个 Swapchain Image 一个 Framebuffer
createDescriptorSetLayout(ctx)  # Binding=0, UniformBuffer, Vertex Stage
createUniformBuffers(ctx)       # 2 个 UBO（双缓冲）
createDescriptorPool(ctx)       # UniformBuffer × 2
createDescriptorSets(ctx)       # 分配 + 写入 UBO 绑定
createGraphicsPipeline(ctx)     # Shader → Pipeline Layout → GraphicsPipeline
createVertexBuffer(ctx)         # 加载网格 → 上传 GPU
createIndexBuffer(ctx)
```

#### 渲染状态

| 状态 | 值 |
|------|----|
| 图元拓扑 | `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` |
| 多边形模式 | `VK_POLYGON_MODE_FILL` |
| 背面剔除 | `VK_CULL_MODE_BACK_BIT` |
| 正面方向 | `VK_FRONT_FACE_COUNTER_CLOCKWISE`（glm 默认） |
| 深度测试 | 启用, compareOp = LESS |
| 混合 | 关闭（colorWriteMask 全通道） |
| 多重采样 | 1 sample（无 MSAA） |
| 动态状态 | Viewport + Scissor |

#### UpdateUniformBuffer 逻辑

```cpp
// Model 矩阵: 平移 + Y轴旋转 + X轴旋转
model = translate(I, modelTranslation_)
      * rotate(I, modelYawRadians_, Y)
      * rotate(I, modelPitchRadians_, X)

// View 矩阵: 从 Scene 的 Camera 读取
camera.setPerspective(fov, aspect, near, far)  // 每帧根据 extent 更新 aspect
view = camera.viewMatrix()      // orbit: lookAt(pos, target, up)
projection = camera.projectionMatrix()
projection[1][1] *= -1          // Vulkan Y 轴翻转
```

#### 网格加载

1. 尝试加载 `meshInputPath_`（如果有命令行参数）
2. 回退加载 `assets/models/basic_mesh.obj`
3. 如果都失败，使用硬编码的 fallback 三角形（3 顶点, 1 面）
4. 计算 `sceneRadius_`（包围球半径），同步到 `camera.setMaxDistance()`

#### 与 VulkanContext 的交互模式

ForwardPass 在 `initialize` 时保存 `ctx_` 指针。私有 create 方法接收 `VulkanContext& ctx` 参数（因为 `onSwapchainResize` 时使用的是更新后的 ctx），但在 `record` 和 `shutdown` 中使用保存的 `ctx_->` 指针。

---

### 2.5 Camera — 轨道摄像机

**路径**: `include/scene/Camera.h`（纯头文件实现）

**职责**: 封装轨道（Orbit）摄像机的数学逻辑，所有方法 inline。

**内部状态**:

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `target_` | (0,0,0) | 旋转中心点 |
| `distance_` | 3.5 | 到目标的距离 |
| `yaw_` | 0 | 水平旋转角（弧度） |
| `pitch_` | 0 | 垂直旋转角（弧度），clamp ±1.5 |
| `fov_` | 45° | 视场角（弧度） |
| `aspect_` | 16:9 | 宽高比 |
| `nearPlane_` / `farPlane_` | 0.1 / 100 | 近 / 远裁剪面 |
| `maxDistance_` | 100 | 最大距离（由场景包围球决定） |

**公开方法**:

| 方法 | 说明 |
|------|------|
| `rotate(dYaw, dPitch)` | 增量旋转，pitch 自动 clamp |
| `zoom(delta)` | 改变距离，自动 clamp 到 [0.1, maxDistance] |
| `pan(delta)` | 沿 right/up 方向平移 target |
| `setPerspective(fov, aspect, near, far)` | 更新投影参数（每帧根据 swapchain extent 调用） |
| `viewMatrix()` | 计算 lookAt 矩阵 |
| `projectionMatrix()` | 计算 perspective 矩阵 |
| `setMaxDistance(d)` | 设置最大距离（场景加载后调用） |

**forward() 计算**: 球坐标 → 方向向量
```cpp
forward = (sin(yaw)*cos(pitch), sin(pitch), -cos(yaw)*cos(pitch))
```

**viewMatrix()**: 
```cpp
position = target - forward * distance
return lookAt(position, target, worldUp=(0,1,0))
```

---

### 2.6 Scene — 场景数据容器

**路径**: `include/scene/Scene.h`

**职责**: 持有摄像机实例和场景级数据列表。纯数据结构，无渲染逻辑。

**成员**:

| 成员 | 类型 | 说明 |
|------|------|------|
| `camera` | Camera | 轨道摄像机实例 |
| `directionalLights` | vector<DirectionalLight> | 定向光源列表 |
| `pointLights` | vector<PointLight> | 点光源列表 |
| `materials` | vector<Material> | 材质列表 |

**结构体定义**:

| 结构体 | 字段 |
|--------|------|
| `DirectionalLight` | direction, color, intensity |
| `PointLight` | position, color, intensity, range |
| `Material` | name, albedo, metallic, roughness, ao |
| `MeshInstance` | transform（mat4） |

---

### 2.7 MeshIO — OBJ 模型加载

**路径**: `include/scene/MeshIO.h` / `src/scene/MeshIO.cpp`

**输出数据结构**:

```cpp
struct MeshVertexInput { vec3 position; vec3 normal; vec2 uv; };
struct MeshInputData { vector<MeshVertexInput> vertices; vector<uint32_t> indices; };
```

**公开函数**: `bool loadObjMesh(const string& filePath, MeshInputData& out)`

使用简单的逐行解析（非 tinyobjloader），支持 `v`（顶点）、`vt`（纹理坐标）、`vn`（法线）、`f`（面）四种 OBJ 指令。返回 `true` 表示加载成功。

---

### 2.8 Application — 应用入口

**路径**: `include/core/Application.h` / `src/core/Application.cpp`

**职责**: 解析命令行参数，创建 Renderer，启动主循环。

```cpp
int Application::run(int argc, char** argv) {
    if (argc > 1) renderer_.setMeshInputPath(argv[1]);
    renderer_.initialize(1600, 900);
    renderer_.mainLoop();
    renderer_.shutdown();
    return 0;
}
```

---

## 三、关键数据流

### 3.1 初始化流

```
main()
  Application::run()
    Renderer::initialize(w, h)
      initWindow()                     ← Win32 窗口
      VulkanContext::initialize()      ← Instance → Device → Swapchain
      ForwardPass::initialize(ctx)     ← RenderPass → Pipeline → Buffers
      ForwardPass::setScene(scene_)    ← 关联场景数据
      分配 CommandBuffers
      创建 Semaphores + Fences
```

### 3.2 每帧渲染流

```
Renderer::drawFrame()
  │
  ├─ vkWaitForFences              ← CPU 等上一帧完成
  ├─ ctx_.acquireNextImage()     ← GPU 释放 Swapchain Image
  ├─ vkBeginCommandBuffer(cmd)
  │
  ├─ ForwardPass::record(cmd, frame, image)
  │     ├─ updateUniformBuffer() ← 从 scene_->camera 读取 view/proj
  │     ├─ vkCmdBeginRenderPass
  │     ├─ vkCmdBindPipeline
  │     ├─ vkCmdSetViewport / SetScissor
  │     ├─ vkCmdBindVertexBuffers / BindIndexBuffer
  │     ├─ vkCmdBindDescriptorSets
  │     └─ vkCmdDrawIndexed → vkCmdEndRenderPass
  │
  ├─ vkEndCommandBuffer(cmd)
  ├─ vkQueueSubmit              ← 提交到 Graphics Queue
  └─ vkQueuePresentKHR          ← 呈现到屏幕
```

### 3.3 窗口 Resize 流

```
WM_SIZE → framebufferResized_ = true
  │
Renderer::drawFrame()
  └─ vkQueuePresentKHR 返回 OUT_OF_DATE / framebufferResized_
      │
      Renderer::recreateSwapchain()
        ├─ ctx_.setFramebufferSize(w, h)
        ├─ ctx_.recreateSwapchain()      ← 重建 swapchain + imageViews
        ├─ pass->onSwapchainResize(ctx)  ← 每个 Pass 重建依赖资源
        └─ 重建 renderFinishedSemaphores_
```

### 3.4 鼠标输入流

```
WM_MOUSEMOVE / WM_MOUSEWHEEL
  │
Renderer::handleWindowMessage()
  ├─ rightDrag  → scene_.camera.rotate()
  ├─ leftDrag   → scene_.camera.pan()
  └─ wheel      → scene_.camera.zoom()
      │
      Camera 内部更新 yaw_/pitch_/target_/distance_
      │
      下一帧 ForwardPass::updateUniformBuffer()
        └─ 读取 camera.viewMatrix() / projectionMatrix()
```

---

## 四、设计决策

### 4.1 为什么 VulkanContext 在 core/ 而不是 renderer/

VulkanContext 只封装 Vulkan API，不包含任何渲染概念（没有 RenderPass、Pipeline、Descriptor）。它是可复用的基础设施层，任何 Vulkan 程序都需要。`renderer/` 下的所有类依赖 `core/`，反向不成立。

### 4.2 为什么 Renderer 不持有 ForwardPass 指针

Renderer 仅通过 `RenderPass*` 接口操作 Pass，不需要知道具体类型。`dynamic_cast<ForwardPass*>` 只在 `setMeshInputPath` 中出现，这是命令行参数转发的权宜之计——未来场景系统完善后，模型加载由 Scene 管理，此调用可删除。

### 4.3 为什么 Camera 是纯头文件实现

Camera 的所有方法都是简单的数学运算（三角函数、矩阵乘法），放在头文件中允许编译器内联优化。不涉及 IO 或平台 API，不需要单独的 .cpp 文件。

### 4.4 为什么 kMaxFramesInFlight 放在 RenderPass 中

这是 Pass 和 Renderer 都需要引用的常量。放在基类中提供唯一权威定义，ForwardPass 通过继承直接访问，Renderer 通过 `RenderPass::kMaxFramesInFlight` 引用。改一处全局生效。

### 4.5 为什么 Scene 的成员是 public

当前 Scene 是纯数据容器，没有需要保护的不变量。光源/材质列表直接公开访问比封装 getter/setter 更简洁。未来如需添加验证逻辑（如光源数量上限），可以改为 private + 访问器。

### 4.6 资源销毁顺序

所有 Vulkan 资源在 `vkDeviceWaitIdle` 之后销毁。销毁顺序遵循依赖关系：
1. Pass 级资源（Framebuffer → Pipeline → RenderPass → Descriptor → Buffer）
2. 帧级同步对象（Semaphore → Fence）
3. VulkanContext（CommandPool → Device → Surface → Instance）
4. Win32 窗口

---

## 五、文件行数统计

| 文件 | 行数 | 类型 |
|------|------|------|
| `VulkanContext.h` | 157 | 头文件（声明） |
| `VulkanContext.cpp` | 820 | 实现 |
| `RenderPass.h` | 24 | 接口 |
| `Renderer.h` | 63 | 头文件 |
| `Renderer.cpp` | 374 | 实现 |
| `ForwardPass.h` | 106 | 头文件 |
| `ForwardPass.cpp` | 657 | 实现 |
| `Camera.h` | 77 | 纯头文件 |
| `Scene.h` | 48 | 数据结构 |
| `MeshIO.h` | 25 | 头文件 |
| `MeshIO.cpp` | ~120 | 实现 |
| `Application.h` | 16 | 头文件 |
| `Application.cpp` | 27 | 实现 |
| **合计** | **~2500** | |
