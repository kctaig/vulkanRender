# Vulkan Renderer

基于 Pass 架构的 Vulkan 渲染框架。支持 OBJ/FBX/glTF 模型导入、纹理加载、轨道摄像机，代码强调 C++17 现代化和最小样板。

## 目录结构

```
├── include/                       # 公共头文件
│   ├── core/
│   │   ├── Application.h          # 程序入口
│   │   ├── VulkanContext.h        # 设备/交换链/内存工厂 + 上传/执行工具
│   │   ├── VulkanResource.h       # RAII 包装（UniqueBuffer/Image/Pipeline 等）
│   │   └── Window.h               # Win32 窗口封装
│   ├── renderer/
│   │   ├── RenderPass.h           # Pass 基类（含 7 个共享 helper）
│   │   ├── Renderer.h             # 窗口 + 帧循环 + 同步 + Pass 编排
│   │   ├── ForwardPass.h          # 前向渲染 Pass
│   │   ├── PipelineBuilder.h      # 链式管线创建
│   │   └── DescriptorWriter.h     # 链式描述符分配+写入
│   └── scene/
│       ├── Camera.h               # 轨道摄像机
│       ├── Scene.h                # 数据容器（Camera + AssetManager + 实例列表）
│       └── AssetManager.h         # assimp 模型导入 + stb 纹理上传
├── src/                            # 实现文件
│   ├── main.cpp
│   ├── core/
│   │   ├── Application.cpp
│   │   ├── VulkanContext.cpp
│   │   └── Window.cpp
│   ├── renderer/
│   │   ├── ForwardPass.cpp
│   │   ├── PipelineBuilder.cpp
│   │   ├── Renderer.cpp
│   │   └── RenderPass.cpp
│   └── scene/
│       └── AssetManager.cpp
├── 3dparty/                        # 第三方库（源码级集成）
│   ├── glm/                        #   GLM 数学库
│   ├── stb_image/stb_image.h       #   纹理解码（PNG/JPG/BMP/TGA）
│   └── assimp/                     #   模型导入库（源码编译）
├── shaders/                        # GLSL 着色器（自动编译为 SPIR-V）
│   ├── forwardPass.vert
│   └── forwardPass.frag
├── resource/                       # 运行时资源
│   └── models/                     #   OBJ/FBX 模型文件 + 纹理
└── docs/                           # 文档
    ├── ARCHITECTURE_ROADMAP.md
    ├── CODE_ANALYSIS.md
    └── VULKAN_API_REFERENCE.md
```

## 架构

```
Application  →  Renderer  →  Pass[]
                  │              ├── ForwardPass
                  ├── Window     └── (GeometryPass, LightingPass, …)
                  ├── VulkanContext
                  └── Scene
                        ├── Camera
                        ├── AssetManager (assimp + stb)
                        └── Instance[]
```

| 层 | 职责 | 依赖 |
|----|------|------|
| `core/VulkanContext` | Instance/Device/Swapchain、内存分配、staging 上传、一次性 cmd 执行 | Vulkan API |
| `core/Window` | Win32 窗口创建 + 消息循环 | Win32 |
| `renderer/RenderPass` | Pass 基类 — ctx_/scene_/renderPass_/framebuffers + 7 个共享 helper | VulkanContext |
| `renderer/Renderer` | 帧循环、同步、Pass 生命周期、鼠标→Camera 转发 | VulkanContext, Window, Scene |
| `scene/Scene` | 数据中心 — Camera + AssetManager + 实例列表 | AssetManager |
| `renderer/ForwardPass` | 前向渲染 — 从 Scene 读取实例逐个 DrawIndexed | RenderPass, Scene |

## RenderPass 基类提供的共享方法

所有 Pass 继承后直接调用，无需重写：

| 方法 | 作用 |
|------|------|
| `createRenderPass(ctx, attachments, subpasses, deps)` | 创建 VkRenderPass |
| `createDepth(ctx, outImage)` | 深度缓冲 |
| `createFramebuffers(ctx, depthView)` | 每 swapchain image 一个 Framebuffer |
| `createDefaultTexture(ctx, outImage, outSampler)` | 256×256 棋盘格纹理 |
| `createUniformBuffers<UBO>(ctx, out)` | UBO 双缓冲 |
| `createDescriptorPool(ctx, sizes, outPool)` | 描述符池 |
| `allocateDescriptorSets(layout, pool, outSets)` | 分配描述符集 |

## 快速开始

### 构建

```bash
# 安装 assimp（到 3dparty/ 目录，或通过 vcpkg）
# 下载 stb_image.h 到 3dparty/stb_image/

cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

### 运行

```bash
# 加载模型
./build/Debug/vulkan_renderer.exe resource/models/bunny.obj
./build/Debug/vulkan_renderer.exe resource/models/OldMan/OldMan.fbx

# 不传参数 = 空场景（清屏色，无模型）
./build/Debug/vulkan_renderer.exe
```

### 鼠标操作

| 操作 | 效果 |
|------|------|
| 右键拖拽 | 旋转 |
| 左键拖拽 | 平移 |
| 滚轮 | 缩放 |
| W/S | 前进/后退 |
| A/D | 左移/右移 |

## 添加新 Pass

继承 `RenderPass`，实现四个虚函数，构造函数中调基类 helper：

```cpp
class MyPass : public vr::RenderPass {
public:
    bool initialize(vr::VulkanContext& ctx) override {
        ctx_ = &ctx;
        // 用基类 helper 搭基础设施
        createRenderPass(ctx, myAttachments, mySubpasses, myDeps);
        createDepth(ctx, depthImage_);
        createFramebuffers(ctx, depthImage_.view());
        createDefaultTexture(ctx, textureImage_, textureSampler_);
        createUniformBuffers<MyUBO>(ctx, uniformBuffers_);
        createDescriptorPool(ctx, poolSizes, descriptorPool_);
        // 自己写 pipeline + descriptor layout + descriptor writes
        // …
        return true;
    }
    void record(VkCommandBuffer cmd, uint32_t frame, uint32_t image) override { /* … */ }
    void onSwapchainResize(vr::VulkanContext& ctx) override { /* … */ }
    void shutdown() override { /* … */ }
};
```

在 `Renderer::initialize()` 中注册：

```cpp
auto pass = std::make_unique<MyPass>();
pass->initialize(ctx_);
pass->setScene(scene_);
passes_.push_back(std::move(pass));
```
