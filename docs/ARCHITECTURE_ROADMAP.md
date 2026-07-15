# Vulkan Renderer 架构演进路线

## 当前实际状态

```
Scene (数据中心, Renderer 持有)
├── Camera              ← 轨道摄像机（球坐标, rotate/zoom/pan）
├── AssetManager        ← assimp + stb_image, 一键导入 OBJ/FBX/glTF
├── Instance[]          ← { modelId, transform }
├── DirectionalLight[]  ← 定向光列表
└── PointLight[]        ← 点光源列表

ForwardPass (只读 Scene*, 各 Pass 独立)
├── RenderPass / Pipeline / Descriptor / Framebuffer
├── 棋盘格默认纹理（256×256）
├── record() 遍历 scene_->instances 逐个 DrawIndexed
└── updateUniformBuffer() 从 Camera 读取 view/projection

Renderer (纯编排, 不碰渲染逻辑)
├── Window + 消息循环
├── VulkanContext（设备/交换链/内存分配/CommandPool）
├── Scene
├── vector<unique_ptr<RenderPass>>
├── 帧级同步（Semaphore/Fence, 双缓冲）
└── 鼠标 → Camera::rotate/pan/zoom
```

### 实际目录结构

```
include/
├── core/
│   ├── Application.h          # 入口，组装 Renderer
│   └── VulkanContext.h        # Instance/Device/Swapchain/内存工厂
├── renderer/
│   ├── RenderPass.h           # 抽象接口 + kMaxFramesInFlight
│   ├── Renderer.h             # 窗口 + 帧循环 + 同步
│   └── ForwardPass.h          # 前向渲染 Pass
└── scene/
    ├── Camera.h               # 轨道摄像机（纯头文件 inline）
    ├── Scene.h                # 数据容器（Camera + AssetManager + 实例列表）
    └── AssetManager.h         # assimp 模型导入 + stb 纹理上传

src/
├── main.cpp
├── core/
│   ├── Application.cpp
│   └── VulkanContext.cpp
├── renderer/
│   ├── ForwardPass.cpp
│   └── Renderer.cpp
└── scene/
    └── AssetManager.cpp

3dparty/
├── glm/                       # GLM 头文件库
├── stb_image/stb_image.h      # 纹理加载
└── assimp/                    # 模型导入 SDK（待安装）

shaders/
├── forwardPass.vert           # MVP + 法线 → 片元
└── forwardPass.frag           # 纹理采样 + 简单漫反射
```

### 已完成 Phase 总结

| Phase | 内容 | 状态 |
|-------|------|:----:|
| P0 | 抽离 VulkanContext — 设备/交换链/内存/CommandPool 独立 | 完成 |
| P1 | 场景模型 — Camera + Scene + Light + AssetManager | 完成 |
| P2 | RenderPass 抽象 + ForwardPass — Pass 接口 + 前向渲染实现 | 完成 |
| P3 | 纹理加载 + Material — stb_image + 默认棋盘格纹理 + assimp 模型导入 | 完成 |
| — | 目录分离 include/ / src/，build 系统整理 | 完成 |

---

## 下一步改进方向

### A. Window / Input 分离（估时：中）

Renderer 当前混合窗口管理、输入处理和帧编排。拆为三个类：

```cpp
class Window {                    // 纯 Win32，不依赖 Vulkan
    bool create(w, h, title);
    HWND native() const;
    void pumpMessages();
    bool shouldClose() const;
};

struct MouseState {               // 每帧累积的鼠标增量
    float deltaX, deltaY;
    float scroll;
    bool rightDown, leftDown;
};

class Input {                     // 从 Window 消息提取输入状态
    void onMessage(UINT msg, WPARAM, LPARAM);
    MouseState consumeFrame();    // 取出并清零
};

class Renderer {                  // 只剩帧循环 + Pass 编排
    Window window_;
    Input input_;
    VulkanContext ctx_;
    Scene scene_;
    std::vector<unique_ptr<RenderPass>> passes_;
};
```

### B. PipelineBuilder — 消除管线创建样板（估时：小）

`createGraphicsPipeline` 目前 ~80 行 `VkPipelineXxxCreateInfo{}` 样板。链式 API：

```cpp
GraphicsPipeline pipeline = PipelineBuilder()
    .vertexShader("forwardPass.vert.spv")
    .fragmentShader("forwardPass.frag.spv")
    .vertexInput<ForwardPass::Vertex>()
    .depthTest(true, VK_COMPARE_OP_LESS)
    .cullMode(VK_CULL_MODE_BACK_BIT)
    .colorAttachment(ctx.swapchainFormat())
    .descriptorSetLayout(layout)
    .build(ctx, renderPass);
```

消除 ~60 行/每 Pass，新 Pass 创建管线只需一个链式调用。

### C. RAII 资源包装（估时：中）

当前所有 VkBuffer/VkImage/VkPipeline 是裸句柄 + 手动 `vkDestroy*`。Move-only 包装类：

```cpp
class UniqueBuffer {
    VkBuffer handle_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
public:
    UniqueBuffer() = default;
    UniqueBuffer(VkDevice d, VkBuffer b) : handle_(b), device_(d) {}
    ~UniqueBuffer() { if (handle_) vkDestroyBuffer(device_, handle_, nullptr); }
    UniqueBuffer(UniqueBuffer&& o) noexcept : handle_(o.handle_), device_(o.device_) { o.handle_ = VK_NULL_HANDLE; }
    VkBuffer get() const { return handle_; }
};
```

收益：ForwardPass::shutdown() 从 ~40 行手动清理缩减为 `= default`。

### D. ResourceTable — 多 Pass 资源共享（估时：中）

Pass 之间通过命名资源表解耦。例如 GeometryPass 输出 GBuffer → LightingPass 读取：

```cpp
class ResourceTable {
    void setImageView("gbuf.position", view, format, extent);
    VkImageView imageView("gbuf.position") const;
    VkSampler   sampler("default") const;
    void clearSwapchainResources();  // resize 时清理
};

// RenderPass 新增两个可选方法
virtual void registerOutputs(ResourceTable& table) {}  // 发布
virtual void bindInputs(const ResourceTable& table) {}  // 订阅
```

---

## 多 Pass 渲染路线（特效）

### Shadow Mapping（估时：大）

ShadowMapPass → 定向光源视角渲染深度图 → LightingPass 采样 + PCF 柔化。

```
ShadowMapPass:
  每个投射阴影的光源 → 2D depth image (2048², D32_SFLOAT)
  正交投影渲染场景深度
  Sampler: VK_COMPARE_OP_LESS + depth compare

LightingPass:
  shadowFactor = texture(shadowMap, vec4(uv, layer, depth - bias))
```

### 延迟光照（估时：大）

GeometryPass (MRT 写 GBuffer) → LightingPass (全屏三角形, 读 GBuffer + ShadowMap)。

**GBuffer 格式**：

| Attachment | 格式 | 通道 |
|------------|------|------|
| Position | R16G16B16A16_SFLOAT | RGB=world pos |
| Normal | R16G16B16A16_SFLOAT | RGB=world normal |
| Albedo | R8G8B8A8_UNORM | RGB=albedo, A=ao |
| Material | R8G8_UNORM | R=roughness, G=metallic |

### MSAA（估时：小）

改动点集中：`VkImageCreateInfo.samples` + `VkPipelineMultisampleStateCreateInfo.rasterizationSamples` 从 1 改为 4。物理设备需查询 `framebufferColorSampleCounts` 支持。

### PBR + IBL（估时：中）

Cook-Torrance BRDF (D=GGX, F=Schlick, G=Smith) + IBL 三步预计算：
1. BRDF Integration LUT (512², R16G16_SFLOAT)
2. 环境立方体贴图预过滤 (mip chain, 每级对应粗糙度)
3. Irradiance map (漫反射积分)

### TAA（估时：中）

时序抗锯齿：每帧 Halton jitter 投影 → 保留 color + motion vector → reproject + blend (1/8 当前 + 7/8 历史)。

---

## 实施优先级（更新版）

| 优先级 | 步骤 | 估时 | 依赖 |
|--------|------|------|------|
| **1** | PipelineBuilder | 小 | 无 |
| **2** | RAII 资源包装 | 中 | 1 |
| **3** | Window / Input 分离 | 中 | 1 |
| **4** | ResourceTable | 中 | 1 |
| **5** | MSAA | 小 | 2 |
| **6** | GBuffer + 延迟光照 | 大 | 4 |
| **7** | Shadow Mapping | 大 | 4,6 |
| **8** | PBR + IBL | 中 | 6 |
| **9** | TAA / 后处理 | 中 | 6 |
| **10** | ImGui 调试 UI | 中 | 4 |

---

## 描述符管理策略

**原则**：全局 Set + Push Constants，不为每个物体分配独立 Set。

```glsl
// set=0, binding=0 — 全局 UBO
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view, projection;
    vec3 cameraPos;
    uint lightCount;
} global;

// set=0, binding=1 — 纹理
layout(set = 0, binding = 1) uniform sampler2D texSampler;

// push_constant — per-draw
layout(push_constant) uniform Push {
    mat4 model;
} pc;
```

| 数据 | 传递方式 | 更新频率 |
|------|----------|----------|
| view / projection | Descriptor Set 0 (UBO) | 每帧 |
| 纹理 | Descriptor Set 0 (Sampler) | 不变 |
| model 矩阵 | Push Constant | 每 DrawCall |

---

## 重构原则

1. **每步可运行** — 每次重构后项目必须能编译运行
2. **接口先行** — 先定义纯虚基类，再填充实现
3. **依赖单向** — Scene 不依赖 Renderer, Pass 只依赖 VulkanContext + Scene
4. **增量替换** — 新代码跑通后再删旧代码
5. **资源生命周期清晰** — 谁创建谁销毁，所有权不跨层转移
