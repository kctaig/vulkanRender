# Vulkan Renderer

基于 `vulkan_renderer_架构设计文档.md` 生成的 C++/CMake Vulkan 渲染器工程。

## 当前实现范围

- 阶段 1.5 已落地：`Win32` 窗口 + `Vulkan` 实例/设备/交换链（已移除 GLFW）
- 图形管线已接通：`RenderPass` / `Pipeline` / `Framebuffer` / `CommandBuffer`
- 已接入深度缓冲：`Depth Image` + `Depth Image View` + 深度测试
- 已切换真正 `MVP UBO`：`DescriptorSetLayout` + `UniformBuffer` + `DescriptorSet`
- 已支持基础 mesh 输入路径：启动参数可指定 `.obj` 文件
- 已升级基础几何输入：`position/normal/uv + index buffer`（为 GBuffer 阶段做准备）
- 交换链重建与窗口 resize 处理
- 已恢复并接入 `ImGui`（`imgui_impl_win32` + `imgui_impl_vulkan`）
- 已恢复交互控制：右键旋转、左键平移、滚轮缩放
- 已支持 `Docking` 停靠布局与主 DockSpace
- 已新增可停靠 `Output` 输出窗口（日志面板）
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

## 渲染实现规范（重要）

- 当前唯一渲染实现文件：`src/renderer/Renderer.cpp`
- 已移除历史版本文件：`RendererV2.cpp`、`RendererV3.cpp`
- 后续迭代统一在 `Renderer.cpp` 上进行
- 不再新增 `RendererV4.cpp` / `Renderer_xxx.cpp` 这类版本化文件

## 依赖

- Vulkan SDK
- Dear ImGui（docking 分支，Win32/Vulkan backends）
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

## 进度

- ✅ 阶段 1：基础 Vulkan 渲染链路
- ✅ 阶段 1.5：Win32 + ImGui 调试层 + 交互控制 + Docking + Output
- ⏳ 阶段 2：真实 GBuffer attachments + deferred lighting
- ⏳ 阶段 3：Cook-Torrance 参数纹理与 IBL
- ⏳ 阶段 4：PCF/PCSS shadow map 与滤波
- ⏳ 阶段 5：Tone Mapping / TAA / Bloom
- ⏳ 阶段 6：Render Graph 自动 barrier 与资源生命周期管理
- ⏳ 阶段 7：GPU Culling（Frustum/Occlusion/LOD）与 Light Culling
