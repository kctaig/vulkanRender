# Vulkan Renderer

基于 `vulkan_renderer_架构设计文档.md` 生成的 C++/CMake Vulkan 渲染器工程。

## 当前实现范围

- 阶段 1 已落地：`GLFW` 窗口 + `Vulkan` 实例/设备/交换链
- 图形管线已接通：`RenderPass` / `Pipeline` / `Framebuffer` / `CommandBuffer`
- 可渲染动态旋转 `MVP` 彩色三角形（push constant 传递矩阵）
- 交换链重建与窗口 resize 处理
- shader 自动编译：`triangle.vert` / `triangle.frag` -> `SPIR-V`

## 目录结构

```text
.
├─ CMakeLists.txt
├─ cmake/
├─ src/
│  ├─ core/
│  ├─ renderer/
│  │  └─ passes/
│  ├─ scene/
│  └─ common/
├─ shaders/
└─ assets/
```

## 依赖

- Vulkan SDK
- GLFW3（CMake target: `glfw`）
- GLM（CMake target: `glm::glm`）
- CMake >= 3.24
- C++20 编译器

## Windows 构建（PowerShell）

```powershell
Set-Location "d:\code\Vulkan\vulkanRender"
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

## Windows 运行（PowerShell）

```powershell
Set-Location "d:\code\Vulkan\vulkanRender"
.\build\Debug\vulkan_renderer.exe
```

## 下一步建议（按文档路线）

1. 阶段 2：落地真实 GBuffer attachments + deferred lighting
2. 阶段 3：引入 Cook-Torrance 参数纹理与 IBL
3. 阶段 4：实现 PCF/PCSS shadow map 与滤波
4. 阶段 5：加入 Tone Mapping / TAA / Bloom
5. 阶段 6：Render Graph 自动 barrier 与资源生命周期管理
6. 阶段 7：GPU Culling（Frustum/Occlusion/LOD）与 Light Culling
