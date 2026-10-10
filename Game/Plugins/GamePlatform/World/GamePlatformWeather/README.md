# GamePlatformWeather（游戏平台通用天气插件）

2026-10-10｜V1.0 源码实施。所属层：GamePlatform（第一层）；目录：`Game/Plugins/GamePlatform/World/GamePlatformWeather/`。

## 职责与现状

本插件承担跨游戏世界天气契约、调度、单世界服务器权威复制、客户端过渡及环境表现转发；不分配服务器、不实现天气粒子/材质/音效播放器、不改变移动/碰撞/战斗规则。

- `GamePlatformWeatherRuntime`：Runtime（共享运行），双端均可使用；定义`FGamePlatformWeatherState`、`UGamePlatformWeatherPresetDefinition`、`UGamePlatformWeatherWorldSubsystem`、`AGamePlatformWeatherReplicator`。只有权威网络模式可初始化和改写天气，客户端只能观察。
- `GamePlatformWeatherClient`：ClientOnly（客户端／编辑器），`UGamePlatformWeatherClientWorldSubsystem`订阅权威快照、在过渡期间以0.1秒定时器插值并写入`GamePlatformSurface`，同时提交`GamePlatformPresentation`中立VFX/SFX请求。若Provider或实际资产不存在，请求返回`ProviderMissing`，不假定视觉已成功播放。

## 数据与网络

`FGamePlatformWeatherState`保存晴、阴、雨、雪、风、雾、雷暴等类型及七项归一化0..1的环境参数（雨、雪、湿润、积雪、积水、雾、风），另有摄氏温度[-100,100]。非法值包含NaN、Inf、非法枚举、范围越界，拒绝提交，不偷偷变为“晴天”。

`FGamePlatformWeatherQuantizedState`网络将七个强度量化为0..255；温度按0.1摄氏度编码为int16。`FGamePlatformWeatherSnapshot`包含过渡前/后状态、服务器开始秒数、过渡秒数、世界内Revision；新加入客户端可按`AGameStateBase::GetServerWorldTimeSeconds`恢复过渡。不会复制每一粒雨滴或雪花。

`UGamePlatformWeatherWorldSubsystem`是服务器调度的唯一归属；每个真实世界由项目GameMode明确`ActivateWeather`激活一次，随后可调用`SetWeather`（人工指令）或`ConfigureSchedule`和`StartSchedule`（自动周期）。自动调度在服务器定时器执行，权重选择由服务器随机源完成，客户端不生成天气权威结果。世界退出`Deinitialize`取消定时器与委托；Actor/快照有当前世界身份，旧世界回调不得污染新世界。

天气预设定义继承`UGamePlatformDefinitionBase`，由`GamePlatformData`管理真实主资产ID和租约；`ApplyPreset`只消费外部**已完成校验并仍保持有效租约**的定义，绝不私自同步加载或保存裸对象供下一轮使用。

## 三层边界

- `GamePlatformWorld`：世界/区域身份和就绪；不承接雨滴渲染与天气权威调度。
- `GamePlatformSurface`：雪、苔藓、湿润、积水材质参数；天气客户端只改六个动态参数，保留苔藓和积雪高度基础配置，具体项目若还有其他写入方必须建立单一所有者/状态合成，禁止多个系统互相覆盖。
- `GamePlatformPresentation`：统一语义目录/Provider；`GamePlatformVFX`/Niagara负责粒子，`GamePlatformSFX`负责音频；天气插件不复制这些执行器。
- `DBAWorlds`：神兽联盟项目GameMode世界启动及各地图策略；`DBAWorldPack_*`拥有天气预设实例、真实雨雪特效映射、项目地表材质实例；非MOBA世界不依赖竞技模块。

## 项目接线

`ADivineBeastsWorldGameMode::BeginPlay`仅在服务器激活初始天气，默认为晴；`InitialWeather`、`bEnableWeatherSchedule`、`WeatherSchedule`为项目GameMode可编辑的策略。打开客户端和服务端目标已通过Target显式装配；专用服务器不含ClientOnly模块，不应Cook纯天气视觉资产。正式动态地图差异未来交由真实WorldDefinition/内容包资产，不在平台插件硬编码Village、OpenWorld或MainArena。

## 真实资源与验证限制

当前Weather C++并不自带真实Niagara/SFX或Material Function；`GamePlatformSurface`的MPC与母材质也需由UE编辑器生成/编译。天气状态更新成功不等于雨雪可见。交付门禁见`Docs/TestingAndEvidence.md`和`Docs/Implementation/WeatherSystemImplementationPlan_V1.md`。源码构建、UE Automation、Cook、联机和GPU/CPU实测分别记证；未经执行不得宣称通过。
