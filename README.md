# Vulkan Renderer

基于 `vulkan_renderer_架构设计文档.md` 生成的 C++/CMake Vulkan 渲染器工程。

## 当前实现范围

- 阶段 1 已落地：`GLFW` 窗口 + `Vulkan` 实例/设备/交换链
- 图形管线已接通：`RenderPass` / `Pipeline` / `Framebuffer` / `CommandBuffer`
- 已接入深度缓冲：`Depth Image` + `Depth Image View` + 深度测试
- 已切换真正 `MVP UBO`：`DescriptorSetLayout` + `UniformBuffer` + `DescriptorSet`
- 已支持基础 mesh 输入路径：启动参数可指定 `.obj` 文件
- 已升级基础几何输入：`position/normal/uv + index buffer`（为 GBuffer 阶段做准备）
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

## Mesh 输入路径（PowerShell）

默认会加载 `assets/models/basic_mesh.obj`。

也可以在启动时指定自定义 `.obj`：

```powershell
Set-Location "d:\code\Vulkan\vulkanRender"
.\build\Debug\vulkan_renderer.exe "d:\code\Vulkan\vulkanRender\assets\models\basic_mesh.obj"
```

## 下一步建议（按文档路线）
1. 阶段 1.5：接入 ImGui 调试层（性能面板 + 参数调节）
2. 阶段 2：落地真实 GBuffer attachments + deferred lighting
3. 阶段 3：引入 Cook-Torrance 参数纹理与 IBL
4. 阶段 4：实现 PCF/PCSS shadow map 与滤波
5. 阶段 5：加入 Tone Mapping / TAA / Bloom
6. 阶段 6：Render Graph 自动 barrier 与资源生命周期管理
7. 阶段 7：GPU Culling（Frustum/Occlusion/LOD）与 Light Culling
