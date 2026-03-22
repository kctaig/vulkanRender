# Vulkan Renderer

基于 `vulkan_renderer_架构设计文档.md` 生成的 C++/CMake Vulkan 渲染器工程。

## 当前实现范围

- 阶段 1.5 已落地：`Win32` 窗口 + `Vulkan` 实例/设备/交换链（已移除 GLFW）
- 图形管线已接通：`RenderPass` / `Pipeline` / `Framebuffer` / `CommandBuffer`
- 已升级基础几何输入：`position/normal/uv + index buffer`（为 GBuffer 阶段做准备）
- 已接入阶段 2 deferred：`Geometry -> Lighting -> ImGui` 三子通道
- 已实现 `Position/Normal/Albedo` GBuffer + deferred lighting 合成
- 已实现 `Final/Position/Normal/Albedo` 调试视图切换
- 已支持 deferred 多光源（可在 UI 调整数量与强度）
- 已新增可停靠 `Output` 输出窗口（日志面板）
- shader 自动编译：`triangle.vert` / `triangle.frag` -> `SPIR-V`

## 目录结构

├─ CMakeLists.txt
├─ src/
 备注：当前已进入阶段 3，lighting 已切换为 Cook-Torrance 基线（常量 `Metallic/Roughness/AO` + UI 可调）。
│  ├─ renderer/
│  │  └─ passes/
│  ├─ scene/

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
- ✅ 阶段 3：Cook-Torrance 参数纹理与 IBL（已完成）
- ⏳ 阶段 4：PCF/PCSS shadow map 与滤波
- ⏳ 阶段 5：Tone Mapping / TAA / Bloom

## Frame Graph 设计思路（适配当前项目，可扩展）

目标：将当前 `deferred + Scene 离屏 + ImGui` 的渲染流程，升级为声明式、可编译、可裁剪的帧图系统，降低手写同步与资源管理复杂度。

### 1) 核心抽象

- `FrameGraph`：每帧构建 DAG（只声明 pass 与资源关系，不直接持有业务逻辑）
- `PassNode`：`setup(builder)` 声明读写依赖；`execute(context)` 录制 Vulkan 命令
- `ResourceHandle`：逻辑资源句柄（如 `SceneColor`、`GBufferNormal`），与真实 `VkImage/VkBuffer` 解耦
- `Blackboard`：跨 pass 数据交换（相机、光源参数、调试模式、Scene 视口信息）

### 2) 编译阶段（FrameGraph::compile）

- 依赖分析与拓扑排序：根据资源读写自动排序 pass，检测循环依赖
- 生命周期计算：推导资源生存区间，为 transient aliasing 准备
- 自动同步计划：生成 image/buffer barrier（layout/stage/access），减少散落的手写 barrier
- Pass Culling：剔除无消费者的 pass（例如关闭某调试视图时）

### 3) 执行阶段（FrameGraph::execute）

- 按编译后的执行序列调用每个 pass 的 `execute`
- 在 pass 边界应用编译得到的 barrier plan
- 输出 GPU profiling 点（可选），用于后续性能归因

### 4) 与当前代码映射

- `GBufferPass`：写 `GBufferPosition/Normal/Albedo + Depth`
- `DeferredLightingPass`：读 GBuffer，写 `SceneColor`（离屏）
- `ImGuiPass`：采样 `SceneColor` 并写 swapchain
- 后续扩展 pass（Shadow/SSAO/Bloom/TAA）只需声明资源关系，无需改主循环骨架

### 5) 资源策略建议

- 常驻资源：swapchain、持久化 UBO/SSBO、长期缓存纹理
- 瞬态资源：GBuffer、中间后处理纹理（启用 aliasing）
- 外部导入资源：swapchain image / scene mesh buffers / UI font texture

### 6) 迁移路径（低风险）

1. 先把现有三个主 pass（GBuffer/Lighting/ImGui）迁入 FrameGraph 的声明式接口
2. 接入自动 barrier 与 pass culling
3. 接入 transient aliasing 与可视化导图（GraphViz `.dot`）
4. 再推进多队列与异步 compute（阶段 4/5）

## 阶段三收尾（已完成）

- [x] 完成 Cook-Torrance 基线（常量 `Metallic/Roughness/AO` + UI）
- [x] 完成 Scene 组件离屏渲染显示（`SceneColor` -> `ImGui::Image`）
- [x] 接入材质纹理参数工作流（当前为 procedural map 占位，含 BaseColor/Metallic/Roughness/AO）
- [x] 引入 IBL 最小闭环（Irradiance + Prefilter + BRDF LUT 的最小可运行近似实现）
- [x] 统一 PBR 参数来源（常量回退 + map 优先，`Use Procedural Material Maps` 可切换）
- [x] 输出阶段三验收项（UI 平滑帧时/FPS + Output 周期性能基线日志）
