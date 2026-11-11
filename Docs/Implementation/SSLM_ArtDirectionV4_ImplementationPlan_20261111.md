# SSLM Art Direction V4.0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将SSLM V3.0色彩与柔和光照基线升级为全域V4.0艺术指导和视觉质量体系，并把八大艺术体系映射到Lighting Lab后续可执行验收任务。

**Architecture:** V4.0成为唯一全局美术权威文档，V3.0保留为历史色彩基线；AGENTS.md、Docs/README.md、目录规划和CHANGELOG只引用V4.0。Lighting Lab V1现有光照测试保持不变，L2—L8作为明确的后续扩展任务，不伪造UE原生资产或验证结果。

**Tech Stack:** Markdown、JSON、GitHub、Unreal Engine 5.8 Editor/Monolith（后续原生执行）、Python离线校验。

**Spec:** Docs/Implementation/SSLM_ArtDirectionV4_Design_20261111.md

## Global Constraints

- 全局唯一艺术规范为Docs/Art/SSLM全局美术艺术指导与视觉质量规范_V4.0.md。
- V3.0原文保留并标记为历史版本；不删除V3色彩基线和SSLMColorRegistry_V3.json。
- 用户参考图只作为Village湖边群山与峭壁专属基准，不扩展为其他地图地貌模板。
- 所有场景柔和光照，但必须保留方向、体积、接触阴影、空间层次和战斗可读性。
- 不新增未经校准的正式HEX、UE材质数值、GPU预算、帧率承诺或平台能力承诺。
- Nanite Foliage、GPU PCG、Lumen Hardware、Volumetric Fog等能力必须按UE5.8目标设备实测；Beta/Experimental不得默认成为正式路径。
- 不修改正式地图、材质、植被、水体、灯光、后处理或运行时天气资产。
- 不创建或伪造.umap/.uasset；原生资产只能由UE5.8 Editor/Monolith生成。
- Lighting Lab现有V1仅代表云端代码/配置已合并；真实原生测试仍待家中PC执行。

## Review Focus

- V3.0与V4.0双权威冲突：检查V3.0头部明确历史状态，AGENTS和README只将V4.0列为当前规范。
- 色彩索引名称含V3：确认它仍是V3色值派生索引，不错误声明为V4.0完整艺术规范。
- UE5.8 Beta/Experimental功能：确认文档清楚标注Nanite Foliage和GPU PCG的实验边界。
- Lighting Lab范围夸大：确认L1现有、L2—L8待实施的状态差异明确。
- 玩法可读性和性能否决条件遗漏：确认八大艺术体系、人工签收、真实设备性能和否决条件均可追溯。

---

### Task 1: V4.0主规范与设计规格

**Files:**
- Create: Docs/Art/SSLM全局美术艺术指导与视觉质量规范_V4.0.md
- Create: Docs/Implementation/SSLM_ArtDirectionV4_Design_20261111.md
- Modify: Docs/Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md

- [ ] **Step 1:** 复制并继承V3.0全部已确认色板、色彩空间、区域生态、战斗色语义和光照要求。
- [ ] **Step 2:** 增加八大艺术体系、五项高级技术、区域艺术指导卡、生产流程、验收等级和否决条件。
- [ ] **Step 3:** 增加UE5.8官方技术链接及功能状态说明，区分稳定能力与Beta/Experimental能力。
- [ ] **Step 4:** 将V3.0头部标记为历史前序版本，保留原内容和派生色彩索引。
- [ ] **Step 5:** 回读检查所有V3色板条目仍在V4.0内，且无新增未审核正式色值。

### Task 2: Lighting Lab映射与实施计划

**Files:**
- Create: Docs/Implementation/SSLM_ArtDirectionV4_ImplementationPlan_20261111.md
- Modify: Docs/Implementation/SSLM_LightingLab_Design_20261111.md

- [ ] **Step 1:** 明确现有Lighting Lab V1只覆盖L1光照/曝光/GI/阴影基线。
- [ ] **Step 2:** 登记L2材质样板、L3地形峭壁、L4植被群落、L5水体岸线、L6角色战斗镜头、L7天气时间、L8画质平台回归。
- [ ] **Step 3:** 为每个任务定义输入资产、固定镜头、检查项、证据格式、目标设备和人工审核角色。
- [ ] **Step 4:** 不修改现有Lighting Lab V1运行脚本/参数，待家中PC完成首次原生执行后再决定扩展接口。

### Task 3: 全局入口与历史版本治理

**Files:**
- Modify: AGENTS.md
- Modify: Docs/README.md
- Modify: Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md
- Modify: Docs/CHANGELOG.md

- [ ] **Step 1:** 将AGENTS版本更新为1.5.0，强制新增/修改美术内容引用V4.0。
- [ ] **Step 2:** Docs README将V4.0列为现行唯一权威规范，V3.0列为历史基线。
- [ ] **Step 3:** 更新目录规划，登记V4.0、设计规格、实施计划和Lighting Lab映射。
- [ ] **Step 4:** CHANGELOG记录V4.0规则升级和未执行的原生资产事项。

### Task 4: 离线一致性与引用校验

**Files:**
- Test: 通过云端回读和结构检查完成；不要求UE模块。

- [ ] **Step 1:** 检查V4.0主文档包含八大艺术体系、五项技术、Lighting Lab L1—L8和验收否决条件。
- [ ] **Step 2:** 检查AGENTS和README当前规范只指向V4.0。
- [ ] **Step 3:** 检查V3.0历史标记和V3派生色板引用无冲突。
- [ ] **Step 4:** 检查所有官方Epic链接、文档路径、版本号和日期。
- [ ] **Step 5:** 在功能分支创建Draft PR，不直接覆盖main；等待审核与后续合并指令。

## Final Verification

- JSON矩阵/配置若有修改，必须成功解析。
- GitHub回读所有新旧文件，检查路径、链接和标题。
- 比较main与功能分支，确认没有意外改动Lighting Lab脚本、正式地图或材质资产。
- 本次文档完成不得声明UE场景、材质、植被、水体、性能或人工视觉验收通过。
