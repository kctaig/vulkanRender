# Vulkan Renderer

基于 Pass 架构的最小化 Vulkan 渲染框架。

## 目录结构

```
├── include/                    # 头文件
│   ├── core/
│   │   ├── Application.h      # 程序入口，组装顶层组件
│   │   └── VulkanContext.h    # Device、Swapchain、内存、CommandPool
│   ├── renderer/
│   │   ├── RenderPass.h       # Pass 抽象接口
│   │   ├── Renderer.h         # 窗口 + 帧循环 + Pass 编排
│   │   └── ForwardPass.h      # 前向渲染 Pass（三角形 / 网格）
│   └── scene/
│       ├── MeshIO.h           # OBJ 模型加载
│       └── Scene.h            # 场景数据结构（Mesh, Light, Camera）
├── src/                        # 实现文件
│   ├── main.cpp
│   ├── core/
│   │   ├── Application.cpp
│   │   └── VulkanContext.cpp
│   ├── renderer/
│   │   ├── ForwardPass.cpp
│   │   └── Renderer.cpp
│   └── scene/
│       └── MeshIO.cpp
├── shaders/                    # GLSL 着色器
│   ├── triangle.vert
│   └── triangle.frag
├── assets/
│   └── models/                 # OBJ 模型文件
├── docs/
│   └── ARCHITECTURE_ROADMAP.md # 架构演进路线
└── CMakeLists.txt
```

## 架构

```
 ── Application ──         程序入口
        │
 ── Renderer ──            窗口、帧循环、Pass 编排、同步
        │
   ┌────┴────┐
   │         │
 VulkanContext   Pass[]     渲染 Pass 列表
 (设备/交换链/    │
  内存分配)    ForwardPass   (未来: GeometryPass, LightingPass, …)
```

### 分层职责

| 层 | 职责 | 依赖 |
|----|------|------|
| `core/VulkanContext` | Instance、Device、Swapchain、内存分配、CommandPool、Shader 编译 | 仅 Vulkan API |
| `renderer/RenderPass` | 抽象接口：`initialize()` `record()` `onSwapchainResize()` `shutdown()` | `VulkanContext` |
| `renderer/Renderer` | Win32 窗口、帧循环、图像获取/呈现、Pass 编排、鼠标输入 | `VulkanContext` `RenderPass` |
| `renderer/ForwardPass` | RenderPass、Pipeline、Descriptor、顶点/索引/Uniform 缓冲、网格加载 | `VulkanContext` |

依赖方向：`renderer/` → `core/`，`core/` 不依赖任何上层模块。

### 每帧数据流

```
Renderer::drawFrame()
  ├── ctx_.acquireNextImage()          → 获取可用的 Swapchain 图像
  ├── pass->record(cmd, frame, image)  → Pass 录制渲染命令
  ├── vkQueueSubmit()                  → 提交 GPU 执行
  └── vkQueuePresentKHR()              → 呈现到屏幕
```

## 构建

依赖：
- CMake 3.24+
- Vulkan SDK 1.4+
- MSVC 2022（或 Clang 18+）

```bash
# Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Visual Studio
cmake -B build_vs -G "Visual Studio 17 2022" -A x64
cmake --build build_vs --config Debug
```

## 使用

```bash
# 默认三角形
./build/Debug/vulkan_renderer.exe

# 加载 OBJ 模型
./build/Debug/vulkan_renderer.exe assets/models/bunny.obj
```

鼠标操作：

| 操作 | 效果 |
|------|------|
| 右键拖拽 | 旋转模型 |
| 左键拖拽 | 平移模型 |
| 滚轮 | 缩放 |

## 添加新 Pass

实现 `RenderPass` 接口，然后在 `Renderer::initialize()` 中注册即可，无需修改 Renderer 代码：

```cpp
class MyPass : public vr::RenderPass {
public:
    bool initialize(vr::VulkanContext& ctx) override {
        // 创建 RenderPass、Pipeline、Descriptor 等
        return true;
    }
    void record(VkCommandBuffer cmd, uint32_t frame, uint32_t image) override {
        // 录制渲染命令
    }
    void onSwapchainResize(vr::VulkanContext& ctx) override {
        // 重建依赖 Swapchain 的资源
    }
    void shutdown() override {
        // 销毁资源
    }
};

// 在 Renderer::initialize() 中:
auto myPass = std::make_unique<MyPass>();
myPass->initialize(ctx_);
passes_.push_back(std::move(myPass));
```
