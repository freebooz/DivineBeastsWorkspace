# SSLM UE5.8 统一艺术光照基准场景设计规格

版本：V1.0｜日期：2026-11-11｜状态：用户已选择方案A并授权云端代码实施

## 1. 目标与背景

建设独立的 **SSLM Lighting Lab（SSLM统一艺术光照实验室）**，用于在不改变正式地图的前提下，对UE5.8场景柔和艺术光照进行可重复、可比较、可追溯的实验。实验室以现有新手村环境资产为第一套代表性测试场景，覆盖山体/峭壁、湖泊、森林植被、竹林、古建筑及玩家角色视角；后续可以在同一测试方法下增加其他生境样本。

项目的视觉目标是“柔而不平、亮而不曝、暗而有细节、雾而不糊”：保留主光方向、体积塑形、接触阴影和近中远景层次，同时避免硬黑阴影、刺眼高光、过曝、死黑、强烈色偏及过量Bloom。

## 2. 已知工程基线

- 仓库：`freebooz/DivineBeastsWorkspace`；引擎目标：UE5.8。
- 正式新手村地图：`/DBAWorldPack_Village/Maps/L_Village_Start`。
- 现有晨间光雾作者：`Tools/Unreal/World/ReviewVillageMorningEnvironmentUE.py`；既有PIE审核辅助：`Tools/Unreal/World/VillageMorningPIEReviewUE.py`。
- 现有晨间晴朗/薄雾/晨雾是隔离候选，P5人工审核与正式合入尚未批准。正式新手村阴天曝光补偿+1.0 EV是单地图校准值，不得当作全项目常量。
- 现有晨间作者在执行前会检查正式地图哈希、地图几何和未保存包状态。本方案复用其只读基线、SaveAs副本、几何保护和证据结构，不将旧候选值直接推广为新标准。
- 当前Monolith MCP需要在开发PC的UE编辑器运行后才能实际执行；云端仓库只维护脚本、JSON、测试和文档。

## 3. 目标范围

1. 在`/Game/Development/LightingLab/Maps/`创建开发专用关卡副本，不覆盖正式地图。
2. 建立六套可对照的光照候选：柔和日光、清晨暖光、阴天漫射光、黄金时刻、暮光、柔和月夜。
3. 建立五档Directional Light Source Angle阴影对照：0.5357°、1°、2°、3°、4°。这些是实验采样点，不是已批准的全局默认值。
4. 建立三档质量配置：High、Medium、Low；并提供现有GI基线、Lumen Software及有硬件光追条件时的Lumen Hardware对照。
5. 复用五个固定相机视角：入口、桃花林、湖岸、湖畔茅屋、低洼林地；统一FOV与分辨率，记录真实引擎参数回读。
6. 生成基线、作者参数、环境组件、CVar、地图哈希、几何保护和独立进程冷重载证据。
7. 提供无需Unreal模块的离线配置/脚本结构测试，便于云端CI或开发PC执行。

## 4. 非目标与禁止事项

- 不修改正式新手村地图、正式天气定义、正式材质、正式后处理、正式灯光或玩家出生点。
- 不创建新的运行时环境管理器、天气服务、插件、Subsystem、Level Blueprint执行器或第二套光照播放器。
- 不把实验室JSON冒充运行时DataAsset，不将文本占位冒充`.umap`或`.uasset`。
- 不把Lumen、VSM/SMRT、体积雾或硬件光追视为所有平台默认可用；能力需在目标设备读取和验证。
- 不把历史晨间参数、历史GPU数据或概念图当作本轮验收结果。
- 不通过Exposure Bias、全屏Tint、强Bloom或浓雾掩盖材质/光照问题。

## 5. 备选方案与决定

- **方案A（已选）：独立Lighting Lab。** 在开发目录建立隔离关卡副本，复用已有作者与PIE审核链，增加参数化配置、阴影/GI/质量对照和离线测试。优点是正式地图零风险、可复用、可扩展；代价是需要在开发PC首次运行Monolith作者来生成真实地图副本。
- 方案B：继续扩展现有Village Morning隔离地图。实施较快，但实验室会被晨间雾效和旧测试合同绑定，不利于成为跨场景统一标准。
- 方案C：直接修改正式新手村或全局配置。反馈快但无法隔离变量，且与既有P5门禁冲突，因此不采用。

用户已明确选择方案A，并要求将代码直接提交云端仓库，供家中开发PC同步。

## 6. 技术架构

- **配置层：** `Tools/Unreal/World/SSLM_LightingLab_V1.json`，定义六套光照候选、五档阴影角、三档质量、三种GI对照、固定镜头及证据输出位置。
- **UE作者层：** `SSLM_LightingLabUE58.py`，通过既有`ReviewVillageMorningEnvironmentUE.py`复用引擎版本检查、正式地图哈希、地图几何保护、SaveAs、保存及冷重载辅助；作者只对开发副本写入。
- **PIE审核层：** `SSLM_LightingLabPIEReviewUE.py`，复用既有`VillageMorningPIEReviewUE.py`的PIE地图白名单、地表绑定和固定相机；PIE显式应用已验证的Clear天气定义作为中性天气/风场基线，再读取光照候选实际回值。GI对照仅在PIE运行时尝试设置CVar并读取回值，项目INI不被改写。
- **离线测试层：** `Tests/Assets/TestSSLM_LightingLab.py`，校验配置、脚本语法、预设数量、阴影角、质量/GI合同和正式资产保护约束。
- **证据层：** `Saved/Validation/SSLM_LightingLab20261111/`，属于开发PC上的可重建运行证据，不提交虚构的UE二进制资产。

## 7. 验收标准

- 离线测试可在普通Python环境执行，不需要导入Unreal模块。
- UE作者执行前能确认UE5.8、正式地图身份、无PIE、无未保存包、正式地图哈希及副本目标不存在。
- 每份实验地图均由UE5.8 Editor SaveAs生成；保存后正式地图哈希不变，非环境几何/实例/出生点与基线一致。
- 每套预设保存后记录灯光、天空光、雾、局部雾、后处理、地图身份和正式地图哈希。
- 独立Editor进程冷重载后，所有已保存环境字段与证据一致；失败时停止，不自动覆盖或删除现场。
- PIE对GI模式、质量档和CVar执行写入后必须读取回值；不支持的功能明确记为Unsupported，不可伪报通过。
- 人工审核需检查五个固定镜头、软阴影/接触细节、暗部/高光、植被与峭壁体积、雾边界、湖面反射、角色和战斗可读性；概念图和静态配置不替代人工签收。
- 目标设备性能数据分别记录GPU/CPU帧时间、显存、画质档、分辨率、RHI和驱动；不预设未经批准的性能数字。

## 8. 实施交付边界

本次云端实施交付源代码、配置、离线测试、设计/执行计划及目录索引，并将`/Game/Development/LightingLab`加入`Game/Config/DefaultGame.ini`的NeverCook与`Build/Game/ServerPresentationExclusions.json`的AllServers排除。`Game/Content/Development/LightingLab/README.md`只保留目录所有权说明。真实`.umap`副本、UE参数回读、PIE截图、GPU数据和人工签收必须由家中开发PC的UE5.8编辑器与Monolith MCP执行后再登记；本次云端提交不得宣称这些原生验收已完成。
