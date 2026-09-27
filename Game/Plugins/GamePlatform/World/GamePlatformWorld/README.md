# 游戏平台世界插件

本插件维护当前UE世界的逻辑身份、数据租约、区域提供者、世界就绪与引擎流送协调；不分配服务器、不登录、不调用Go、不执行ClientTravel，不实现OpenWorld玩法。

本次交付是**源码与离线测试实现，UE目标／资产／联网未验证**。存在必需未执行项，整体不能标记通过。Session目前没有公开可用快照，因此网络上下文初始化明确返回SessionPrerequisiteMissing，不能将本地开发例外视作SessionWorld完成。

## 启用与最小使用

正式主工程启用GamePlatformWorld。双端GamePlatformWorld依赖Core、Data、Loading、Engine、GameplayTags；GamePlatformWorldEditor执行真实定义与地图校验，依赖DataValidation。运行模块不依赖UI、VFX或Session客户端模块。

先完成正式三目标构建，再用CreateFoundationAssets.py生成原有地图，运行CreateWorldAssets.py生成真实世界与区域定义。显式FoundationWorld启动使用项目私有DBAFoundationWorldBootstrap；未启用时不自动加载测试定义。服务器还必须显式选择开发角色。步骤见[配置与运行](Docs/ConfigurationAndRun.md)。

公开访问由IGamePlatformWorldService::Get(UWorld&)取得。申请定义后读取GetReadiness；RegisterRegionProvider必须使用当前代次、当前世界弱对象和真实区域定义身份。Loading通过GamePlatformWorldServices::CreateReadinessTask创建独占任务，由实例组合根登记，不在模块启动注册全局工厂。

## 审查导航

1. [架构与生命周期](Docs/Architecture.md)
2. [实际公开接口](Docs/API.md)
3. [上下文字段来源](Docs/WorldContext.md)
4. [区域模型](Docs/RegionModel.md)
5. [流送与就绪](Docs/StreamingAndReadiness.md)
6. [会话与加载集成](Docs/SessionAndLoadingIntegration.md)
7. [网络权威](Docs/NetworkingAuthority.md)
8. [配置与运行](Docs/ConfigurationAndRun.md)
9. [测试证据](Docs/TestingAndEvidence.md)
10. [故障定位](Docs/Troubleshooting.md)
11. [迁移交接](Docs/MigrationAndHandover.md)
12. [人工审查记录](Docs/ManualReview.md)

