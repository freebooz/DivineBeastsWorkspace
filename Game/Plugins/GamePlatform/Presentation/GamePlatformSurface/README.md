# GamePlatformSurface（游戏平台通用环境表面材质插件）

正式位置：`Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/`。

本插件提供跨游戏复用的环境表面材质基础能力：统一客户端环境表面状态、每世界 MPC（Material Parameter Collection，材质参数集合）桥接，以及雪、苔藓、湿润、积水等通用材质资产合同。它不负责《神兽联盟》专属纹理、天气权威、PCG 生成、Niagara 特效、水体模拟、角色材质或服务器玩法判定。

## 模块

- `GamePlatformSurfaceClient（环境表面客户端模块）`：`ClientOnly（仅客户端）`，只允许 Client／Editor 目标；提供状态、服务、MPC桥接和 Blueprint（蓝图）入口。
- `GamePlatformSurfaceEditor（环境表面编辑器模块）`：`Editor（编辑器）`；提供真实 MPC 生成／校验 Commandlet（命令行工具）、资产合同与自动化测试。
- 不创建 Runtime／Server 空模块。Dedicated Server（专用服务器）不链接客户端模块，也不应 Cook（烘焙）纯表面表现资产。

## 已实现运行时能力

`FGamePlatformSurfaceEnvironmentState（环境表面状态）`统一包含：`GlobalWetness（湿润度）`、`GlobalSnowAmount（积雪量）`、`GlobalSnowHeightCm（积雪参考高度，厘米）`、`GlobalMossInfluence（苔藓影响）`、`GlobalPuddleAmount（积水量）`、`RainIntensity（降雨表现强度）`、`SnowIntensity（降雪表现强度）`、`TemperatureCelsius（环境表现温度）`。

状态更新无 Tick（逐帧更新）：非法 NaN／Inf 整包拒绝，合法值先裁剪，同值去重，再一次性写入当前世界的 MPC Instance（材质参数集合实例）。

标准核心 MPC 路径为：`/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal`。该 `.uasset（虚幻资产）`只能通过 Unreal Editor／Commandlet 真实创建，源码不会写文本占位。

## 通用材质资产合同

一期稳定命名包括 `M_GP_Surface_Master（通用表面母材质）`、`M_GP_Surface_Lite（简化母材质）`、`MF_GP_SlopeMask（坡度遮罩）`、`MF_GP_HeightMask（高度遮罩）`、`MF_GP_WorldNoise（世界空间噪声）`、`MF_GP_SnowLayer（积雪层）`、`MF_GP_MossLayer（苔藓层）`、`MF_GP_WetnessLayer（湿润层）`、`MF_GP_PuddleLayer（积水层）`。

这些 Material／Material Function（材质／材质函数）必须由 Unreal Material Editor（虚幻材质编辑器）真实制作、编译和保存；当前源码只声明稳定路径、参数和职责，不伪造二进制资产。

## 神兽联盟项目边界

`GamePlatformSurface`只拥有平台通用机制与模板。桃林、新手村、瀑布湿岩、雪山、国风建筑等项目纹理和 Material Instance（材质实例）归对应 `DBAWorldPack_*（神兽联盟世界内容包）`；项目世界定义与组合仍归 `DBAWorlds（神兽联盟项目世界插件）`。

PCG负责“在哪里生成什么”；Surface负责“表面如何表现”；VFX负责雨滴、雪花、水雾、飞溅等动态效果；Surface负责湿润、积雪、苔藓、积水等表面状态。

详细说明见 `Docs/Architecture.md`、`Docs/组件清单与使用说明.md`、`Docs/AssetAuthoring.md`、`Docs/PerformanceAndServer.md`、`Docs/TestingAndEvidence.md`、`Docs/ManualReview.md`。


## 2026-09-30 设计审查修复

本次资源/生命周期与行为合同见 [设计修复说明](Docs/DesignRemediation-2026-09-30.md)。源码及新增回归不等于UE运行、真实资产或Cook验收；准确执行证据由任务修复报告记录。


2026-10-09本插件源码整改、中文API/所有权说明和待UE验收边界见 [本轮源码说明](Docs/AuditRemediation-2026-10-09.md)。
