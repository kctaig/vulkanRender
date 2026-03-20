# Vulkan Renderer

基于 `vulkan_renderer_架构设计文档.md` 生成的 C++/CMake Vulkan 渲染器工程。

## 当前实现范围

- 阶段 1.5 已落地：`Win32` 窗口 + `Vulkan` 实例/设备/交换链（已移除 GLFW）
- 图形管线已接通：`RenderPass` / `Pipeline` / `Framebuffer` / `CommandBuffer`
- 已接入深度缓冲：`Depth Image` + `Depth Image View` + 深度测试
- 已切换真正 `MVP UBO`：`DescriptorSetLayout` + `UniformBuffer` + `DescriptorSet`
- 已支持基础 mesh 输入路径：启动参数可指定 `.obj` 文件
- 已升级基础几何输入：`position/normal/uv + index buffer`（为 GBuffer 阶段做准备）
- 已接入阶段 2 deferred：`Geometry -> Lighting -> ImGui` 三子通道
- 已实现 `Position/Normal/Albedo` GBuffer + deferred lighting 合成
- 已实现 `Final/Position/Normal/Albedo` 调试视图切换
- 已支持 deferred 多光源（可在 UI 调整数量与强度）
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

## 验证层开关

- `VK_LAYER_KHRONOS_validation` 已接入
- 仅 `Debug` 构建启用验证层（编译宏 `VR_ENABLE_VALIDATION`）
- `Release` 构建默认不启用验证层

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

## 光源设置说明（阶段 2）

当前为 **deferred 多点光源**，主要在 UI 的 `Stage 1.5 Controls` 面板中调参：

- `Deferred Light Count`：光源数量，范围 `1 ~ 32`
- `Deferred Light Intensity`：全局光源强度系数，范围 `0.1 ~ 8.0`
- `Deferred Debug`：调试输出模式（`Final / Position / Normal / Albedo`）

### 光源位置规则

shader 会按索引将光源分布在场景周围环带上（见 `shaders/deferred_lighting.frag`）：

- 角度：`angle = 2π * index / lightCount`
- 半径：`radius = 8.0 + (index % 4) * 2.0`
- 高度：`height = 2.0 + (index % 3) * 1.8`
- 位置：`(cos(angle) * radius, height, sin(angle) * radius)`

这会形成“多层半径 + 多层高度”的环形分布，便于观察 deferred 叠加效果。

### 光源颜色与衰减

- 颜色：根据索引生成 hue（黄金比例步进）得到不同色相，避免光源颜色过于接近
- 衰减：
	`attenuation = 1.0 / (1.0 + 0.09 * d + 0.032 * d^2)`，其中 `d = distance(lightPos, worldPos)`
- 最终单光贡献：`albedo * lightColor * max(dot(normal, lightDir), 0) * attenuation`

备注：当前实现为阶段 2 基线（多光源 Lambert + 衰减），后续阶段 3 会切换到 Cook-Torrance PBR 光照模型。

## 改造计划

- ✅ 阶段 1：基础 Vulkan 渲染链路
- ✅ 阶段 1.5：Win32 + ImGui 调试层 + 交互控制 + Docking + Output
- ✅ 阶段 2：真实 GBuffer attachments + deferred lighting + 多光源 + 调试视图
- ⏳ 阶段 3：Cook-Torrance 参数纹理与 IBL
- ⏳ 阶段 4：PCF/PCSS shadow map 与滤波
- ⏳ 阶段 5：Tone Mapping / TAA / Bloom
