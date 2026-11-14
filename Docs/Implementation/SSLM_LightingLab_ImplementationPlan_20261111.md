# SSLM Lighting Lab Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在UE5.8项目中建立不触碰正式地图的SSLM独立艺术光照实验室，支持六种光照候选、阴影/GI/画质对照和真实证据采集。

**Architecture:** 以JSON作为编辑器作者输入，新增UE Python作者入口和PIE审核入口；复用现有Village晨间光雾作者及PIE助手的地图哈希、几何保护、固定相机和地表绑定能力，PIE继承副本保存的天气快照，不重新应用天气预设。所有原生地图副本只由UE5.8 Editor SaveAs生成，云端不伪造二进制资产。

**Tech Stack:** Unreal Engine 5.8 Editor Python、Monolith MCP、Python 3标准库、JSON、GitHub。

**Spec:** `Docs/Implementation/SSLM_LightingLab_Design_20261111.md`

## Global Constraints

- 正式地图固定为`/DBAWorldPack_Village/Maps/L_Village_Start`，只读保护。
- UE目标版本固定为5.8；所有原生`.umap`/`.uasset`必须由UE编辑器生成。
- 实验关卡仅位于`/Game/Development/LightingLab/Maps/`。
- 不新增插件、运行时管理器、天气服务、Subsystem、服务器合同或正式Cook配置。
- 晨间候选仍受P5人工审核门禁约束；正式地图+1.0 EV仅为单地图历史校准值。
- 所有CVar对照仅限PIE运行时，必须读取回值；不写项目INI。
- 任何正式地图哈希变化、未保存包、几何变化或编辑器版本不符均立即停止。
- 所有新增代码与配置必须有中文注释/说明，并同步目录规划。

## Review Focus

- 正式地图哈希或未保存包状态变化：作者入口必须在任何写入前拒绝执行；由Task 2测试静态保护调用顺序。
- 目标开发地图已存在或上次作者中断：默认拒绝覆盖；由Task 2测试目标存在即停止。
- Unreal Python属性/组件在当前UE5.8构建不可用：必须输出真实缺失字段并停止，不得假设写入成功；由开发PC运行时回读验证。
- Lumen/硬件光追CVar不存在或不能切换：报告Unsupported或ReadbackMismatch，不得记为通过；由Task 3验证。
- 不同画质/镜头下截图不一致：固定FOV、分辨率、镜头和天气，并将真实环境条件写入证据；由Task 3验证。

---

### Task 1: 光照实验配置与离线合同测试

**Files:**
- Create: `Tools/Unreal/World/SSLM_LightingLab_V1.json`
- Create: `Tests/Assets/TestSSLM_LightingLab.py`

**Interfaces:**
- Produces: JSON配置中的`profiles`、`presets`、`shadow_sweep`、`quality`、`gi_modes`、`camera`字段；作者与PIE脚本只读取该合同。

- [ ] **Step 1: 编写离线合同测试**，覆盖六套profile、11个实验地图、五档Source Angle、High/Medium/Low、三种GI模式、正式地图路径与开发目录隔离。
- [ ] **Step 2: 执行测试并确认初始失败**，因为配置和作者脚本尚不存在。
- [ ] **Step 3: 创建JSON配置**。六套profile：SoftDaylight、MorningWarm、OvercastDiffuse、GoldenHour、Twilight、Moonlit；五档阴影角：0.5357、1、2、3、4度。每项均标记为待实机校准候选。
- [ ] **Step 4: 完成离线合同测试**。运行`python Tests/Assets/TestSSLM_LightingLab.py`，预期所有配置/路径/枚举/脚本结构检查通过。

### Task 2: UE5.8隔离关卡作者与保存保护

**Files:**
- Create: `Tools/Unreal/World/SSLM_LightingLabUE58.py`
- Reuse: `Tools/Unreal/World/ReviewVillageMorningEnvironmentUE.py`

**Interfaces:**
- `configure() -> dict`：读取JSON并设置既有作者模块的实验室路径、证据路径和PIE白名单。
- `preflight() -> dict`：验证UE5.8、正式地图身份、无PIE/未保存包、输出目录不存在，生成当前正式地图基线。
- `create_lab() -> dict`：对六套氛围profile和五档阴影角各创建一份正式地图副本，逐项应用候选并保存。
- `cold_audit() -> dict`：仅在独立Editor进程中验证保存回读、几何一致和正式地图哈希不变。
- `inspect_current() -> dict`：只读输出当前关卡和环境参数。

- [ ] **Step 1: 编写作者安全合同测试**，断言作者复用正式地图哈希、几何比较、SaveAs和保存回读辅助，且只允许Development/LightingLab路径。
- [ ] **Step 2: 实现配置装载和预检**，输出目录已有基线时拒绝覆盖；不自动删除已有关卡或证据；复用旧版Monolith Nanite槽位映射并按几何保护派生新地图键，缺失时在任何写入前停止。
- [ ] **Step 3: 实现基线相对参数应用**。太阳强度和天空光强度以正式地图实际回读值为基准乘候选倍率；Source Angle、色温、太阳高度角、雾与局部雾使用每个profile的显式候选参数。
- [ ] **Step 4: 保存前验证**。核验后处理曝光/色彩设置未被作者修改、非环境几何一致、正式地图SHA不变，再调用既有SaveAs/保存流程。
- [ ] **Step 5: 实现独立进程冷重载**。PID必须不同于作者基线PID；逐地图比对环境组件字段、几何和正式地图哈希。
- [ ] **Step 6: 执行离线测试与Python语法检查**。开发PC后续执行Monolith原生测试；云端阶段不得声称地图已创建。

### Task 3: PIE质量/GI对照入口

**Files:**
- Create: `Tools/Unreal/World/SSLM_LightingLabPIEReviewUE.py`
- Reuse: `Tools/Unreal/World/VillageMorningPIEReviewUE.py`

**Interfaces:**
- `setup(run, quality, view, gi_mode) -> dict`：调用既有PIE初始化/相机/地表绑定，再执行GI模式对照。
- `readback_cvars(names) -> dict`：读取GI、反射、硬件光追和体积雾相关CVar。
- GI模式：Baseline（只读现状）、LumenSoftware（尝试软件Lumen）、LumenHardware（仅当硬件光追前置条件满足时尝试）。
- 质量档：High、Medium、Low；视角：Entry、PeachWoodland、LakeShore、LakesideHut、LowWoodland。

- [ ] **Step 1: 编写PIE合同静态测试**，确保只接受实验地图白名单、只运行时设置CVar、不写INI。
- [ ] **Step 2: 实现PIE初始化复用**，复用既有PIE地图白名单、固定相机和地表绑定，并显式应用既有Clear天气定义作为中性天气/风场基线；PIE后读取灯光与雾效回值，确认天气投影没有破坏候选对照。
- [ ] **Step 3: 实现GI模式设置及读回**。硬件光追未启用时标记Unsupported；命令未生效时标记ReadbackMismatch；不将未支持能力当作失败的光照候选。
- [ ] **Step 4: 记录质量、GI模式、CVar前后值、地图、相机、天气、引擎版本和时间戳**。
- [ ] **Step 5: 执行离线合同测试**；真实PIE截图、GPU Profiler和性能采样在开发PC执行。

### Task 4: 文档、目录索引与交付说明

**Files:**
- Create: `Docs/Implementation/SSLM_LightingLab_Design_20261111.md`
- Create: `Docs/Implementation/SSLM_LightingLab_ImplementationPlan_20261111.md`
- Modify: `Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md`
- Modify: `Docs/README.md`
- Modify: `Docs/CHANGELOG.md`
- Modify: `Game/Config/DefaultGame.ini`
- Modify: `Build/Game/ServerPresentationExclusions.json`
- Create: `Game/Content/Development/LightingLab/README.md`

- [ ] **Step 1: 登记新增脚本、配置、测试、设计和计划的准确路径与职责**，并将开发地图目录加入DefaultGame NeverCook与三服务器AllServers排除合同。
- [ ] **Step 2: 更新Docs正式索引和变更记录**，明确云端源码完成与原生UE执行待家中PC之间的状态边界。
- [ ] **Step 3: 回读所有新增文件和索引引用**，检查链接、文件名、JSON解析、Python语法及离线测试结果。
- [ ] **Step 4: 在功能分支提交并创建Pull Request**，不直接合并main。

## Final Verification

- 运行：`python Tests/Assets/TestSSLM_LightingLab.py`
- 运行：`python -m py_compile Tools/Unreal/World/SSLM_LightingLabUE58.py Tools/Unreal/World/SSLM_LightingLabPIEReviewUE.py`
- 检查：`git diff --check`
- 离线测试同时核对`Game/Config/DefaultGame.ini` NeverCook、`Build/Game/ServerPresentationExclusions.json` AllServers规则和开发目录标记文件。
- 开发PC原生门禁：UE5.8编辑器/Monolith运行`preflight → create_lab → 独立Editor重启 → cold_audit → PIE setup/snapshot`；真实GPU和人工视觉结果单独记录。
