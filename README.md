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

## 改造计划（覆盖原阶段计划）

目标：在不打断当前可运行版本的前提下，逐步将渲染流程迁移到 **RenderGraph 驱动**，并在迁移过程中同步推进 PBR、阴影与后处理。

### 里程碑 A：RenderGraph 最小骨架（当前优先级最高）

- [ ] 定义 `PassNode / ResourceNode / reads / writes` 抽象
- [ ] 支持按依赖生成执行顺序（先不做复杂优化）
- [ ] 保留现有 `Renderer.cpp` 渲染结果不变（功能等价迁移）

### 里程碑 B：阶段 2 现有流程图驱动化

- [ ] 将 `Geometry -> Lighting -> ImGui` 三 pass 接入 RenderGraph
- [ ] 由图统一管理 GBuffer/Swapchain 资源读写关系
- [ ] 先维持现有 subpass 方案，确保性能与正确性稳定

### 里程碑 C：PBR（阶段 3）并入图

- [ ] 增加 `Metallic / Roughness / AO` 材质通道（先常量，再纹理）
- [ ] Lighting Pass 从 Lambert 升级为 Cook-Torrance
- [ ] 在 RenderGraph 中声明材质资源依赖

### 里程碑 D：阴影（阶段 4）并入图

- [ ] 新增 `Shadow Pass`（深度图生成）
- [ ] Lighting Pass 读取 shadow map（先 PCF，再 PCSS）
- [ ] 由 RenderGraph 管理 shadow 资源生命周期与依赖

### 里程碑 E：后处理（阶段 5）并入图

- [ ] `HDR -> Tone Mapping -> TAA -> Bloom` 作为独立 pass 链
- [ ] 支持中间纹理复用与可视化调试输出
- [ ] 保持 ImGui 调试面板对关键参数可调

### 里程碑 F：RenderGraph 增强能力（原阶段 6 前置实施）

- [ ] 自动 barrier 插入（从手写同步迁移）
- [ ] 资源别名与生命周期压缩
- [ ] Pass 编译缓存与重建策略（窗口 resize / 配置切换）

### 里程碑 G：优化（阶段 7）

- [ ] GPU Culling（Frustum/Occlusion/LOD）
- [ ] Tiled/Clustered Light Culling
- [ ] 分析并优化 CPU 提交与 GPU 带宽瓶颈

### 当前状态

- [x] 基础渲染链路、Win32 + ImGui、Deferred 多光源、调试视图已可用
- [x] Debug 验证层启用、Release 关闭已接入
- [ ] 正式进入里程碑 A（RenderGraph 最小骨架）
