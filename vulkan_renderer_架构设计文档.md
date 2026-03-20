# Vulkan Renderer 架构设计文档

## 1. 项目目标

构建一个基于C++的 Vulkan 的现代实时渲染器，要使用CMAKE进行项目管理，支持：

- 光栅化管线（MVP 变换）
- 延迟渲染（Deferred Rendering）
- PBR（Cook-Torrance）
- 阴影（PCF / PCSS）
- 抗锯齿（MSAA / TAA）
- 高级材质（法线贴图 / 视差贴图）

## 2. 总体架构

### 2.1 Frame Pipeline

每一帧执行流程：

```
Frame Begin
 ├── Scene Update（CPU）
 ├── Culling（CPU / GPU）
 ├── Render Graph Build
 ├── Geometry Pass（GBuffer）
 ├── Shadow Pass
 ├── Lighting Pass
 ├── Post Processing
 └── Present
```

## 3. 数据结构设计

### 3.1 CPU 侧 Scene

```
Scene
 ├── Mesh
 ├── Material
 ├── Light
 ├── Camera
```

### 3.2 GPU 数据

使用 SSBO / UBO：

```
ObjectBuffer
MaterialBuffer
LightBuffer
CameraBuffer
```

## 4. 渲染流程设计

### 4.1 Geometry Pass（GBuffer）

输出多个 Render Target：

```
GBuffer
 ├── Position（或 depth reconstruct）
 ├── Normal
 ├── Albedo
 ├── Metallic
 ├── Roughness
 ├── AO
```

功能：

- 模型变换（MVP）
- 法线贴图
- 视差贴图
- 材质参数写入

### 4.2 Shadow Pass

流程：

```
Render from light view → Shadow Map
```

PCSS 实现步骤：

1. Blocker Search
2. Penumbra Estimation
3. Filtering

### 4.3 Lighting Pass

基于 GBuffer 进行光照计算：

```
for each pixel:
    reconstruct position
    for each light:
        compute BRDF
        apply shadow
    add IBL
```

### 4.4 Post Processing

推荐顺序：

```
HDR → Tone Mapping → TAA → Bloom → Output
```

## 5. 核心模块设计

### 5.1 Renderer 模块

```
Renderer
 ├── Device
 ├── Swapchain
 ├── ResourceManager
 ├── PipelineManager
 ├── RenderGraph
 ├── Pass System
```

### 5.2 Pass 系统

```
Pass
 ├── GBufferPass
 ├── ShadowPass
 ├── LightingPass
 ├── PostProcessPass
```

每个 Pass 描述：

- 输入资源
- 输出资源
- Shader
- Pipeline

### 5.3 Resource 管理

负责：

- Buffer 分配
- Texture 生命周期
- Descriptor Set 管理

### 5.4 Pipeline 管理

封装：

- Graphics Pipeline
- Compute Pipeline
- Shader Module

## 6. Render Graph 设计（关键）

### 6.1 抽象接口

```
AddPass(name)
    .reads(resource)
    .writes(resource)
```

示例：

```
AddPass("GBuffer")
    writes(gbuffer)

AddPass("Lighting")
    reads(gbuffer)
    writes(hdr)
```

### 6.2 功能

- 自动资源依赖分析
- 自动插入 barrier
- 管理资源生命周期

## 7. GPU 并行优化

### 7.1 Culling

- Frustum Culling
- Occlusion Culling（Hi-Z）
- LOD 选择

实现方式：

- Compute Shader

### 7.2 Light Culling

- Tiled Lighting
- Clustered Lighting

## 8. Shader 结构

```
shader/
 ├── pbr.glsl
 ├── lighting.glsl
 ├── shadow.glsl
 ├── common.glsl
```

建议：

- 模块化
- 共享函数库

## 9. 扩展功能

- IBL（环境光）
- SSAO / GTAO
- SSR
- GPU Instancing

## 10. 实现路线（推荐顺序）

### 阶段 1：基础

- Vulkan 初始化
- MVP 渲染
- 深度测试

### 阶段 2：延迟渲染

- GBuffer
- 多光源

### 阶段 3：PBR

- Cook-Torrance

### 阶段 4：阴影

- PCF → PCSS

### 阶段 5：环境光

- IBL

### 阶段 6：后处理

- TAA
- Tone Mapping

### 阶段 7：优化

- Render Graph
- GPU Culling

## 11. 总结

该渲染器架构的核心在于：

1. 数据驱动（Scene → GPU）
2. 分阶段渲染（Deferred Pipeline）
3. 自动化管理（Render Graph）

通过逐步实现，可以从基础渲染器扩展到接近工业级实时渲染系统。

