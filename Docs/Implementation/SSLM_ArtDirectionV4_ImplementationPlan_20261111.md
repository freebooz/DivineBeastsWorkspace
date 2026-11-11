# SSLM Art Direction V4.0 Implementation Plan

执行顺序：L1 → L2 → L3 → L4 → L5 → L6 → L7 → L8。完整任务合同见 Docs/Implementation/SSLM_LightingLab_L1-L8_ExecutionRoadmap_20261111.md。

**目标：** 将SSLM V3.0色彩与柔和光照基线升级为全域V4.0艺术指导和视觉质量体系，并按L1—L8顺序完成原生场景样板、性能验证和人工验收。

**架构：** V4.0为全局美术唯一权威规范；V3.0保留为历史色彩基线。Lighting Lab为隔离验证载体，所有原生资产由UE5.8 Editor/Monolith创建。L1先建立光照测量基线，L2—L8依赖前置已批准结果逐步扩展。

## 全局约束
- 现行唯一艺术规范：Docs/Art/SSLM全局美术艺术指导与视觉质量规范_V4.0.md。
- L1—L8具体输入、输出、门禁和里程碑见Execution Roadmap。
- 不修改正式地图、材质、植被、水体、灯光、后处理或运行时天气资产，除非阶段证据通过并另行批准集成。
- 不创建或伪造.umap/.uasset；原生资产只能由UE5.8 Editor/Monolith生成。
- Nanite Foliage、GPU PCG、Lumen Hardware、Volumetric Fog等按目标设备实测，Beta/Experimental不得默认成为正式路径。
- Lighting Lab目录继续NeverCook并排除所有专用服务器。
- 所有候选参数与资产在完成技术验证和人工签收前保持Candidate状态。

## Task 1：云端路线图与审核矩阵
- [ ] 为L1—L8明确依赖、输入资产、制作/验证内容、输出证据和放行门禁。
- [ ] 在ReviewMatrix中登记每个阶段状态、输入、交付物和验收条件。
- [ ] 明确L1现有V1云端代码与UE原生验证待执行的区别。
- [ ] 检查路线图与V4.0主规范的八大艺术体系、L1—L8一致。

## Task 2：L1—L8原生实施准备
- [ ] L2详细合同见Docs/Implementation/SSLM_L2_PainterlyPBR_MaterialLookDev_20261111.md；优先复用现有Surface材质资产、函数和作者工具，不建立平行材质插件/母版。
- [ ] L1未完成原生验证和人工批准前，L2只进行纹理来源/通道审计和候选LookDev准备，不批准正式材质。
- [ ] L3详细合同见Docs/Implementation/SSLM_L3_TerrainCliffLandscape_20261111.md；复用既有Gaea/十层PBR/Nanite/RVT流程，不修改正式Village地图或共享材质。
- [ ] 家中PC首先完成L1：preflight、create_lab、冷重载、PIE、固定机位、Profiler和人工审核。
- [ ] L2—L8可并行整理候选资产清单和设计卡，但不得把未通过的L1候选写成正式基线。
- [ ] 每阶段原生创建、保存、冷重载和证据回读通过后，更新ReviewMatrix状态。
- [ ] 每阶段通过后再批准下一阶段的集成依赖，不允许跨阶段跳过门禁。

## Task 3：版本治理与发布
- [ ] 检查V4.0是唯一全局美术入口，V3.0为历史色彩基线。
- [ ] 检查Lighting Lab路线图、审核矩阵、README与目录规划互相引用一致。
- [ ] 检查云端文件变更没有伪造UE原生资产或声称未完成的性能/视觉验收。
- [ ] PR经用户审阅后再合并main；不擅自合并。

## 最终验证
- JSON必须成功解析，阶段数量为8，顺序为L1—L8。
- 每个阶段必须有输入、输出证据、验收门禁和当前状态。
- GitHub回读路线图、矩阵、V4.0主规范、AGENTS和README。
- 确认正式地图/材质/植被/水体/灯光资产没有被本轮修改。
- 云端计划完成不得宣称UE5.8原生执行、GPU性能或人工视觉验收通过。