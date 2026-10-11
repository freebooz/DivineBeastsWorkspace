# 《神兽联盟》飞鸟、蝴蝶和鱼群正式打包与验收记录

日期：2026-10-11  
工作区：DivineBeastsWorkspace（神兽联盟工作空间）  
引擎：Unreal Engine 5.8（虚幻引擎5.8）  
本次验证基线：main，3230708a（本轮读取时）。

## 已验证的工程与架构

- GamePlatformVFX（游戏平台视觉特效插件）承载通用群落行为、Niagara（粒子特效系统）实例与资源生命周期；神兽联盟项目层配置物种及内容，纯表现不向DedicatedServer（专用服务器）复制。
- UBT（虚幻构建工具）完整 Client/Server 目标日志均出现 `Result: Succeeded`。Client日志位于 `Saved/Validation/FoundationM0/d0e0d75f-1563-4132-9e30-1b3245ff6682/Build/Client/UBT.log`，Server日志位于 `Saved/Validation/FoundationM0/2dc35ef2-850a-4020-8b2c-0314f720fcc2/Build/Server/UBT.log`。与当时实际二进制及收据时间对应，服务器收据 `CustomConfig=DedicatedServer`。
- 模拟算法原生回归17项通过，三层依赖和插件组合21项通过，容器审计器11项单元测试通过。
- `Saved/Validation/Fauna/IntegrationReview.json`：蝶与鱼两物种各5种数量，共10组实际PIE GPU回读通过；停栖、惊扰、移动、显式重试、世界清理均有独立证据。
- `Saved/Validation/BirdFlockAudit/FaunaRegression_20261011T150801/IntegrationReview.json`：三种飞鸟15组GPU回归通过。
- `Saved/Validation/Fauna/VisualReviewLatest.json`：真实PIE截图与暂停/恢复技术审核完成，用户最终美术认可仍待确认。

## 真实客户端发布结果

- 入口：`Build/Game/CookFrontEndClient.ps1`，使用 `FrontEndClient（正式前端客户端配置）`，没有跳过真实RHI颜色质量门禁。
- RHI颜色报告：`Game/Saved/Validation/UIColorRHI/Latest.json`；正式发布前核验通过。
- 正式结果：`Saved/Validation/FoundationM0/e2589f05-b9f2-4d0f-995c-5f513cf4adca/Cook-FrontEndClient/result.json`，`Passed`，退出码0。
- IoStore（虚幻资源容器）CSV：同一目录 `ContainerInspection/ClientContainer.csv`，实际2,761条记录，确认前端登录 `L_DBA_FrontEnd`、角色预览 `L_DBA_CharacterStudio` 和新手村 `L_Village_Start` 三图各一份。
- 原有前端资产门禁134项必需资产缺失0；原有飞鸟门禁通过。
- 新增Fauna发布报告：同一目录 `FaunaContainer.json`，`status=passed`，20项必要包存在、缺失0、禁止资产0。

## 真实专用服务器发布结果

- 入口：`Build/Game/CookFoundation.ps1`，`Target=Server`、`CustomConfig=VillageServer`、正式新手村地图、`Pak`。
- 正式结果：`Saved/Validation/FoundationM0/c79c9c09-fb8e-45f5-97b1-b0d449472c06/Cook-Server/result.json`，`Passed`，退出码0。
- 服务端实际IoStore CSV：同一目录 `FaunaContainerInspection/ServerContainer.csv`，647条记录，真实 `L_Village_Start.umap` 有1份。
- Fauna服务端容器报告：`FaunaContainerInspection/ServerFaunaContainer.json`，`status=passed`，禁止资源0。
- 同一目录 `FaunaContainerInspection/ServerPakList.txt`：实际UnrealPak Pak列举成功，不包含Fauna视觉资源路径。

## 交付限制及下一步

**以上真实构建、定向Cook、正式Stage和包级资源隔离已经完成验证，但不等于最终游戏功能和美术整体发布已认可。**

1. 新手村花面、水体和PCG稳定锚点缺少已审核的实际落位数据。不要根据审核关卡的示例坐标修改正式共享地图，不要把客户端特效组件硬引用加入Server共享地图。需要先取得真实锚点及水域约束，再在项目客户端按锚点装配栖息区。
2. 当前真实PIE审核使用简化花床与水池。蓝色材质近景较亮、环境花朵偏示意，须在正式场景真实光照、水面、岸边和距离下人工审美复核。
3. 尚未完成目标PC与移动端帧率、CPU/GPU时间、长时间内存和多群同时显示预算测试。
4. 还需单独进行从客户端登录到Village、跨区域加载/卸载及服务器准入的端到端验证；纯环境生物不得改变战斗权威和任务奖励。

本轮没有将人工审核状态改为通过，没有把测试地图作为正式世界地图，也没有要求覆盖现有其他并行任务资源。
